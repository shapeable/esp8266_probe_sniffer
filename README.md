# ESP8266 Probe Sniffer

This repository is heavily derived from the following examples:  
* http://github.com/kalanda/esp8266-sniffer
* https://github.com/squix78/esp8266-oled-ssd1306/ 
* https://github.com/esp8266/Arduino/tree/master/libraries/DNSServer

Currently, the main changes/additions include:  
* Rejection of broadcast probes (considered uninteresting)  
* hexDump function to view captured packets  
* an SSD1306 i2c screen to display SSIDs of captured probe requests
* structures for organizing SSID and MAC address data
* logic for identifying news SSIDs and unique probe requests
* logic for sorting SSIDs by # of unique requests and average RSSIs
* captive portal with logon form  
* clickButton library for debouncing interrupts
* disable buttons after selection of SSID
* turn off channel hopping before setting up as AP
* moved captive portal setup and loop to separate ino file
* captured credentials saved to EEPROM and dumped over serial on reset
* ability to clear EEPROM on reset by uncommenting one line (or on button combination?)
* screen timeouts (deep sleep after X seconds of inactivity) on SSID list and captive portal screens
* asynchronous web server for captive portal
* full website stored in SPIFFS

# Dependencies
* platformio-core (install with ```sudo pip install platformio```)
* all other dependecies are taken care of by the ```platformio.ini``` file

# To flash this firmware:
1. clone this repository
2. connect ESP8266 board to computer with micro USB cable
3. cd into the cloned repository and run following command from terminal ```pio run -t upload```
4. if this does not work try specifying the USB device like so ```pio run -t upload --upload-port /dev/ttyUSB0``` or whatever port your USB device defaulted to.

# From original repo
This is only an easy experiment which uses the ESP8266 wifi module to look for near smartphones around you. You can do this very easily with any computer and some software but this is a good way to learn the possibilities of these tiny ESP8266 modules.

**VERY IMPORTANT:** *This code is only for educational purposes. We don’t want to listen for any private communication and we don't do it. All packets that you can listen with this code are public packets without any encryption or secure layer on it, continuously broadcasted to the air by smartphones. Please, check which country's laws applies to you before use this code.*

## Introduction

Some time ago I saw this [video](https://youtu.be/DbqkBAjId_U?t=405) of *Chema Alonso* about how insecure are our mobile phones. He explains, among other things, that your phone is constantly searching for all WiFi networks which you already connect  in the past (unless you did remove as "saved"), saying to anyone who is listening for those public packets where you have been before, and of course, your unique device MAC address.

Those public packets are named as "probe requests" and are used by smartphones to connect faster to wifi networks than if it waits for the network send a Beacon frame to announce the SSID.

This program just listen for those "probe requests" and prints to serial port the information. For now only shows the RSSI (bigger values are near devices), the MAC address of the device and the SSID (if available) of the wifi network which is looking for. Something like that:

![](doc/capture.jpg)

## Code  

This repository uses [Platformio](http://platformio.org/platformio-ide) to compile and upload code to ESP8266 but you can also use it with [Arduino IDE](https://github.com/esp8266/Arduino#installing-with-boards-manager).

I use one [nodeMCU devkit](https://github.com/nodemcu/nodemcu-devkit) because is a fast and easy way to program the chip because it includes serial to usb converter and breadboard compatibility but there are [many flavors](http://www.esp8266.com/wiki/doku.php?id=esp8266-module-family) available.

## Useful links

- [Bins that track mobiles banned by City of London Corporation](http://www.telegraph.co.uk/technology/news/10237811/Bins-that-track-mobiles-banned-by-City-of-London-Corporation.html)
- [Where’ve you been? Your smartphone’s Wi-Fi is telling everyone](http://arstechnica.com/information-technology/2014/11/where-have-you-been-your-smartphones-wi-fi-is-telling-everyone/)
- [Passive wifi tracking](http://edwardkeeble.com/2014/02/passive-wifi-tracking/)
- [IEEE 802.11 frame format](http://www.studioreti.it/slide/802-11-Frame_E_C.pdf)
- [esp8266 forum](http://www.esp8266.com/viewtopic.php?f=6&t=1589)
