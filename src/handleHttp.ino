/** Redirect to captive portal if we got a request for another domain. Return true in that case so the page handler do not try to handle the request again. */
/*boolean captivePortal() {
  if (!isIp(server.hostHeader()) && server.hostHeader() != (String(myHostname)+".local")) {
    Serial.print("Request redirected to captive portal");
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send ( 302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}*/

/** Handle the WLAN save form and redirect to WLAN config page again */
/*void handleWifiSave() {
  Serial.println("wifi save");

  // capture credentials POSTed in portal
  //httpd.arg("u").toCharArray(capturedCredentials[capturecount].username, 32);
  //httpd.arg("p").toCharArray(capturedCredentials[capturecount].password, 32);
  httpd.sendHeader("Location", "wifi", true);
  httpd.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  httpd.sendHeader("Pragma", "no-cache");
  httpd.sendHeader("Expires", "-1");
  httpd.send ( 302, "text/plain", "");  // Empty content inhibits Content-length header so we have to close the socket ourselves.
  httpd.client().stop(); // Stop is needed because we sent no content length
  saveCredentials();
  capturecount++;
}*/

/*void handleNotFound() {
  if (captivePortal()) { // If caprive portal redirect instead of displaying the error page.
    return;
  }
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send ( 404, "text/plain", message );
}*/
