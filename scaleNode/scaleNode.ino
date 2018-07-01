#include <ArduinoJson.h>
#include <ESP8266WebServer.h>

String processCommand(String cmd, int weight = 0);

ESP8266WebServer server(80);

void setup() {

  Serial.begin(115200);

  server.on("/", HTTP_GET, [] () {
    server.send(404, "text/plain", "Nada aqui...");
  });

  server.on("/read", HTTP_GET, [] () {
    server.send(200, "text/json", processCommand("read"));
  });

  server.on("/tare", HTTP_GET, [] () {
    server.send(200, "text/json", processCommand("tare"));
  });

  server.on("/blink", HTTP_GET, [] () {
    server.send(200, "text/json", processCommand("blink"));
  });

  server.on("/calibrate", HTTP_GET, [] () {
    if (server.hasArg("weight")) {
      int weight = server.arg("weight").toInt();
      server.send(200, "text/json", processCommand("calibrate", weight));
    } else {
      server.send(404, "text/plain", "Parametro weight faltando");      
    }
  });

  server.begin();
}

void loop() {
  server.handleClient();
}

String processCommand(String cmd, int weight) {
  String response;
  return response;
}

