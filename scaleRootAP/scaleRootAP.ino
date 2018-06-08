//************************************************************
// this is a simple example that uses the painlessMesh library to
// connect to a another network and broadcast message from a webpage to the edges of the mesh network.
// This sketch can be extended further using all the abilities of the AsyncWebserver library (WS, events, ...)
// for more details
// https://gitlab.com/painlessMesh/painlessMesh/wikis/bridge-between-mesh-and-another-network
// for more details about my version
// https://gitlab.com/Assassynv__V/painlessMesh
// and for more details about the AsyncWebserver library
// https://github.com/me-no-dev/ESPAsyncWebServer
//************************************************************

#define _TASK_TIMEOUT

#include "IPAddress.h"
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>
#include <ESP8266WebServer.h>

#define   STATION_SSID     "balancas"
#define   STATION_PASSWORD "andrepayao"
#define   STATION_PORT     5555

// Prototypes
void sendCommand(String scaleID, String cmd, int weight = 0);
void taskReceiveMsgCallback();
void taskReceiveMsgOnDisable();

Scheduler     runner; // to control your personal task
Task taskReceiveMsg(1 * TASK_SECOND, TASK_FOREVER, &taskReceiveMsgCallback, &runner, false, NULL, &taskReceiveMsgOnDisable);

ESP8266WebServer server(80);
IPAddress myIP(0,0,0,0);
IPAddress myAPIP(0,0,0,0);

SoftwareSerial sSerial(13, 15); // GPIO13 - RX, GPIO15 - TX (aka D7, D8)

void setup() {
  Serial.begin(115200);
  sSerial.begin(115200);

  runner.addTask(taskReceiveMsg);

  taskReceiveMsg.setTimeout(10 * TASK_SECOND);

  // Trying the ACCESS POINT
  // =======================

  Serial.print("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(STATION_SSID, STATION_PASSWORD);

  IPAddress localIP(192,168,222,11);
  IPAddress gw(192,168,222,11);
  IPAddress mask(255,255,255,0);
  WiFi.softAPConfig(localIP, gw, mask);
  
  myAPIP = WiFi.softAPIP();
  Serial.println("My AP IP is " + myAPIP.toString());

  server.on("/", HTTP_GET, [] () {
    server.send(404, "text/plain", "Nada aqui...");
  });

  server.on("/readAll", HTTP_GET, [] () {
    if (taskReceiveMsg.isEnabled()) {
      server.send(403, "text/plain", "Aguardando resposta anterior");
    } else {
      server.send(200, "text/json", readAll());
    }
  });

  server.on("/read", HTTP_GET, [] () {
    if (taskReceiveMsg.isEnabled()) {
      //server.send(403, "text/plain", "Aguardando resposta anterior");
    } else {
      if (server.hasArg("scale")) {
        String scaleId = server.arg("scale");
        sendCommand(scaleId, "read");
      } else {
        server.send(404, "text/plain", "Parametro scale faltando");
      }
    }
  });

  server.on("/tare", HTTP_GET, [] () {
    Serial.println("\nChegou requisicao de tare...");
    if (taskReceiveMsg.isEnabled()) {
      //server.send(403, "text/plain", "Aguardando resposta anterior");
    } else {
      if (server.hasArg("scale")) {
        String scaleId = server.arg("scale");
        sendCommand(scaleId, "tare");
      } else {
        server.send(404, "text/plain", "Parametro scale faltando");
      }
    }
  });

  server.on("/blink", HTTP_GET, [] () {
    if (taskReceiveMsg.isEnabled()) {
      //server.send(403, "text/plain", "Aguardando resposta anterior");
    } else {
      if (server.hasArg("scale")) {
        String scaleId = server.arg("scale");
        sendCommand(scaleId, "blink");
      } else {
        server.send(404, "text/plain", "Parametro scale faltando");
      }
    }
  });

  server.on("/calibrate", HTTP_GET, [] () {
    if (taskReceiveMsg.isEnabled()) {
      //server.send(403, "text/plain", "Aguardando resposta anterior");
    } else {
      if (server.hasArg("scale")) {
        String scaleId = server.arg("scale");
        if (server.hasArg("weight")) {
          int weight = server.arg("weight").toInt();
          sendCommand(scaleId, "calibrate", weight);
        } else {
          server.send(404, "text/plain", "Parametro weight faltando");      
        }
      } else {
        server.send(404, "text/plain", "Parametro scale faltando");
      }
    }
  });

  server.begin();

}

void loop() {
  runner.execute();
  server.handleClient();
}

//=====================

void taskReceiveMsgCallback() {
  if (sSerial.available()) {
    String msg = sSerial.readString();  // lembrar de ajustar sSerial.setTimeout() se necessário
    server.send(200, "text/json", msg);
    taskReceiveMsg.disable();
  }
}

void taskReceiveMsgOnDisable() {
  if (taskReceiveMsg.timedOut()) {
    Serial.println("\ntaskReceiveMsg has timed out");
    server.send(408, "text/plain", "Timed out... :-(");
  } else {
    Serial.println("\ntaskReceiveMsg has received response.");
  }
}

void sendCommand(String scaleID, String cmd, int weight) {
  String command;

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["command"] = cmd;
  root["scale"] = scaleID;
  root["weight"] = weight;

  root.printTo(command);

  Serial.print("sendCommand: ");
  Serial.println(command);

  sSerial.print(command);
  taskReceiveMsg.enable();

}

String readAll() {
  String response;

  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["response"] = "readAll";
  root["status"] = "OK";
  root["scales"] = 50;
  JsonArray& values = root.createNestedArray("values");
  DynamicJsonBuffer scaleValue(200);
  JsonObject& scaleRead = scaleValue.createObject();
  scaleRead["123456010"] = "010";
  scaleRead["123456020"] = "020";
  scaleRead["123456030"] = "030";
  scaleRead["123456040"] = "040";
  scaleRead["123456050"] = "050";
  scaleRead["123456060"] = "060";
  scaleRead["123456070"] = "070";
  scaleRead["123456080"] = "080";
  scaleRead["123456090"] = "090";
  scaleRead["123456100"] = "100";
  values.add(scaleRead);
  
  //root.printTo(response);
  root.prettyPrintTo(response);
  return response;
}

