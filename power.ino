#include <Wire.h>
#include <bluefruit.h>

#include "MPU6050.h"
#include "HX711.h"

//#define DEBUG
//#define BLE_LOGGING
//#define CALIBRATE

// Crank length, in meters
#define CRANK_RADIUS 0.1750

// Hooked up the wires backwards apparently, force is negated.
// If it isn't, just set to 1.
#define HOOKEDUPLOADBACKWARDS -1

// The pause for the loop, and based on testing the actual
// calls overhead take about 20ms themselves E.g. at 50ms delay, 
// that means a 50ms delay, plus 20ms to poll. So 70ms per loop, 
// will get ~14 samples/second.
#define LOOP_DELAY 50

// Min pause How often to crunch numbers and publish an update (millis)
// NOTE If this value is less than the time it takes for one crank
// rotation, we will not report a crank revolution. In other words,
// if the value is 1000 (1 second), cadence under 60 RPM won't register.
#define MIN_UPDATE_FREQ 1500

// NOTE LED is automatically lit solid when connected,
// we don't currently change it, default Feather behavior
// is nice. 
// TODO Though not optimal for power, not sure how much it takes.
#define LED_PIN 33

MPU6050 gyro;
HX711 load;


void setup() {
  Wire.begin();
  Serial.begin(115200);

  // Setup, calibrate our other components
  gyroSetup();
  loadSetup();
  bleSetup();

#ifdef DEBUG
  // The F Macro stores strings in flash (program space) instead of RAM
  Serial.println(F("All setup complete."));
#endif
}

void loop() {
  // These aren't actually the range of a double, but
  // they should easily bookend force readings.
  static const float MIN_DOUBLE = -100000.f;
  static const float MAX_DOUBLE = 100000.f;

  // Vars for polling footspeed
  static float dps = 0.f;
  static float avgDps = 0.f;
  // Cadence is calculated by increasing total revolutions.
  // TODO it's possible this rolls over, about 12 hours at 90RPM for 16 bit unsigned.
  static uint16_t totalCrankRevs = 0;
  // Vars for force
  static double force = 0.f;
  static double avgForce = 0.f;
  // Track the max and min force per update, and exclude them.
  static double maxForce = MIN_DOUBLE;
  static double minForce = MAX_DOUBLE;
  // We only publish every once-in-a-while.
  static long lastUpdate = millis();
  // To find the average values to use, count the num of samples
  // between updates.
  static int16_t numPolls = 0;

  // During every loop, we just want to get samples to calculate
  // one power/cadence update every interval we update the central.

  // Degrees per second
  dps = getNormalAvgVelocity(dps);
  avgDps += dps;

  // Now get force from the load cell.
  force = getAvgForce(force);
  // We wanna throw out the max and min.
  if (force > maxForce) {
    maxForce = force;
  }
  if (force < minForce) {
    minForce = force;
  }
  avgForce += force;

  numPolls += 1;

#ifdef BLE_LOGGING
  blePublishLog("F%.1f|%.1f %d", force, dps, numPolls);
#endif

#ifdef DEBUG
  // Just print these values to the serial, something easy to read.
  Serial.print(F("Force: ")); Serial.println(force);
  Serial.print(F("DPS:   ")); Serial.println(dps);
#endif  // DEBUG

  if (Bluefruit.connected()) {
    // We have a central connected
    long timeNow = millis();
    long timeSinceLastUpdate = timeNow - lastUpdate;
    // Must ensure there are more than 2 polls, because we're tossing the high and low.
    // Check to see if the updateTime fun determines the cranks are cranking (in which)
    // case it'll aim to update once per revolution. If that's the case,
    // increment crank revs.
    bool pedaling = false;
    if (timeSinceLastUpdate > updateTime(dps, &pedaling) && numPolls > 2) {
      // Find the actual averages over the polling period.
      avgDps = avgDps / numPolls;
      // Subtract 2 from the numPolls for force because we're removing the high and
      // low here.
      avgForce = avgForce - minForce - maxForce;
      avgForce = avgForce / (numPolls - 2);

      // Convert dps to mps
      float mps = getCircularVelocity(avgDps);

      // That's all the ingredients, now we can find the power.
      int16_t power = calcPower(mps, avgForce);

#ifdef DEBUG
  // Just print these values to the serial, something easy to read.
  Serial.print(F("Pwr: ")); Serial.println(power);
#endif  // DEBUG

      // The time since last update, as published, is actually at
      // a resolution of 1/1024 seconds, per the spec. BLE will convert, just send
      // the time, in millis.
      if (pedaling) {
        totalCrankRevs += 1;
      }
      blePublishPower(power, totalCrankRevs, timeNow);

#ifdef BLE_LOGGING
      // It's not even useful for sending cadence to the computer, ironically.
      int16_t cadence = getCadence(avgDps);
      // Log chars over BLE, for some insight when not wired to a
      // laptop. Need to keep total ASCII to 20 chars or less.
      blePublishLog("B%.1f %.1f %d", avgForce, mps, power);
      blePublishLog("%d: %d polls", millis() / 1000, numPolls);
#endif

      // Reset the latest update to now.
      lastUpdate = millis();
      // Let the averages from this polling period just carry over.
      numPolls = 1;
      maxForce = MIN_DOUBLE;
      minForce = MAX_DOUBLE;
    }
  }

  delay(LOOP_DELAY);
}

/**
 * Figure out how long to sleep in the loops. We'd like to tie the update interval
 * to the cadence, but if they stop pedaling need some minimum.
 *
 * Return update interval, in milliseconds.
 */
float updateTime(float dps, bool *pedaling) {
  // So knowing the dps, how long for 360 degrees?
  float del = min(MIN_UPDATE_FREQ, 1000.f * (360.f / dps));
  if (del < MIN_UPDATE_FREQ) {
      // Let the caller know we didn't just hit the max pause,
      // the cranks are spinning.
      *pedaling = true;
  }
  return del;
}

/**
 * Given the footspeed (angular velocity) and force, power falls out.
 *
 * Returns the power, in watts. Force and distance over time.
 */
int16_t calcPower(double footSpeed, double force) {
  // Multiply it all by 2, because we only have the sensor on 1/2 the cranks.
  return (2 * force * footSpeed);
}
