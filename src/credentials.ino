/** Load WLAN credentials from EEPROM */

void loadCredentials( int i) {

  if( addr >= 512 ){
    addr = 0;
  }

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
    previousCaptures = i + 1;
  }
}

/** Store WLAN credentials to EEPROM */
void saveCredentials() {

  if( addr >= 512 ){
    addr = 0;
  }

  Serial.println(capturedCredentials[capturecount].username);
  Serial.println(capturedCredentials[capturecount].password);

  EEPROM.begin(512);
  EEPROM.put(addr, capturedCredentials[capturecount].username);
  addr += sizeof(capturedCredentials[capturecount].username);

  EEPROM.put(addr, capturedCredentials[capturecount].password);
  addr += sizeof(capturedCredentials[capturecount].password);

  //mark the end of stored credentials
  char ok[2+1] = "OK";
  EEPROM.put(addr, ok);

  EEPROM.commit();
  EEPROM.end();

}


void clearEEPROM(){
  EEPROM.begin(512);
  for( int i = 0; i <= 512 ; i++){
    EEPROM.write(i,0);
  }
  char ok[2+1] = "OK";
  EEPROM.put(64, ok);
  EEPROM.commit();
  EEPROM.end();

}
