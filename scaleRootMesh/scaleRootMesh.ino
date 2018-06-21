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

const unsigned int MAX_INPUT = 512;

// Prototypes
void receivedCallback( const uint32_t &from, const String &msg );
void taskReadAllCallback();
void taskReadAllOnDisable();
void processRequest(String jsonCommand);
void taskReceiveRequestCallback();
void processIncomingByte (const byte inByte);

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

long num_nodes;

SoftwareSerial sSerial(13, 15, false, MAX_INPUT); // GPIO13 - RX, GPIO15 - TX (aka D7, D8)

void setup() {
  Serial.begin(115200);

  // A SoftwareSerial em ESP8266 junto com Mesh não suporta velocidade alta, por isso 38400
  sSerial.begin(38400);
  while(sSerial.available()) sSerial.read();  // Clear buffer

  mesh.setDebugMsgTypes( ERROR | DEBUG | STARTUP | CONNECTION );  // set before init() so that you can see startup messages
  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  //mesh.setDebugMsgTypes( ERROR );

  // Channel set to 1. Make sure to use the same channel for your mesh and for you other
  // network (STATION_SSID)
  mesh.init( MESH_PREFIX, MESH_PASSWORD, &runner, MESH_PORT);
  mesh.setRoot();
  mesh.onReceive(&receivedCallback);

  runner.addTask(taskReceiveCommand);
  runner.addTask(taskReadAll);

  taskReceiveCommand.enable();
  //taskReadAll.setTimeout(30 * TASK_SECOND);

  myNodeId = mesh.getNodeId();
  Serial.printf("My nodeId is %u\n", myNodeId);

}

void loop() {
  runner.execute();
  mesh.update();
}

void taskReceiveRequestCallback() {
  while (sSerial.available()) processIncomingByte(sSerial.read());
}

void processRequest(String jsonCommand) {
  String response;

Serial.printf("Request received: %s\n", jsonCommand.c_str());

  DynamicJsonBuffer jsonBuffer(MAX_INPUT);
  JsonObject& rcvdJson = jsonBuffer.parseObject(jsonCommand);
  if (!rcvdJson.success()) {
    Serial.println("parseObject() failed");
    sSerial.flush();
    sSerial.println("{\"status\":\"Failed\",\"reason\":\"parseObject() failed\"}");
    return;
  }

  String cmd = rcvdJson["command"];
  String scaleId = rcvdJson["scaleId"];
  uint32_t nodeId = strToUint32(scaleId);
  int weight = rcvdJson["weight"].as<int>();

  JsonObject& replJson = jsonBuffer.createObject();
  replJson["command"] = cmd;
  replJson["scaleId"] = scaleId;

  boolean reached = (myNodeId == nodeId) || isNodeReachable(nodeId);
  if (!reached) {
    replJson["status"] = "FALHOU: identificador nao encontrado";
    replJson.printTo(response);
    Serial.printf("Respondendo %s\n", response.c_str());
    sSerial.flush();
    sSerial.println(response);
    return;
  }

  replJson["status"] = "OK";
  if (myNodeId == nodeId) {         // FAZ AQUI E AGORA  :-)
    if (String("read").equals(cmd)) {
      replJson["value"] = 6.9;      // lê valor da balança local
    } else {
      // invoca o comando para a balança
    }
  } else {
    if (String("read").equals(cmd)) { // PRECISA ENVIAR E AGUARDAR RESPOSTA (receiveCallback)
    } else {                          // APENAS ENVIA
      sendCommand(nodeId, cmd, weight);
    }    
  }

  replJson.printTo(response);
  sSerial.flush();
  sSerial.println(response);

  Serial.print("scaleRootMesh enviou resposta: ");
  Serial.println(response);
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

