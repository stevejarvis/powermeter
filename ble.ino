/**
 * This file keeps the BLE helpers, to send the data over bluetooth
 * to the bike computer. Or any other receiver, if dev/debug.
 *
 * For the Adafruit BLE lib, see:
 * https://github.com/adafruit/Adafruit_nRF52_Arduino/tree/bd0747473242d5d7c58ebc67ab0aa5098db56547/libraries/Bluefruit52Lib
 */

#include <stdarg.h>

// Service and character constants at:
// https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/bd0747473242d5d7c58ebc67ab0aa5098db56547/libraries/Bluefruit52Lib/src/BLEUuid.h
/* Pwr Service Definitions
 * Cycling Power Service:      0x1818
 * Power Measurement Char:     0x2A63
 * Cycling Power Feature Char: 0x2A65
 * Sensor Location Char:       0x2A5D
 */
BLEService        pwrService  = BLEService(UUID16_SVC_CYCLING_POWER);
BLECharacteristic pwrMeasChar = BLECharacteristic(UUID16_CHR_CYCLING_POWER_MEASUREMENT);
BLECharacteristic pwrFeatChar = BLECharacteristic(UUID16_CHR_CYCLING_POWER_FEATURE);
BLECharacteristic pwrLocChar  = BLECharacteristic(UUID16_CHR_SENSOR_LOCATION);

/*
 * A made up service to help development.
 */
BLEService        logService = BLEService(0xface);
BLECharacteristic logChar    = BLECharacteristic(0x1234);

BLEDis bledis;    // DIS (Device Information Service) helper class instance
BLEBas blebas;    // BAS (Battery Service) helper class instance

void bleSetup() {
  Bluefruit.begin();
  Bluefruit.setName(DEV_NAME);

  // Set the connect/disconnect callback handlers
  Bluefruit.setConnectCallback(connectCallback);
  Bluefruit.setDisconnectCallback(disconnectCallback);

  // off Blue LED for lowest power consumption
  Bluefruit.autoConnLed(false);

  // Configure and Start the Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();

  // Start the BLE Battery Service
  blebas.begin();

  // Setup the Heart Rate Monitor service using
  // BLEService and BLECharacteristic classes
  setupPwr();

#ifdef BLE_LOGGING
  setupBleLogger();
#endif

  // Setup the advertising packet(s)
  startAdv();

#ifdef DEBUG
  Serial.println("BLE module configured and advertising.");
#endif
}

