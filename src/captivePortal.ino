
const char *myHostname = "esp8266";

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
ESP8266WebServer server(80);

String responseHTML = ""
  "<!DOCTYPE html><html><head><title>CaptivePortal</title></head><body>"
  "<h1>Please sign in to your WiFi</h1>"
  "</table>"
  "\r\n<br /><form method='POST' action='wifisave'><h4>Connect to network:</h4>"
  "<input type='text' placeholder='username' name='n'/>"
  "<br /><input type='password' placeholder='password' name='p'/>"
  "<br /><input type='submit' value='Connect/Disconnect'/></form>"
"</body></html>";

void captiveSetup() {

  //wifi_set_phy_mode(PHY_MODE_11B);
  ESP.eraseConfig();
  WiFi.disconnect();

  //WiFi.mode(WIFI_AP);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  yield();

  String apstr = SSIDlist[numberOfInterrupts2+1].name;
  int str_len = apstr.length() + 1;
  char ap[str_len];
  apstr.toCharArray(ap, str_len);
  WiFi.softAP(ap);

  //WiFi.softAP("ESP-D1mini");

  //delay(500);

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);

  // replay to all requests with same HTML
  server.onNotFound([]() {
    server.send(200, "text/html", responseHTML);
  });
  server.on("/wifisave", handleWifiSave);
  server.begin();

  yield();

}

void captiveLoop() {
  dnsServer.processNextRequest();
  server.handleClient();
}
