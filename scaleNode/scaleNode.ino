#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <TaskScheduler.h>


#define   MAX_LENGTH      200
#define   LED             2
#define   BLINK_DURATION  500  // milliseconds LED is on for
#define   ANDROID_IP      0xAA

// Prototypes
int readScale();
void tareScale();
void calibrateScale(int weight);
String serializeJson();
void blinkOn();
void blinkOff();
void checkWifi();
void registerScale();

typedef struct {
  String cmd;
  String scaleId;
  String status;
  int value;
} replyType;

replyType reply;


String myId;
IPAddress myIP;
String myStrIP;

IPAddress androidIP;
boolean isRegistered = false;

const char* ssid = "balancas";
const char* password = "andrepayao";

Scheduler     runner;

Task taskBlinkControl(1 * TASK_SECOND, TASK_FOREVER, &blinkOn);
Task taskCheckWifi(5 * TASK_SECOND, TASK_FOREVER, &checkWifi);
Task taskRegisterScale(5 * TASK_SECOND, TASK_FOREVER, &registerScale);

ESP8266WebServer server(80);
HTTPClient httpClient;

void setup() {

  Serial.begin(115200);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, true);

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Wait for connection
  Serial.println();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  myIP = WiFi.localIP();
  myStrIP = String(myIP[0]) + '.' + String(myIP[1]) + '.' + String(myIP[2]) + '.' + String(myIP[3]);
  myId = WiFi.macAddress();
  Serial.print("IP address: ");
  Serial.println(myIP);
  Serial.print("MAC address: ");
  Serial.println(myId);

  androidIP[0] = myIP[0] & 0xFF;
  androidIP[1] = myIP[1] & 0xFF;
  androidIP[2] = myIP[2] & 0xFF;
  androidIP[3] = ANDROID_IP;

  server.on("/", HTTP_GET, [] () {
    server.send(404, "text/plain", "Nada aqui...");
  });

  server.on("/read", HTTP_GET, [] () {
    reply.cmd = "read";
    reply.scaleId = myId;
    reply.status = "OK";
    reply.value = readScale();
    server.send(200, "text/json", serializeJson());
  });

  server.on("/tare", HTTP_GET, [] () {
    tareScale();
    reply.cmd = "tare";
    reply.scaleId = myId;
    reply.status = "OK";
    reply.value = -1;
    server.send(200, "text/json", serializeJson());
  });

  server.on("/blink", HTTP_GET, [] () {
    taskBlinkControl.enable();
    reply.cmd = "blink";
    reply.scaleId = myId;
    reply.status = "OK";
    reply.value = -1;
    server.send(200, "text/json", serializeJson());
  });

  server.on("/calibrate", HTTP_GET, [] () {
    if (server.hasArg("weight")) {
      int weight = server.arg("weight").toInt();
      calibrateScale(weight);
      reply.cmd = "calibrate";
      reply.scaleId = myId;
      reply.status = "OK";
      reply.value = -1;
      server.send(200, "text/json", serializeJson());
    } else {
      server.send(404, "text/plain", "Parametro weight faltando");      
    }
  });

  server.begin();

  runner.addTask(taskBlinkControl);
  runner.addTask(taskCheckWifi);
  runner.addTask(taskRegisterScale);

  taskCheckWifi.enable();
  // taskRegisterScale.enable();                    DESCOMENTAR QUANDO TIVER O SERVIÇO ANDROID PRONTO  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
}

void loop() {
  runner.execute();
  server.handleClient();
}

int readScale() {
  int value = 0;
  return value;
}

void tareScale() {
  
}

void calibrateScale(int weight) {
  
}

//=====================

String serializeJson() {
  String serializedJson;

  StaticJsonBuffer<MAX_LENGTH> jsonBuffer;
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

void blinkOn() {
  digitalWrite(LED, false);
  taskBlinkControl.setCallback(&blinkOff);
  taskBlinkControl.delay(BLINK_DURATION);
}

void blinkOff() {
  digitalWrite(LED, true);
  taskBlinkControl.setCallback(&blinkOn);
  taskBlinkControl.disable();
}

void checkWifi() {
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Trying WiFi connection...");
    WiFi.begin(ssid, password);
    do {
      delay(500);
      Serial.print(".");
    } while (WiFi.status() != WL_CONNECTED);
  }
}

void registerScale() {

  String strAndroidIP = String(androidIP[0]) + '.' + String(androidIP[1]) + '.' + String(androidIP[2]) + '.' + String(androidIP[3]);
  String urlAndroid = "http://";
  urlAndroid += strAndroidIP;
  urlAndroid += "/registerScale";
  httpClient.begin(urlAndroid);
  httpClient.addHeader("Content-Type", "text/json");
  String json2send = "{\"scaleId\":\"";
  json2send += myId;
  json2send += "\",\"scaleIP\":\"";
  json2send += myStrIP;
  json2send += "\"}";
  int httpCode = httpClient.POST(json2send);
  isRegistered = 200 == httpCode;
  if(isRegistered) {
    taskRegisterScale.disable();
  }

}

