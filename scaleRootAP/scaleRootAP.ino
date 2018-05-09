#include <painlessMesh.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>

/* Set these to your desired credentials. */
const char *ssid = "balancas";
const char *password = "apayao";

long reqNumber = 0;

ESP8266WebServer server(80);

void handleRoot() {
  server.send(200, "text/html", "<h1>Hi there!</h1>");
}

void setup() {
  Serial.begin(115200);

  Serial.println("Configuring access point...");

  WiFi.softAP(ssid, password);

  IPAddress localIP(192,168,222,11);
  IPAddress gw(192,168,222,11);
  IPAddress mask(255,255,255,0);
  WiFi.softAPConfig(localIP, gw, mask);
  
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}

