#include <painlessMesh.h>

#define   MESH_SSID       "meshOne"
#define   MESH_PASSWORD   "onePassword"
#define   MESH_PORT       5555

// Prototypes
void replyTo();
void receivedCallback(uint32_t from, String & msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback(); 
void nodeTimeAdjustedCallback(int32_t offset); 
void delayReceivedCallback(uint32_t from, int32_t delay);

Scheduler     userScheduler;

painlessMesh  mesh;

bool calc_delay = false;
SimpleList<uint32_t> nodes;

String rcvdMsg;
String replyMsg;
uint32_t destination;

Task taskReplyTo(TASK_SECOND * 1, TASK_FOREVER, &replyTo ); // start with a one second interval

void setup() {
  Serial.begin(115200);

  mesh.setDebugMsgTypes(ERROR | DEBUG | STARTUP| CONNECTION);  // set before init() so that you can see startup messages

  mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);

  userScheduler.addTask(taskReplyTo);

}

void loop() {
  userScheduler.execute();
  mesh.update();
}

void receivedCallback(uint32_t from, String & msg) {
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  destination = from;
  rcvdMsg = msg.c_str();

  taskReplyTo.setInterval(random(TASK_MILLISECOND * 500, TASK_MILLISECOND * 2000));
  taskReplyTo.enable();
}

void replyTo() {
  Serial.printf("--> scaleNode: Sending message: %s\n", replyMsg.c_str());
  
  // DEVERIA TRATAR A MENSAGEM RECEBIDA E DEPOIS RETORNAR RESPOSTA
  String cai = "Balanca mas nao cai DOIS!!!";
  mesh.sendSingle(destination, cai);
  mesh.sendSingle(destination, rcvdMsg);

  if (calc_delay) {
    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end()) {
      mesh.startDelayMeas(*node);
      node++;
    }
    calc_delay = false;
  }
  taskReplyTo.disable();
}

void newConnectionCallback(uint32_t nodeId) { 
  Serial.printf("--> scaleNode: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("--> scaleNode: Changed connections %s\n", mesh.subConnectionJson().c_str());
 
  nodes = mesh.getNodeList();

  Serial.printf("Num nodes: %d\n", nodes.size());
  Serial.printf("Connection list:");

  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end()) {
    Serial.printf(" %u", *node);
    node++;
  }
  Serial.println();
  calc_delay = true;
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("--> scaleNode: Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void delayReceivedCallback(uint32_t from, int32_t delay) {
  Serial.printf("--> scaleNode: Delay to node %u is %d us\n", from, delay);
}

