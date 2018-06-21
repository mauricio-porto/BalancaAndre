#include <painlessMesh.h>

#define   MESH_SSID       "meshOne"
#define   MESH_PASSWORD   "onePassword"
#define   MESH_PORT       5555

// Prototypes
void treatRequest();
void showNodeList();
void receivedCallback(uint32_t from, String & msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback(); 
void nodeTimeAdjustedCallback(int32_t offset); 
void delayReceivedCallback(uint32_t from, int32_t delay);

Scheduler     runner;

painlessMesh  mesh;

bool calc_delay = false;
SimpleList<uint32_t> nodes;

String rcvdMsg;
String replyMsg;
uint32_t destination;

Task taskTreatRequest(100 * TASK_MILLISECOND, TASK_FOREVER, &treatRequest ); // start with a one second interval

void setup() {
  Serial.begin(115200);

  mesh.setDebugMsgTypes(ERROR | DEBUG | STARTUP| CONNECTION);  // set before init() so that you can see startup messages

  mesh.init(MESH_SSID, MESH_PASSWORD, &runner, MESH_PORT);
  mesh.setContainsRoot();
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);

  Serial.printf("\n\nMy nodeID is %u\n\n", mesh.getNodeId());

  runner.addTask(taskTreatRequest);

}

void loop() {
  runner.execute();
  mesh.update();
}

void receivedCallback(uint32_t from, String & msg) {
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  destination = from;
  rcvdMsg = msg.c_str();

  taskTreatRequest.enable();
}

// Apenas responde a pedidos de leitura da balanca
// Todas os outros são comandos que não exigem retorno/resposta
void treatRequest() {

  // DEVE TRATAR A MENSAGEM RECEBIDA E DEPOIS RETORNAR RESPOSTA
  if (rcvdMsg.indexOf("read") >= 0) {  //read?scale=987654321
    //lê valor e retorna send.single(from, PESO);
    replyMsg = "56";
    mesh.sendSingle(destination, replyMsg);
    Serial.printf("--> scaleNode: Sending message: %s\n", replyMsg.c_str());
  } else if (rcvdMsg.indexOf("tare") >= 0) {
    //
  } else if (rcvdMsg.indexOf("blink") >= 0) {
    
  } else if (rcvdMsg.indexOf("calibrate") >= 0) {
    
  }

  if (calc_delay) {
    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end()) {
      mesh.startDelayMeas(*node);
      node++;
    }
    calc_delay = false;
  }
  taskTreatRequest.disable();
}

void newConnectionCallback(uint32_t nodeId) { 
  Serial.printf("--> scaleNodeMesh: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("--> scaleNodeMesh: Changed connections %s\n", mesh.subConnectionJson().c_str());
  //showNodeList();
  calc_delay = true;
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("--> scaleNodeMesh: Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void delayReceivedCallback(uint32_t from, int32_t delay) {
  Serial.printf("--> scaleNodeMesh: Delay to node %u is %d us\n", from, delay);
}

void showNodeList() {
  nodes = mesh.getNodeList();

  Serial.printf("Num nodes: %d\n", nodes.size());
  Serial.printf("Connection list:");

  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end()) {
    Serial.printf(" %u", *node);
    node++;
  }
  Serial.println();  
}

