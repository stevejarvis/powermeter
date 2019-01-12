#include <Wire.h>
#include "MPU6050.h"
#include "HX711.h"

#define CADENCE
#define DEBUG

#define SAMPLES_PER_SECOND 4
#define LED_PIN 13

MPU6050 gyro;
HX711 load;
bool led_on = false;

void setup() {
  Serial.begin(115200);

  gyroSetup();
  loadSetup();
#ifdef DEBUG
  Serial.println("All setup complete.");
#endif
}

void loop() {
  digitalWrite(LED_PIN, led_on);
  
#ifdef CADENCE
  static double cadence = 0;
#endif
  static double power = 0;
  static double normalAvgVelocity = 0;
  static double metersPerSecond = 0;
  static double avgForce = 0;

  // Get the average velocity from the gyroscope, in m/s.
  normalAvgVelocity = getNormalAvgVelocity(normalAvgVelocity);
  metersPerSecond = getCircularVelocity(normalAvgVelocity);
#ifdef CADENCE
  // Not necessary for power, but a good sanity check calculation 
  // to do development and get going.
  cadence = getCadence(normalAvgVelocity);
#endif

  // Now get force from the load cell
  avgForce = getAvgForce(avgForce);

  // That's all the ingredients, now we can find the power.
  power = calcPower(metersPerSecond, avgForce);

#ifdef DEBUG
#ifdef CADENCE
  Serial.write('c');
  Serial.println(cadence);
#endif // CADENCE
  Serial.write('p');
  Serial.println(power);
#endif // DEBUG

  blePublishPower(power);

  led_on = !led_on;
  digitalWrite(LED_PIN, led_on);

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
