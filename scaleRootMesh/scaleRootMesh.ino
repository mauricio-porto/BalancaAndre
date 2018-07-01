#include <SoftwareSerial.h>
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
#define   MESH_CHANNEL    4

const unsigned int MAX_INPUT = 512;

// Prototypes
void receivedCallback( const uint32_t &from, const String &msg );
void taskReadAllCallback();
void taskReadAllOnDisable();
void processRequest(String jsonCommand);
void taskReceiveRequestCallback();
void processIncomingByte (const byte inByte);
String serializeJson();
String serializeNodeList();

String readAll();
String readScale(uint32_t scaleID);

void sendCommand(uint32_t scaleID, String cmd, int weight = 0);
void sendReadRequest(uint32_t scaleID);

uint32_t strToUint32(String scaleID);
boolean isNodeReachable(uint32_t nodeId);

painlessMesh  mesh;
Scheduler     runner; // to control your personal task
Task taskReceiveCommand(100 * TASK_MILLISECOND, TASK_FOREVER, &taskReceiveRequestCallback);
Task taskReadAll(10 * TASK_MILLISECOND, TASK_FOREVER, &taskReadAllCallback, &runner, false, NULL, &taskReadAllOnDisable);

uint32_t myNodeId;

SimpleList<uint32_t> nodes;
long num_nodes;

boolean waitingResponse;

typedef struct {
  String cmd;
  String scaleId;
  String status;
  int value;
} replyType;

replyType reply;

SoftwareSerial sSerial(13, 15, false, MAX_INPUT); // GPIO13 - RX, GPIO15 - TX (aka D7, D8)

void setup() {
  Serial.begin(115200);

  // A SoftwareSerial em ESP8266 junto com Mesh não suporta velocidade alta, por isso 38400
  sSerial.begin(38400);
  while(sSerial.available()) sSerial.read();  // Clear buffer

  //mesh.setDebugMsgTypes( ERROR | DEBUG | STARTUP | CONNECTION );  // set before init() so that you can see startup messages
  mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | MSG_TYPES | REMOTE ); // all types on
  //mesh.setDebugMsgTypes( ERROR );

  mesh.init( MESH_PREFIX, MESH_PASSWORD, &runner, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
  mesh.setRoot();
  //mesh.setContainsRoot();
  mesh.onReceive(&receivedCallback);

  runner.addTask(taskReceiveCommand);
  runner.addTask(taskReadAll);

  taskReceiveCommand.enable();
  //taskReadAll.setTimeout(30 * TASK_SECOND);

  myNodeId = mesh.getNodeId();

  //ESP.deepSleep(5e6);
}

void loop() {
  runner.execute();
  mesh.update();
}

void taskReceiveRequestCallback() {
  while (sSerial.available()) processIncomingByte(sSerial.read());
}

void processRequest(String jsonCommand) {

  waitingResponse = false;

Serial.printf("Request received: %s\n", jsonCommand.c_str());

  StaticJsonBuffer<MAX_INPUT> jsonBuffer;
  JsonObject& rcvdJson = jsonBuffer.parseObject(jsonCommand);
  if (!rcvdJson.success()) {
    Serial.println("parseObject() failed");
    sSerial.flush();
    sSerial.println("{\"status\":\"Failed\",\"reason\":\"parseObject() failed\"}");
    return;
  }

  reply.cmd = rcvdJson["command"].as<String>();

  if (String("list").equals(reply.cmd)) {    
    sSerial.flush();
    sSerial.println(serializeNodeList());
    return;
  }

  reply.scaleId = rcvdJson["scaleId"].as<String>();
  reply.value = -1;                                // MENOS 1 significa value não usado

  uint32_t nodeId = strToUint32(reply.scaleId);
  int weight = rcvdJson["weight"].as<int>();

  boolean reached = (myNodeId == nodeId) || isNodeReachable(nodeId);
  if (!reached) {
    reply.status = "FALHOU: identificador nao encontrado";
    sSerial.flush();
    sSerial.println(serializeJson());
    return;
  }

  reply.status = "OK";
  if (myNodeId == nodeId) {         // FAZ AQUI E AGORA  :-)
    if (String("read").equals(reply.cmd)) {
      reply.value = 69;      // lê valor da balança local
    } else {
      // invoca o comando para a balança
    }
  } else {
    waitingResponse = (String("read").equals(reply.cmd));
    sendCommand(nodeId, reply.cmd, weight);
  }

  if (!waitingResponse) {
    String resp = serializeJson();
    sSerial.flush();
    sSerial.println(resp);
  
    Serial.print("scaleRootMesh enviou resposta: ");
    Serial.println(resp);
  }
}

void sendCommand(uint32_t scaleId, String cmd, int weight) {
  String msg = cmd;
  if (weight > 0) {
    msg += ":";
    msg += weight;
  }
  mesh.sendSingle(scaleId, msg);
}

// SOMENTE RECEBE RESPOSTAS DE PEDIDOS DE LEITURA. TODOS OS OUTROS COMANDOS NÃO RETORNAM RESPOSTA/VALOR
void receivedCallback( const uint32_t &from, const String &msg ) {
  Serial.printf("scaleRootMesh: Received from %u msg=%s\n", from, msg.c_str());
  if (waitingResponse) {
    reply.value = msg.toInt();
    sSerial.flush();
    sSerial.println(serializeJson());
  }
}

void taskReadAllCallback() {
  if (num_nodes <= 0) {
    taskReadAll.disable();
  }
}

void taskReadAllOnDisable() {
  /*
  if (taskReadAll.timedOut()) {
    Serial.println("taskReadAll has timed out");
  } else {
    Serial.println("taskReadAll has received all responses");
  }
  */
}

//=====================

String serializeJson() {
  String serializedJson;

  StaticJsonBuffer<MAX_INPUT> jsonBuffer;
  JsonObject& jsonObj = jsonBuffer.createObject();

  jsonObj["cmd"] = reply.cmd;
  jsonObj["scaleId"] = reply.scaleId;
  jsonObj["status"] = reply.status;
  if (reply.value > -1) {
    jsonObj["value"] = reply.value;
  }

  jsonObj.printTo(serializedJson);
  return serializedJson;
}

String serializeNodeList() {
  String serializedJson;

  StaticJsonBuffer<MAX_INPUT> jsonBuffer;
  JsonObject& jsonObj = jsonBuffer.createObject();

  nodes = mesh.getNodeList();
  jsonObj["numNodes"] = nodes.size();
  JsonArray& nodeList = jsonObj.createNestedArray("nodeList");
  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end()) {
    nodeList.add(*node);
    node++;
  }

  jsonObj.printTo(serializedJson);
  return serializedJson;
}

String readScale(uint32_t scaleID) {
  String response;

  String req = "read";
  mesh.sendSingle(scaleID, req);  // TODO: PRECISA TRATAR RETORNO na receivedCallBack

  StaticJsonBuffer<256> jsonBuffer;
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
  nodes = mesh.getNodeList();
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

void processIncomingByte (const byte inByte) {
  static char input_line [MAX_INPUT];
  static unsigned int input_pos = 0;

  switch (inByte) {
    case '\n':   // end of text
      input_line [input_pos] = 0;  // terminating null byte
      processRequest(String(input_line));
      input_pos = 0;  
      break;
    case '\r':   // discard carriage return
      break;
    default:
      // keep adding if not full ... allow for terminating null byte
      if (input_pos < (MAX_INPUT - 1))
        input_line [input_pos++] = inByte;
      break;
  }
}

