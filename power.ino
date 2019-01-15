#include <Wire.h>
#include <bluefruit.h>

#include "MPU6050.h"
#include "HX711.h"

#define DEBUG

#define SAMPLES_PER_SECOND 2
#define LED_PIN 33
#define CAD_UPDATE_MILLIS 2000

MPU6050 gyro;
HX711 load;
bool led_on = false;

void setup() {
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb
          
  bleSetup();
  gyroSetup();
  loadSetup();
#ifdef DEBUG
  Serial.println("All setup complete.");
#endif
}

void loop() {  
  // Vars for tracking cadence
  static double normalAvgVelocity = 0;
  static double metersPerSecond = 0;
  static double cadence = 0;
  static short totalCrankRevs = 0;
  static long lastCadUpdate = millis();
  // Vars for force
  static double avgForce = 0;
  // And together, is power
  static double power = 0;

  // Get the average velocity from the gyroscope, in m/s.
  normalAvgVelocity = getNormalAvgVelocity(normalAvgVelocity);
  metersPerSecond = getCircularVelocity(normalAvgVelocity);
  // Not necessary for power, but a good sanity check calculation 
  // to do development and get going and easy added value.
  cadence = getCadence(normalAvgVelocity);
  
  // Now get force from the load cell
  avgForce = getAvgForce(avgForce);

  // That's all the ingredients, now we can find the power.
  power = calcPower(metersPerSecond, avgForce);

#ifdef DEBUG
  // Just print these values to the serial, something easy
  // to read, not BLE packed stuff.
  Serial.write('c');
  Serial.println(cadence);
  Serial.write('p');
  Serial.println(power);
#endif // DEBUG

  if (Bluefruit.connected()) {      
    // Note: We use .notify instead of .write!
    // If it is connected but CCCD is not enabled
    // the characteristic's value is still updated although notification is not sent
    blePublishPower(short(power));
    // For resolution and rounding errors, we need to make the cadence update less
    // regularly. Like every few seconds, because it's just total crank revs and 
    // published as an int.
    long timeSinceLastUpdate = millis() - lastCadUpdate;
    if (timeSinceLastUpdate > CAD_UPDATE_MILLIS) {
      // Time for an update. The time since last update, as published, is actually at
      // a resolution of 1/1024 seconds, per the spec. BLE will convert, just send
      // how long it's been, in millis.
      // As for totalCrankRevs, a calculation closer to the "source" would be 
      // preferable, as we've already done quite a bit of math on cadence. Some of
      // that math has been a rolling average. For simplicity right now, assume
      // the current cadence has been constant for the duration.
      totalCrankRevs += (cadence / 60) * (timeSinceLastUpdate / 1000);
      blePublishCadence(totalCrankRevs, timeSinceLastUpdate);
      // Reset the latest update to now.
      lastCadUpdate = millis();
    }
  }

  delay(1000 / SAMPLES_PER_SECOND);
}

/**
 * Given the footspeed (angular velocity) and force, power falls out.
 *
 * Returns the power, in watts. Force and distance over time.
 */
double calcPower(double footSpeed, double force) {
  return (force * footSpeed);
}
