/**
 * Force and load cell-specific code and helpers. HX711 chip.
 */
#include "HX711.h"

// This offset value is obtained by calibrating the scale with known 
// weights, currently manually with a separate sketch. 

// It's 'm' as in:
// 'y = m*x + b' 
// where 'y' is kilograms (our desired answer, technically a 
// force because it's a measure of weight, not mass, here), 'x' is 
// the raw reading of the load cell, and 'b' is the tare offset. So
// this multiplier is the scale needed to translate raw readings to
// units of kilograms.
#define HX711_MULT 2280.f

// Call tare to average this many readings to get going.
#define NUM_TARE_CALLS 50
// How many raw readings to take each sample.
#define NUM_RAW_SAMPLES 7

// Beetle pins we're using.
#define EXCIT_POS A0
#define EXCIT_NEG A1

void loadSetup() {
  // 'load' is declared in power.ini
  load.begin(EXCIT_POS, EXCIT_NEG);
  // Set the scale for the multiplier to get grams.
  load.set_scale(HX711_MULT);
  // Lots of calls to get load on startup, this is the offset
  // that will be used throughout. Make sure no weight on the
  // pedal at startup, obviously.
  load.tare(NUM_TARE_CALLS);

#ifdef DEBUG
  showConfigs();
#endif
}

void showConfigs(void) {
  Serial.println();
  Serial.print(" * Load offset:       ");
  Serial.println(load.get_offset());
  
  Serial.print(" * Load multiplier:   ");
  Serial.println(load.get_scale());

  Serial.print("Power meter calibrated.");
}

/**
 * Get the current force from the load cell. Returns an exponentially
 * rolling average, in kilograms.
 */
double getAvgForce(double lastAvg) {
  const static double WEIGHT = 0.90;
  static double currentData = 0;

  // Power the load cell up and down each run, hopefully saving power
  // but I'm not really sure.
  load.power_up();
  currentData = load.get_units(NUM_RAW_SAMPLES);
  load.power_down();

  // Return a rolling average, including the last avg readings.
  // e.g. if weight is 0.90, it's 10% what it used to be, 90% this new reading.
  return (currentData * WEIGHT) + (lastAvg * (1 - WEIGHT));
}
