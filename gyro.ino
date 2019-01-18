/**
 * MPU6050/gyroscope specific code. Initialize, helpers to do angular math.
 */
 
// Crank length, in meters
#define CRANK_RADIUS 0.175

/** 
 *  Calibrate and initialize the gyroscope
 */
void gyroSetup() {  
  // "gyro" is defined in main, Arduino implicitly smashes these files together
  // to compile, so it's in scope.
  //
  // 2000DPS is a max speed of 2000 degrees per second (5.5 RPS or 333 RPM).
  // The next step down for scale is 1000, or 167 RPM. Actually achievable/exceedable 
  // doing speed work, but very rarely, and presumably at some power savings.
  // The range is multiples of gravity for the accelerometer, and while 2g is fairly
  // low, we don't need that measurement anyway. We just need gyro for foot speed.
  while(!gyro.begin(MPU6050_SCALE_1000DPS, MPU6050_RANGE_2G)){
#ifdef DEBUG
    Serial.println("Could not find the MPU6050 sensor, check wiring!");
#endif
    delay(500);
  }
  // Calibrate gyroscope. The calibration must be at rest.
  gyro.calibrateGyro();
  // Set threshold sensivty. This is a multiple of the raw reading that must be 
  // exceeded to be considered non-zero. 3 is just what was used in examples by
  // the library author, I have no other reason to consider it a good default...
  gyro.setThreshold(3);

#ifdef DEBUG
  checkSettings();
#endif
}

/**
 * This doesn't do anything but echo applied setting on the MPU.
 */
void checkSettings() {
  Serial.println();
  Serial.print(" * Sleep Mode:        ");
  Serial.println(gyro.getSleepEnabled() ? "Enabled" : "Disabled");
  
  Serial.print(" * Clock Source:      ");
  switch(gyro.getClockSource()){
    case MPU6050_CLOCK_KEEP_RESET:     Serial.println("Stops the clock and keeps the timing generator in reset"); break;
    case MPU6050_CLOCK_EXTERNAL_19MHZ: Serial.println("PLL with external 19.2MHz reference"); break;
    case MPU6050_CLOCK_EXTERNAL_32KHZ: Serial.println("PLL with external 32.768kHz reference"); break;
    case MPU6050_CLOCK_PLL_ZGYRO:      Serial.println("PLL with Z axis gyroscope reference"); break;
    case MPU6050_CLOCK_PLL_YGYRO:      Serial.println("PLL with Y axis gyroscope reference"); break;
    case MPU6050_CLOCK_PLL_XGYRO:      Serial.println("PLL with X axis gyroscope reference"); break;
    case MPU6050_CLOCK_INTERNAL_8MHZ:  Serial.println("Internal 8MHz oscillator"); break;
  }
  
  Serial.print(" * Gyroscope:         ");
  switch(gyro.getScale()){
    case MPU6050_SCALE_2000DPS:        Serial.println("2000 degrees/s"); break;
    case MPU6050_SCALE_1000DPS:        Serial.println("1000 degrees/s"); break;
    case MPU6050_SCALE_500DPS:         Serial.println("500 degrees/s"); break;
    case MPU6050_SCALE_250DPS:         Serial.println("250 degrees/s"); break;
  } 
  
  Serial.print(" * Gyroscope offsets: ");
  Serial.print(gyro.getGyroOffsetX());
  Serial.print(" / ");
  Serial.print(gyro.getGyroOffsetY());
  Serial.print(" / ");
  Serial.println(gyro.getGyroOffsetZ());

  Serial.println("Gyroscope setup complete.");
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
double getNormalAvgVelocity(double lastAvg) {  
  const static double WEIGHT = 0.90;
  double newData = 0;

  // Request new data from the MPU
  Vector normGyro = gyro.readNormalizeGyro();

  // The axis we're interested in depends on which way we end up packing this thing
  // into a case on the crankarm. Right now it's sideways, so 'z'.
  newData = normGyro.ZAxis;

  // Return a rolling average, including the last reading.
  // e.g. if weight is 0.90, it's 10% what it used to be, 90% this new reading.
  double newavg = (newData * WEIGHT) + (lastAvg * (1 - WEIGHT));

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
double getCircularVelocity(double normAvgRotate) {
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
 *  Returns an double of cadence, rotations/minute.
 */
double getCadence(double normAvgRotate) {
  // Cadence is the normalized angular velocity, times 60/360, which 
  // converts from deg/s to rotations/min. x * (60/360) = x / 6.
  return normAvgRotate / 6;
}
