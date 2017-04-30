extern "C" {
  #include <user_interface.h>
}
#include "FS.h"
#define DATA_LENGTH           112

#define TYPE_MANAGEMENT       0x00
#define TYPE_CONTROL          0x01
#define TYPE_DATA             0x02
#define SUBTYPE_PROBE_REQUEST 0x04


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

int count = 0;

static void showMetadata(SnifferPacket *snifferPacket) {

  unsigned int frameControl = ((unsigned int)snifferPacket->data[1] << 8) + snifferPacket->data[0];

  uint8_t version      = (frameControl & 0b0000000000000011) >> 0;
  uint8_t frameType    = (frameControl & 0b0000000000001100) >> 2;
  uint8_t frameSubType = (frameControl & 0b0000000011110000) >> 4;
  uint8_t toDS         = (frameControl & 0b0000000100000000) >> 8;
  uint8_t fromDS       = (frameControl & 0b0000001000000000) >> 9;
  uint8_t SSID_length  = snifferPacket->data[25];

  // Only look for probe request packets
  if (frameType != TYPE_MANAGEMENT || frameSubType != SUBTYPE_PROBE_REQUEST)
        return;

  // Filter out broadcast probes
  if(SSID_length == 0){
     return;
  }

  //Create and open csv file to append packet
  File MAClog = SPIFFS.open("/captureMAC.csv", "a+");
  File SSIDlog = SPIFFS.open("/captureSSID.csv", "a+");
  if (!MAClog || !SSIDlog){
      Serial.println("files failed to open");
  }

  Serial.print("RSSI: ");
  Serial.print(snifferPacket->rx_ctrl.rssi, DEC);

  Serial.print(" Ch: ");
  Serial.println(wifi_get_channel());

  char addr[] = "00:00:00:00:00:00";
  getMAC(addr, snifferPacket->data, 10);
  Serial.print("Client MAC: ");
  Serial.println(addr);

  MAClog.println(addr);

  Serial.print("Target SSID: ");
  printDataSpan(26, SSID_length, snifferPacket->data);
  //SSIDlog.println(printDataSpan(26, SSID_length, snifferPacket->data));
  Serial.println();

  Serial.print("Encryption: ");
  Serial.println(snifferPacket->data[1], BIN);

  printPacket(snifferPacket->data);
  Serial.println();

  MAClog.close();
  SSIDlog.close();

  // Reopen csv to read captured packets
  MAClog = SPIFFS.open("/captureMAC.csv", "r");
  if(!MAClog){
      Serial.println("file failed to open");
  }

  String s;
  char* entry;
  int addMAC = 1;
  //Serial.println(addr);
  //Serial.println();

  // unique string function
  while (MAClog.available()) {
      s = MAClog.readStringUntil('\n');
      entry = stringToChar(s, 16);
      //Serial.println(entry);
      if(strcmp(addr, entry) != 0){
          addMAC = 0;
      }
  }
  if(addMAC == 1){
      count++;
  }

  Serial.println(count);
  Serial.println();

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

static void printPacket(uint8_t* data){
    for(uint16_t i = 0; i < 200; i++) {
        printf("%02x", data[i]);
        Serial.print(" ");
        if((i+1)%10 == 0)
            Serial.println();
    }
}

static char* stringToChar(String s, int buffer){
    char array[buffer];
    for(int i = 0; i <= buffer; i++){
        array[i] = s[i];
    }
    return array;
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

    //Format file system
    SPIFFS.begin();
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
    delay(10);
}
