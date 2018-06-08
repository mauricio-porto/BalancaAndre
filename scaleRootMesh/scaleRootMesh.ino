// #define _TASK_TIMEOUT   <<<===  Needs to be in the TaskScheduler.cpp and painlessMesh.h of the PainlesMesh source code

#include <SoftwareSerial.h>
#include "IPAddress.h"
#include "painlessMesh.h"
#include <ArduinoJson.h>

#ifdef ESP8266
#include "Hash.h"
#include <ESPAsyncTCP.h>
#else
#include <AsyncTCP.h>
#endif

#define   MESH_PREFIX     "meshOne"
#define   MESH_PASSWORD   "onePassword"
#define   MESH_PORT       5555

// Prototypes
void receivedCallback( const uint32_t &from, const String &msg );
void taskReadAllCallback();
void taskReadAllOnDisable();
void processCommand(String jsonCommand);
void taskReceiveCommandCallback();

String readAll();
String readScale(uint32_t scaleID);

String sendCommand(uint32_t scaleID, String cmd, int weight = 0);
void sendReadRequest(uint32_t scaleID);

uint32_t strToUint32(String scaleID);
boolean isNodeReachable(uint32_t nodeId);

painlessMesh  mesh;
Scheduler     runner; // to control your personal task
Task taskReceiveCommand(1 * TASK_MILLISECOND, TASK_FOREVER, &taskReceiveCommandCallback);
Task taskReadAll(10 * TASK_MILLISECOND, TASK_FOREVER, &taskReadAllCallback, &runner, false, NULL, &taskReadAllOnDisable);
uint32_t myNodeId;

long num_nodes;

SoftwareSerial sSerial(13, 15); // GPIO13 - RX, GPIO15 - TX (aka D7, D8)

void setup() {
  Serial.begin(115200);
  sSerial.begin(115200);

  mesh.setDebugMsgTypes( ERROR | DEBUG | STARTUP | CONNECTION );  // set before init() so that you can see startup messages
  mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on

  // Channel set to 1. Make sure to use the same channel for your mesh and for you other
  // network (STATION_SSID)
  mesh.init( MESH_PREFIX, MESH_PASSWORD, &runner, MESH_PORT);
  mesh.onReceive(&receivedCallback);

  runner.addTask(taskReceiveCommand);
  runner.addTask(taskReadAll);

  taskReceiveCommand.enable();
  taskReadAll.setTimeout(30 * TASK_SECOND);

  myNodeId = mesh.getNodeId();

}

void loop() {
  runner.execute();
  mesh.update();
}

void taskReceiveCommandCallback() {
  if (sSerial.available()) {
    String jsonCommand = sSerial.readString();
    Serial.println(jsonCommand);
    processCommand(jsonCommand);
  }
}

void processCommand(String jsonCommand) {
  sSerial.print("{'status':'OTE'}");
}

// SOMENTE RECEBE RESPOSTAS DE PEDIDOS DE LEITURA. TODOS OS OUTROS COMANDOS NÃO RETORNAM RESPOSTA/VALOR
void receivedCallback( const uint32_t &from, const String &msg ) {
  Serial.printf("scaleRootMesh: Received from %u msg=%s\n", from, msg.c_str());
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

