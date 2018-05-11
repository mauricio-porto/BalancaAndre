#include "IPAddress.h"
#include <painlessMesh.h>

#ifdef ESP8266
#include "Hash.h"
#include <ESPAsyncTCP.h>
#else
#include <AsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

#define   MESH_SSID       "meshOne"
#define   MESH_PASSWORD   "onePassword"
#define   MESH_PORT       5555

#define   STATION_SSID     "balancas"
#define   STATION_PASSWORD "andrepayao"
#define   STATION_PORT     5555
uint8_t   station_ip[4] = {192,168,222,111};

#define HOSTNAME "AGGREG01"

// Prototypes
void replyTo();
void receivedCallback(uint32_t from, String & msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback(); 
void nodeTimeAdjustedCallback(int32_t offset); 
void delayReceivedCallback(uint32_t from, int32_t delay);
IPAddress getlocalIP();

Scheduler     userScheduler;

painlessMesh  mesh;
AsyncWebServer server(80);
IPAddress myIP(0,0,0,0);
IPAddress myAPIP(0,0,0,0);

bool calc_delay = false;
SimpleList<uint32_t> nodes;

String rcvdMsg;
String replyMsg;
uint32_t destination;

Task taskReplyTo(TASK_SECOND * 1, TASK_FOREVER, &replyTo ); // start with a one second interval

void setup() {
  Serial.begin(115200);

  mesh.setDebugMsgTypes(ERROR | DEBUG | STARTUP| CONNECTION);  // set before init() so that you can see startup messages

  //mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT, STA_ONLY, WIFI_AUTH_WPA2_PSK, 1);
  mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);

  //mesh.stationManual(STATION_SSID, STATION_PASSWORD, STATION_PORT, station_ip);
  //mesh.stationManual(STATION_SSID, STATION_PASSWORD);
  //mesh.setHostname(HOSTNAME);
  myIP = IPAddress(mesh.getStationIP().addr);
  Serial.println("My Station IP is " + myAPIP.toString());
  IPAddress apIP(0,0,0,0);
  myAPIP = IPAddress(mesh.getAPIP().addr);
  Serial.println("My AP IP is " + apIP.toString());

  userScheduler.addTask(taskReplyTo);


  //Async webserver
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<form>Text to Broadcast<br><input type='text' name='BROADCAST'><br><br><input type='submit' value='Submit'></form>");
    if (request->hasArg("BROADCAST")){
      String msg = request->arg("BROADCAST");
      mesh.sendBroadcast(msg);
    }
  });
  //server.begin();
}

void loop() {
  userScheduler.execute();
  mesh.update();

  //myIP = IPAddress(mesh.getStationIP().addr);
  //Serial.println("My Station IP is " + myAPIP.toString());
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
  String cai = "Balanca mas nao cai UM!!!";
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

