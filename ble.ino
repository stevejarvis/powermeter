/**
 * This file keeps the BLE helpers, to send the data over bluetooth
 * to the bike computer. Or any other receiver, if dev/debug.
 */

#define DEV_NAME "JrvsPwr"

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
     
BLEDis bledis;    // DIS (Device Information Service) helper class instance
BLEBas blebas;    // BAS (Battery Service) helper class instance

void bleSetup() {
  Bluefruit.begin();
  Bluefruit.setName(DEV_NAME);
  
  // Set the connect/disconnect callback handlers
  Bluefruit.setConnectCallback(connectCallback);
  Bluefruit.setDisconnectCallback(disconnectCallback);
     
  // Configure and Start the Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();
     
  // Start the BLE Battery Service
  blebas.begin();
  // NOTE TODO right now this is never updated, it'll always read this value
  blebas.write(90);
     
  // Setup the Heart Rate Monitor service using
  // BLEService and BLECharacteristic classes
  setupPwr();
     
  // Setup the advertising packet(s)
  startAdv();

#ifdef DEBUG
  Serial.println("BLE module configured and advertising.");
#endif
}

void startAdv(void) {
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(pwrService);
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
     
void setupPwr(void)
{
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

void blePublishPower(double instantPwr) {
  static int bps = 0;
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
   *   16bits signed int
   */
  // We're using none of the additional, optional flags so far.
  // Per the Arduino docs, a short on all architectures in 16 bit.
  uint16_t pwrdata[2] = { 0b0000000000000000, short(instantPwr) };

  if (pwrMeasChar.notify(pwrdata, sizeof(pwrdata))) {
#ifdef DEBUG
    Serial.print("Power measurement updated to: "); 
    Serial.println(short(instantPwr)); 
  } else {
    Serial.println("ERROR: Notify not set in the CCCD or not connected!");
#endif
  }
}

void connectCallback(uint16_t connHandle) {
  char centralName[32] = { 0 };
  Bluefruit.Gap.getPeerName(connHandle, centralName, sizeof(centralName));

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
  
#ifdef DEBUG
      Serial.println("Disconnected");
      Serial.println("Advertising!");
#endif
}
     
void cccdCallback(BLECharacteristic& chr, uint16_t cccdValue) {
#ifdef DEBUG  
  // Display the raw request packet
  Serial.print("CCCD Updated: ");
  //Serial.printBuffer(request->data, request->len);
  Serial.println(cccdValue);
     
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
