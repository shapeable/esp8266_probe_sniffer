/** Load WLAN credentials from EEPROM */
void loadCredentials( int i) {

  EEPROM.begin(512);

  EEPROM.get(addr, capturedCredentials[i].username);
  addr += sizeof(capturedCredentials[i].username);
  EEPROM.get(addr, capturedCredentials[i].password);
  addr += sizeof(capturedCredentials[i].password);
  char ok[2+1];
  EEPROM.get(addr, ok);
  EEPROM.end();

  //check for end of stored credentials
  if (String(ok) == String("OK")) {
    previousCaptures = i;
  }
}

/** Store WLAN credentials to EEPROM */
void saveCredentials() {

  Serial.println(capturedCredentials[capturecount].username);
  Serial.println(capturedCredentials[capturecount].password);

  EEPROM.begin(512);
  EEPROM.put(addr, capturedCredentials[capturecount].username);
  addr += sizeof(capturedCredentials[capturecount].username);
  Serial.println(sizeof(capturedCredentials[capturecount].username));
  EEPROM.put(addr, capturedCredentials[capturecount].password);
  addr += sizeof(capturedCredentials[capturecount].password);

  //mark the end of stored credentials
  char ok[2+1] = "OK";
  EEPROM.put(addr, ok);

  EEPROM.commit();
  EEPROM.end();

}
