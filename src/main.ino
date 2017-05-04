extern "C" {
  #include <user_interface.h>
}
#include "FS.h"
#include <ESP8266WiFi.h>
#define DATA_LENGTH           112

#define TYPE_MANAGEMENT       0x00
#define TYPE_CONTROL          0x01
#define TYPE_DATA             0x02
#define SUBTYPE_PROBE_REQUEST 0x04
#define SUBTYPE_BEACON        0x08

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

//struct softap_config *APconfig;
int count = 0;
int numberOfMACs = 0;
int probeCount = 0;
int numberOfInterrupts = 0;
volatile byte interruptCounter = 0;
const byte interruptPin = 3;

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

  //Create and open csv file to append packet
  File MAClog = SPIFFS.open("/captureMACs.csv", "a+");
  //File SSIDlog = SPIFFS.open("/captureSSID.csv", "a+");
  if (!MAClog){
      Serial.println("files failed to open");
  }

  char addr[] = "00:00:00:00:00:00";
  getMAC(addr, snifferPacket->data, 10);

  MAClog.println(addr);

  printDataSpan(26, SSID_length, snifferPacket->data);
  //SSIDlog.println(printDataSpan(26, SSID_length, snifferPacket->data));
  Serial.println();

  //Check 2nd byte, 7th bit for encryption
  //Serial.print("Encryption: ");
  //Serial.print(snifferPacket->data[2]);
  //Serial.println();
  //printPacket(snifferPacket->rx_ctrl);
  /*Serial.print(snifferPacket->rx_ctrl.rssi, HEX);
  Serial.print(snifferPacket->rx_ctrl.rate, HEX);
  Serial.print(snifferPacket->rx_ctrl.is_group, HEX);*/

  //Perform hex dump of captured packet to serial
  hexDump(snifferPacket->data);

  Serial.println();

  MAClog.close();
  //SSIDlog.close();

  // Reopen csv to read captured packets
  MAClog = SPIFFS.open("/captureMACs.csv", "r");
  if(!MAClog){
      Serial.println("file failed to open");
  }

  if(uniqueString(MAClog, addr)){
      count++;
  }

  Serial.println(count);
  Serial.println();
  probeCount++;

  MAClog.close();
}

/**
 * Callback for promiscuous mode
 */
static void ICACHE_FLASH_ATTR sniffer_callback(uint8_t *buffer, uint16_t length) {
    struct SnifferPacket *snifferPacket = (struct SnifferPacket*) buffer;
    showMetadata(snifferPacket);
}

static void printDataSpan(uint16_t start, uint16_t size, uint8_t* data) {
    for(uint16_t i = start; i < DATA_LENGTH && i < start+size; i++) {
        Serial.write(data[i]);
    }
}

static void getMAC(char *addr, uint8_t* data, uint16_t offset) {
    sprintf(addr, "%02x:%02x:%02x:%02x:%02x:%02x", data[offset+0], data[offset+1],
                    data[offset+2], data[offset+3], data[offset+4], data[offset+5]);
}

/*static void printPacket(uint8_t* data){
    for(uint16_t i = 0; i < DATA_LENGTH; i++) {
        printf("%02x", data[i]);
        Serial.print(" ");
        if((i+1)%10 == 0)
            Serial.println();
    }
}*/

static void hexDump(uint8_t* data){
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
}

static char* stringToChar(String s, int buffer){
    char array[buffer];
    for(int i = 0; i <= buffer; i++){
        array[i] = s[i];
    }
    return array;
}

static bool uniqueString(File f, char *current){

    String s;
    char* entry;
    bool unique = 1;

    while (f.available()) {
        s = f.readStringUntil('\n');
        entry = stringToChar(s, 16);
        Serial.println(current);
        Serial.println(entry);
        if(strcmp(current, entry) == 0){
            unique = 0;
        }
    }
  return unique;
}

void handleInterrupt() {
  interruptCounter++;
}

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

void setup() {

    //Start serial connection
    Serial.begin(115200);
    pinMode(interruptPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, RISING);

    //Format file system
    SPIFFS.begin();
    Serial.println();
    Serial.println("Please wait 30 secs for SPIFFS to be formatted");
    SPIFFS.format();
    Serial.println("Spiffs formatted");

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
}

void loop() {
    if(interruptCounter > 0){

        interruptCounter--;
        numberOfInterrupts++;

        Serial.print("An interrupt has occurred. Total: ");
        Serial.println(numberOfInterrupts);

        wifi_promiscuous_enable(DISABLE);

        wifi_set_promiscuous_rx_cb(NULL);

        wifi_set_opmode(WIFI_AP);

        //struct softap_config ap_config = {"OpenWrt3"};

        //wifi_softap_set_config(&ap_config);

        Serial.print("Setting soft-AP ... ");

        Serial.println(WiFi.softAP("ESPsoftAP_01") ? "Ready" : "Failed!");

    }
    if(numberOfInterrupts > 0){
        Serial.printf("Stations connected = %d\n", WiFi.softAPgetStationNum());
        delay(3000);

    }
    delay(1000);

}
