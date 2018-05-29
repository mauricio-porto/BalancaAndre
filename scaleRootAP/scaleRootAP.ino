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
// #include <TaskScheduler.h>

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
void replyTo();
void receivedCallback( const uint32_t &from, const String &msg );
void taskReadAllCallback();
void taskReadAllOnDisable();
String readAll();
String readScale(String scaleID);
String tare(String scaleID);
String blink(String scaleID);
String calibrate(String scaleID, int weight);
IPAddress getlocalIP();

painlessMesh  mesh;
Scheduler     runner; // to control your personal task
Task taskReplyTo(TASK_SECOND * 1, TASK_FOREVER, &replyTo ); // start with a one second interval
Task taskReadAll(10 * TASK_MILLISECOND, TASK_FOREVER, &taskReadAllCallback, &runner, false, NULL, &taskReadAllOnDisable);

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

  runner.addTask(taskReplyTo);
  runner.addTask(taskReadAll);

  taskReadAll.setTimeout(30 * TASK_SECOND);

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
      request->send(200, "text/json", readScale(scaleID));
    } else {
      request->send(404, "text/plain", "Parametro scale faltando");
    }
  });

  server.on("/tare", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasArg("scale")) {
      String scaleID = request->arg("scale");
      request->send(200, "text/json", tare(scaleID));
    } else {
      request->send(404, "text/plain", "Parametro scale faltando");
    }
  });

  server.on("/calibrate", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasArg("scale")) {
      String scaleID = request->arg("scale");
      if (request->hasArg("weight")) {
        long weight = request->arg("weight").toInt();
        request->send(200, "text/json", calibrate(scaleID, weight));      
      } else {
        request->send(404, "text/plain", "Parametro weight faltando");      
      }
    } else {
      request->send(404, "text/plain", "Parametro scale faltando");
    }
  });

  server.on("/blink", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if (request->hasArg("scale")) {
      String scaleID = request->arg("scale");
      request->send(200, "text/json", blink(scaleID));
    } else {
      request->send(404, "text/plain", "Parametro scale faltando");
    }
  });

  //Async webserver
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<form>Text to Broadcast<br><input type='text' name='BROADCAST'><br><br><input type='submit' value='Submit'></form>");
    if (request->hasArg("BROADCAST")){
      String msg = request->arg("BROADCAST");
      mesh.sendBroadcast(msg);
      Serial.println("Recebi " + msg);
    }
  });
  server.begin();

}

void loop() {
  mesh.update();
  if(myIP != getlocalIP()){
    myIP = getlocalIP();
    Serial.println("My IP is " + myIP.toString());
  }
}

void receivedCallback( const uint32_t &from, const String &msg ) {
  //Serial.printf("asyncWebServerTest: Received from %u msg=%s\n", from, msg.c_str());
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

void replyTo() {
  
}

String readAll() {
  String response;

  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["response"] = "readAll";
  root["status"] = "OK";
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

String readScale(String scaleID) {
  String response;

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["response"] = "read";
  root["scale"] = scaleID;
  root["status"] = "OK";
  root["value"] = "15";

  root.prettyPrintTo(response);
  return response;
}

String tare(String scaleID) {
  String response;

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["response"] = "tare";
  root["scale"] = scaleID;
  root["status"] = "OK";

  root.prettyPrintTo(response);
  return response;

}

String blink(String scaleID) {
  String response;

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["response"] = "blink";
  root["scale"] = scaleID;
  root["status"] = "OK";

  root.prettyPrintTo(response);
  return response;

}

String calibrate(String scaleID, int weight) {
  String response;

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["response"] = "calibrate";
  root["scale"] = scaleID;
  root["weight"] = weight;
  root["status"] = "OK";

  root.prettyPrintTo(response);
  return response;  
}

IPAddress getlocalIP() {
  return IPAddress(mesh.getStationIP().addr);
}