void startAdv(void) {
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  // Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(-8);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(pwrService);
#ifdef BLE_LOGGING
  Bluefruit.Advertising.addService(logService);
#endif
  Bluefruit.Advertising.addName();

  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   *
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

/*
 * Set up the power service
 */
void setupPwr(void) {
  // Configure supported characteristics:
  pwrService.begin();

  // Note: You must call .begin() on the BLEService before calling .begin() on
  // any characteristic(s) within that service definition.. Calling .begin() on
  // a BLECharacteristic will cause it to be added to the last BLEService that
  // was 'begin()'ed!

  // Has to have notify enabled.
  // Power measurement. This is the characteristic that really matters. See:
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.cycling_power_measurement.xml
  pwrMeasChar.setProperties(CHR_PROPS_NOTIFY);
  // First param is the read permission, second is write.
  pwrMeasChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  // 4 total bytes, 2 16-bit values
  pwrMeasChar.setFixedLen(4);
  // Optionally capture Client Characteristic Config Descriptor updates
  pwrMeasChar.setCccdWriteCallback(cccdCallback);
  pwrMeasChar.begin();

  /*
   * The other two characterstics aren't updated over time, they're static info
   * relaying what's available in our service and characteristics.
   */

  // Characteristic for power feature. Has to be readable, but not necessarily
  // notify. 32 bit value of what's supported, see
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.cycling_power_feature.xml
  pwrFeatChar.setProperties(CHR_PROPS_READ);
  pwrFeatChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  // 1 32-bit value
  pwrFeatChar.setFixedLen(4);
  pwrFeatChar.begin();
  // No extras for now, write 0.
  pwrFeatChar.write32(0);

  // Characteristic for sensor location. Has to be readable, but not necessarily
  // notify. See:
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.sensor_location.xml
  pwrLocChar.setProperties(CHR_PROPS_READ);
  pwrLocChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  pwrLocChar.setFixedLen(1);
  pwrLocChar.begin();
  // Set location to "left crank"
  pwrLocChar.write8(5);
}

/*
 * This service exists only to publish logs over BLE.
 */
#ifdef BLE_LOGGING
void setupBleLogger() {
  logService.begin();

  // Has nothing to do with any spec.
  logChar.setProperties(CHR_PROPS_NOTIFY);
  // First param is the read permission, second is write.
  logChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  // Payload is quite limited in BLE, so come up with good logging shorthand.
  logChar.setMaxLen(20);
  logChar.begin();
}
#endif

/*
 * Publish the instantaneous power measurement.
 */
void blePublishPower(int16_t instantPwr, uint16_t crankRevs, long millisLast) {
  // Power measure characteristic
  /**
   * Fields
   *
   * Flags (16 bits):
   *   b0 pedal power balance present
   *   b1 pedal power balance reference
   *   b2 accumulated torque present
   *   b3 accumulated torque source
   *   b4 wheel revolution data present
   *   b5 crank revolution data present
   *   b6 extreme force magnitudes present
   *   b7 extreme torque magnitudes present
   *   b8 extreme angles present
   *   b9 top dead spot angle present
   *   b10 bottom dead spot angle present
   *   b11 accumulated energy present
   *   b12 offset compenstation indicator
   *   b13 reserved
   *
   * Instananous Power:
   *   16 bits signed int
   *   
   * Cumulative Crank Revolutions:
   *   16 bits signed int
   *
   * Last Crank Event Time
   *   16 bits signed int
   */
  // Flag cadence. Put most-significant octet first, it'll flip later.
  uint16_t flag = 0b0010000000000000;
  //uint16_t flag = 0b0000000000000000;

  // All data in characteristics goes least-significant octet first.
  // Split them up into 8-bit ints. LSO ends up first in array.
  uint8_t flags[2];
  uint16ToLso(flag, flags);
  uint8_t pwr[2];
  uint16ToLso(instantPwr, pwr);

  // Cadnce last event time is time of last event, in 1/1024 second resolution
  uint16_t lastEventTime = uint16_t(millisLast / 1000.f * 1024.f) % 65536;
  // Split the 16-bit ints into 8 bits, LSO is first in array.
  uint8_t cranks[2];
  uint16ToLso(crankRevs, cranks);
  uint8_t lastTime[2];
  uint16ToLso(lastEventTime, lastTime);

  // All fields are 16-bit values, split into two 8-bit values.
  uint8_t pwrdata[8] = { flags[0], flags[1], pwr[0], pwr[1], cranks[0], cranks[1], lastTime[0], lastTime[1] };
  //uint8_t pwrdata[4] = { flags[0], flags[1], pwr[0], pwr[1] };

  //Log.notice("BLE published flags: %X %X pwr: %X %X cranks: %X %X last time: %X %X\n", 
  //           pwrdata[0], pwrdata[1], pwrdata[2], pwrdata[3], pwrdata[4], pwrdata[5], pwrdata[6], pwrdata[7]);

  if (pwrMeasChar.notify(pwrdata, sizeof(pwrdata))) {
#ifdef DEBUG
    Serial.print("Power measurement updated to: ");
    Serial.println(instantPwr);
  } else {
    Serial.println("ERROR: Power notify not set in the CCCD or not connected!");
#endif
  }
}

void blePublishBatt(uint8_t battPercent) {
  blebas.write(battPercent);
#ifdef DEBUG
  Serial.printf("Updated battery percentage to %d", battPercent);
#endif
}

/*
 * Publish a tiny little log message over BLE. Pass a null-terminated
 * char*, in 20 chars or less (counting the null).
 */
#ifdef BLE_LOGGING
void blePublishLog(const char* fmt, ...) {
  static const short MAX = 20;  // 19 chars plus the null terminator
  static char msg[MAX];

  va_list args;
  va_start(args, fmt);
  int numBytes = vsprintf(msg, fmt, args);
  va_end(args);

  if (numBytes < 0) {
    Serial.println("Failed to write BLE log to buffer");
  } else if (numBytes > MAX) {
    Serial.printf("Too many bytes written (%d), overflowed the msg buffer.\n", numBytes);
    Serial.printf("Original message: %s\n", msg);
  } else {
    bool ret = logChar.notify(msg, numBytes);
    if (ret) {
      Serial.printf("Sent log %d byte message: %s\n", ret, msg);
    } else {
      Serial.println("Failed to publish log message over BLE.");
    }
  }
}
#endif

void connectCallback(uint16_t connHandle) {
  char centralName[32] = { 0 };
  Bluefruit.Gap.getPeerName(connHandle, centralName, sizeof(centralName));

  // Light up our 'connected' LED
  digitalWrite(LED_PIN, 1);

#ifdef DEBUG
  Serial.print("Connected to ");
  Serial.println(centralName);
#endif
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 * https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/cores/nRF5/nordic/softdevice/s140_nrf52_6.1.1_API/include/ble_hci.h
 */
void disconnectCallback(uint16_t connHandle, uint8_t reason) {
  (void) connHandle;
  (void) reason;

  digitalWrite(LED_PIN, 0);

#ifdef DEBUG
      Serial.println("Disconnected");
      Serial.println("Advertising!");
#endif
}

void cccdCallback(BLECharacteristic& chr, uint16_t cccdValue) {
#ifdef DEBUG
  // Display the raw request packet
    Serial.printf("CCCD Updated: %d\n", cccdValue);

  // Check the characteristic this CCCD update is associated with in case
  // this handler is used for multiple CCCD records.
  if (chr.uuid == pwrMeasChar.uuid) {
    if (chr.notifyEnabled()) {
      Serial.println("Pwr Measurement 'Notify' enabled");
    } else {
      Serial.println("Pwr Measurement 'Notify' disabled");
    }
  }
#endif
}

/*
 * Given a 16-bit uint16_t, convert it to 2 8-bit ints, and set
 * them in the provided array. Assume the array is of correct
 * size, allocated by caller. Least-significant octet is place
 * in output array first.
 */
void uint16ToLso(uint16_t val, uint8_t* out) {
  uint8_t lso = val & 0xff;
  uint8_t mso = (val >> 8) & 0xff;
  out[0] = lso;
  out[1] = mso;
}
