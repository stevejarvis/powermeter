/**
 * MPU6050/gyroscope specific code. Initialize, helpers to do angular math.
 * 
 * TODO big improvements I think to use the sleep mode and interrupts provided 
 * by this MPU.
 */
 
// Crank length, in meters
#define CRANK_RADIUS 0.1725
#define CALIBRATION_SAMPLES 40

/** 
 *  Calibrate and initialize the gyroscope
 */
void gyroSetup() {  
  // "gyro" is defined in main, Arduino implicitly smashes these files together
  // to compile, so it's in scope.
  
  gyro.initialize();

#ifdef DEBUG
  // verify connection
  Serial.println(F("Testing device connections..."));
  Serial.println(gyro.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

  // Calibrate the gyro
  gyro.setZGyroOffset(0);
  Serial.print("Starting gyroscope offset: "); Serial.println(gyro.getZGyroOffset());
#endif 
  float sumZ = 0;
  int16_t maxSample = -32768;
  int16_t minSample = 32767;
  // Read n-samples
  for (uint8_t i = 0; i < CALIBRATION_SAMPLES; ++i) {
    delay(5);
    int16_t reading = gyro.getRotationZ();
    if (reading > maxSample) maxSample = reading;
    if (reading < minSample) minSample = reading;
    sumZ += reading;
  }

  // Throw out the two outliers
  sumZ -= minSample;
  sumZ -= maxSample;

  // Two fewer than the calibration samples because we took out two outliers.
  float deltaZ = sumZ / (CALIBRATION_SAMPLES - 2);
#ifdef DEBUG
  Serial.print("Discounting max and min samples: "); Serial.print(maxSample); Serial.print(" "); Serial.println(minSample);
  Serial.print("Gyro calculated offset: "); Serial.println(deltaZ);
#endif
  // Set that calibration
  gyro.setZGyroOffset(-1 * deltaZ);
  
#ifdef DEBUG
  dumpSettings();
#endif
}

/**
 * This doesn't do anything but echo applied setting on the MPU.
 */
void dumpSettings() {
  Serial.println();
  Serial.print(" * Gyroscope Sleep Mode: ");
  Serial.println(gyro.getSleepEnabled() ? "Enabled" : "Disabled");
  
  Serial.print(" * Gyroscope offset:     ");
  Serial.println(gyro.getZGyroOffset());
}

/**
 * Gets a normalized averaged rotational velocity calculation. The MPU6050 library supports a 
 * normalized gyroscope reading, which trims off outliers and scales the values to deg/s. 
 * 
 * An exponential average is applied to further smooth data, with weight of WEIGHT. I don't 
 * love this, becaues it means no value is every entirely discarded, but exponential decay
 * probably makes it effectively the same. Maybe something to revisit.
 * 
 * Returns a value for foot speed, in degrees/second.
 */
int16_t getNormalAvgVelocity(int16_t lastAvg) {  
  const static double WEIGHT = 0.90;
  // At +/- 250 degrees/s, the LSB/deg/s is 131. Per the mpu6050 spec.
  const static int16_t SENSITIVITY = 131;

  // Request new data from the MPU. The orientation obviously dictates
  // which x/y/z value we're interested in, but right now Z.
  int16_t rotz = gyro.getRotationZ() / SENSITIVITY;
  
  // Return a rolling average, including the last reading.
  // e.g. if weight is 0.90, it's 10% what it used to be, 90% this new reading.
  int16_t newavg = (rotz * WEIGHT) + (lastAvg * (1 - WEIGHT));

  // Return the absolute value. Cause who knows if the chip is just backwards.
  return abs(newavg);
}

/**
 * Provide the average rate, in degrees/second.
 * 
 * Returns the circular velocity of the rider's foot. Takes in the crank's averaged rotational
 * velocity, converts it to radians, multiplies by the crank radius, and returns the converted 
 * value.
 * 
 * Value returned is in meters/second
 */
float getCircularVelocity(int16_t normAvgRotate) {
  // 2 * PI * radians = 360 degrees  -- by definition
  // normAvgRotate degrees/second * (PI / 180) rad/degree = rad/second
  // (rad/sec) / (rad/circumference) = (rad/sec) / 2 * PI = ratio of a circumference traveled, still per second
  // 2 * PI * CRANK_RADIUS = circumference  -- by definition, that's a circle
  // ratio of circumference traveled * circumference = meters/second

  // It all comes out to:
  // m/s = ((normAvgRotate * (PI / 180)) / (2 * PI)) * (2 * PI * CRANK_RADIUS);
  // And simplifies to:
  return (normAvgRotate * PI * CRANK_RADIUS) / 180;
}

/**
 *  Provide angular velocity, degrees/sec.
 *  
 *  Returns a new cadence measurement.
 *  
 *  Note this isn't necessary for power measurement, but it's a gimme addon 
 *  given what we already have and useful for the athlete.
 *  
 *  Returns an int16 of cadence, rotations/minute.
 */
int16_t getCadence(int16_t normAvgRotate) {
  // Cadence is the normalized angular velocity, times 60/360, which 
  // converts from deg/s to rotations/min. x * (60/360) = x / 6.
  return normAvgRotate / 6;
}
