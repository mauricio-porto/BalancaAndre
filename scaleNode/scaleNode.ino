#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <TaskScheduler.h>
#include <HX711.h>
#include <EEPROM.h>

#define   MAX_LENGTH      200
#define   LED             13   // pin GPIO13 aka D7
#define   BLINK_DURATION  500  // milliseconds LED is on for
#define   ANDROID_IP      170  // 0xAA

#define   HX711_DOUT      12  // HX711.DOUT  - pin GPIO12 aka D6
#define   HX711_SCK       14  // HX711.PD_SCK  - pin GPIO14 aka D5

float scale_fix = -11800.f;   // this value is obtained by calibrating the scale with known weights; see the README for details

// Prototypes
int readScale();
void calibrateScale(int weight);
void resetScale();
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
HX711 scale;

boolean waitingCalibration;

void setup() {

  Serial.begin(115200);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, false);

  scale.begin(HX711_DOUT, HX711_SCK);     // D6, D5

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
    if (waitingCalibration) {
      resetScale();
    }
    server.send(404, "text/plain", "Nada aqui...");
  });

  server.on("/read", HTTP_GET, [] () {
    if (waitingCalibration) {
      resetScale();
    }

    reply.cmd = "read";
    reply.scaleId = myId;
    reply.status = "OK";
    reply.value = readScale();
    reply.value = reply.value >= 0?reply.value:0;
    server.send(200, "text/json", serializeJson());
  });

  server.on("/tare", HTTP_GET, [] () {
    if (waitingCalibration) {
      resetScale();
    }

    scale.power_up();
    scale.tare();
    scale.power_down();

    reply.cmd = "tare";
    reply.scaleId = myId;
    reply.status = "OK";
    reply.value = -1;
    server.send(200, "text/json", serializeJson());
  });

  server.on("/blink", HTTP_GET, [] () {
    if (waitingCalibration) {
      resetScale();
    }
    taskBlinkControl.enable();
    reply.cmd = "blink";
    reply.scaleId = myId;
    reply.status = "OK";
    reply.value = -1;
    server.send(200, "text/json", serializeJson());
  });

  server.on("/prepareCalibration", HTTP_GET, [] () {

    scale.power_up();
    scale.set_scale();
    scale.tare();
    scale.power_down();
    waitingCalibration = true;

    reply.cmd = "prepareCalibration";
    reply.scaleId = myId;
    reply.status = "OK, coloque o peso conhecido na balanca e chame calibrate a seguir";
    reply.value = -1;
    server.send(200, "text/json", serializeJson());
  });

  server.on("/calibrate", HTTP_GET, [] () {
    if (waitingCalibration) {
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
    } else {
        server.send(404, "text/plain", "Precisa chamar prepareCalibration antes");
    }
  });

  server.begin();

  digitalWrite(LED, true); delay(BLINK_DURATION); digitalWrite(LED, false); delay(BLINK_DURATION); digitalWrite(LED, true); delay(BLINK_DURATION); digitalWrite(LED, false);

  runner.addTask(taskBlinkControl);
  runner.addTask(taskCheckWifi);
  runner.addTask(taskRegisterScale);

  scale.set_scale(scale_fix);
  scale.tare();                                     // A BALANÇA DEVE ESTAR VAZIA, LIMPA
  scale.power_down();

  taskCheckWifi.enable();
  taskRegisterScale.enable();
}

void loop() {
  runner.execute();
  server.handleClient();
}

int readScale() {
  int value = 0;

  scale.power_up();
  delay(500);
  float w = scale.get_units(10);
  value = (int) (w * 10.f);
  scale.power_down();

  return value;
}

void calibrateScale(int weight) {

  float w = (float) weight / 10.f;

  scale.power_up();
  float u = scale.get_units(10);
  scale_fix = u / w;
  scale.set_scale(scale_fix);
  scale.power_down();
  waitingCalibration = false;

}

void resetScale() {
  scale.power_up();
  scale.set_scale(scale_fix);
  scale.tare();
  scale.power_down();  
  waitingCalibration = false;
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
  digitalWrite(LED, true);
  taskBlinkControl.setCallback(&blinkOff);
  taskBlinkControl.delay(BLINK_DURATION);
}

void blinkOff() {
  digitalWrite(LED, false);
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
  urlAndroid += ":8000/registerScale";
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
    Serial.println("\n\nBalanca registrada!\n\n");
  }

}

