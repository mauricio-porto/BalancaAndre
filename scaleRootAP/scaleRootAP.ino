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

// #define _TASK_TIMEOUT   <<<===  Needs to be in the TaskScheduler.cpp and painlessMesh.h of the PainlesMesh source code

#include "IPAddress.h"
#include "painlessMesh.h"
#include <ArduinoJson.h>

#ifdef ESP8266
#include "Hash.h"
#include <ESPAsyncTCP.h>
#else
#include <AsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

#define   MESH_PREFIX     "meshOne"
#define   MESH_PASSWORD   "onePassword"
#define   MESH_PORT       5555

#define   STATION_SSID     "balancas"
#define   STATION_PASSWORD "andrepayao"
#define   STATION_PORT     5555

// Prototypes
void receivedCallback( const uint32_t &from, const String &msg );
void taskReadAllCallback();
void taskReadAllOnDisable();
String readAll();
String readScale(uint32_t scaleID);

String sendCommand(uint32_t scaleID, String cmd, int weight = 0);
void sendReadRequest(uint32_t scaleID);

uint32_t strToUint32(String scaleID);
boolean isNodeReachable(uint32_t nodeId);

painlessMesh  mesh;
Scheduler     runner; // to control your personal task
Task taskReadAll(10 * TASK_MILLISECOND, TASK_FOREVER, &taskReadAllCallback, &runner, false, NULL, &taskReadAllOnDisable);
uint32_t myNodeId;

long num_nodes;

AsyncWebServer server(80);
IPAddress myIP(0,0,0,0);
IPAddress myAPIP(0,0,0,0);

void setup() {
  Serial.begin(115200);

  mesh.setDebugMsgTypes( ERROR | DEBUG | STARTUP | CONNECTION );  // set before init() so that you can see startup messages

  // Channel set to 1. Make sure to use the same channel for your mesh and for you other
  // network (STATION_SSID)
  mesh.init( MESH_PREFIX, MESH_PASSWORD, &runner, MESH_PORT);
  mesh.onReceive(&receivedCallback);

  runner.addTask(taskReadAll);

  taskReadAll.setTimeout(30 * TASK_SECOND);

  myNodeId = mesh.getNodeId();

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

  server.on("/readAll", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/json", readAll());
  });

  server.on("/read", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasArg("scale")) {
      String scaleID = request->arg("scale");
      uint32_t nodeId = strToUint32(scaleID);
      if (myNodeId == nodeId) {
        // FAZ AQUI E AGORA  :-)
      } else {
        if (isNodeReachable(nodeId)) {
          request->send(200, "text/json", readScale(nodeId));
        } else {
          String msg = "Identificador ";
          msg += scaleID;
          msg += " não encontrado";
          request->send(404, "text/plain", msg);
        }
      }
    } else {
      request->send(404, "text/plain", "Parametro scale faltando");
    }
  });

  server.on("/tare", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasArg("scale")) {
      String scaleID = request->arg("scale");
      uint32_t nodeId = strToUint32(scaleID);
      if (myNodeId == nodeId) {
        // FAZ AQUI E AGORA  :-)
      } else {
        if (isNodeReachable(nodeId)) {
          request->send(200, "text/json", sendCommand(nodeId, "tare"));
        } else {
          String msg = "Identificador ";
          msg += scaleID;
          msg += " não encontrado";
          request->send(404, "text/plain", msg);
        }
      }
    } else {
      request->send(404, "text/plain", "Parametro scale faltando");
    }
  });

  server.on("/blink", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasArg("scale")) {
      String scaleID = request->arg("scale");
      uint32_t nodeId = strToUint32(scaleID);
      if (myNodeId == nodeId) {
        // FAZ AQUI E AGORA  :-)
      } else {
        if (isNodeReachable(nodeId)) {
          request->send(200, "text/json", sendCommand(nodeId, "blink"));
        } else {
          String msg = "Identificador ";
          msg += scaleID;
          msg += " não encontrado";
          request->send(404, "text/plain", msg);
        }
      }
    } else {
      request->send(404, "text/plain", "Parametro scale faltando");
    }
  });

  server.on("/calibrate", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasArg("scale")) {
      String scaleID = request->arg("scale");
      uint32_t nodeId = strToUint32(scaleID);
      if (request->hasArg("weight")) {
        int weight = request->arg("weight").toInt();
        if (myNodeId == nodeId) {
          // FAZ AQUI E AGORA  :-)
        } else {
          if (isNodeReachable(nodeId)) {
            request->send(200, "text/json", sendCommand(nodeId, "calibrate", weight));
          } else {
            String msg = "Identificador ";
            msg += scaleID;
            msg += " não encontrado";
            request->send(404, "text/plain", msg);
          }
        }
      } else {
        request->send(404, "text/plain", "Parametro weight faltando");      
      }
    } else {
      request->send(404, "text/plain", "Parametro scale faltando");
    }
  });

  server.begin();

}

void loop() {
  mesh.update();
}

// SOMENTE RECEBE RESPOSTAS DE PEDIDOS DE LEITURA. TODOS OS OUTROS COMANDOS NÃO RETORNAM RESPOSTA/VALOR
void receivedCallback( const uint32_t &from, const String &msg ) {
  Serial.printf("scaleRootAP: Received from %u msg=%s\n", from, msg.c_str());
}

void taskReadAllCallback() {
  if (num_nodes <= 0) {
    taskReadAll.disable();
  }
}

void taskReadAllOnDisable() {
  if (taskReadAll.timedOut()) {
    Serial.println("taskReadAll has timed out");
  } else {
    Serial.println("taskReadAll has received all responses");
  }
}

//=====================

String sendCommand(uint32_t scaleID, String cmd, int weight) {
  String response;

  mesh.sendSingle(scaleID, cmd);  // SE FOR calibrate precisa passar também o weight

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["response"] = cmd;
  root["scale"] = scaleID;
  root["status"] = "OK";

  root.prettyPrintTo(response);
  return response;  
}

String readAll() {
  String response;

  SimpleList<uint32_t> nodes = mesh.getNodeList();
  int scales = nodes.size();

  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["response"] = "readAll";
  root["status"] = "OK";
  root["scales"] = scales;
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

String readScale(uint32_t scaleID) {
  String response;

  String req = "read";
  mesh.sendSingle(scaleID, req);  // TODO: PRECISA TRATAR RETORNO na receivedCallBack

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["response"] = "read";
  root["scale"] = scaleID;
  root["status"] = "OK";
  root["value"] = "15";

  root.prettyPrintTo(response);
  return response;
}

uint32_t strToUint32(String scaleID) {
   unsigned long long y = 0;

  for (int i = 0; i < scaleID.length(); i++) {
    char c = scaleID.charAt(i);
    if (c < '0' || c > '9') break;
    y *= 10;
    y += (c - '0');
  }

  return (uint32_t) y;
}

boolean isNodeReachable(uint32_t nodeId) {
  boolean result = false;
  SimpleList<uint32_t> nodes = mesh.getNodeList();
  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end()) {
    if (nodeId == *node) {
      result = true;
      break;
    }
    node++;
  }
  return result;
}

