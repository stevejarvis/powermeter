/**
 * This file keeps the BLE helpers, to send the data over bluetooth
 * to the bike computer. Or any other receiver, if dev/debug.
 */

void blePublishPower(double instantPwr) {
  // The BLE spec...
  // TODO
  Serial.write('p');
  Serial.println(instantPwr);
}
