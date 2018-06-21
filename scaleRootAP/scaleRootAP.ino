#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>

#define   STATION_SSID     "balancas"
#define   STATION_PASSWORD "andrepayao"
#define   STATION_PORT     5555

const unsigned int MAX_INPUT = 512;
long reqNumber = 0;

// Prototypes
String processCommand(String scaleID, String cmd, int weight = 0);
String readString(SoftwareSerial *sSer);

ESP8266WebServer server(80);
IPAddress myIP(0,0,0,0);
IPAddress myAPIP(0,0,0,0);

SoftwareSerial sSerial(13, 15, false, MAX_INPUT); // GPIO13 - RX, GPIO15 - TX (aka D7, D8)

void setup() {
  Serial.begin(115200);

  // A SoftwareSerial em ESP8266 junto com Mesh não suporta velocidade alta, por isso 38400
  sSerial.begin(38400);
  while(sSerial.available()) sSerial.read();  // Clear buffer

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

  server.on("/", HTTP_GET, [] () {
    server.send(404, "text/plain", "Nada aqui...");
  });

  server.on("/readAll", HTTP_GET, [] () {
    server.send(200, "text/json", readAll());
  });

  server.on("/read", HTTP_GET, [] () {
    if (server.hasArg("scale")) {
      String scaleId = server.arg("scale");
      server.send(200, "text/json", processCommand(scaleId, "read"));
    } else {
      server.send(404, "text/plain", "Parametro scale faltando");
    }
  });

  server.on("/tare", HTTP_GET, [] () {
    if (server.hasArg("scale")) {
      String scaleId = server.arg("scale");
      server.send(200, "text/json", processCommand(scaleId, "tare"));
    } else {
      server.send(404, "text/plain", "Parametro scale faltando");
    }
  });

  server.on("/blink", HTTP_GET, [] () {
    if (server.hasArg("scale")) {
      String scaleId = server.arg("scale");
      server.send(200, "text/json", processCommand(scaleId, "blink"));
    } else {
      server.send(404, "text/plain", "Parametro scale faltando");
    }
  });

  server.on("/calibrate", HTTP_GET, [] () {
    if (server.hasArg("scale")) {
      String scaleId = server.arg("scale");
      if (server.hasArg("weight")) {
        int weight = server.arg("weight").toInt();
        server.send(200, "text/json", processCommand(scaleId, "calibrate", weight));
      } else {
        server.send(404, "text/plain", "Parametro weight faltando");      
      }
    } else {
      server.send(404, "text/plain", "Parametro scale faltando");
    }
  });

  server.begin();
  delay(3333);
  processCommand("9876543210", "read", 0); // Apenas para "limpar" a garganta ;-) Por razão desconhecida, o primeiro buffer fica corrompido.

}

void loop() {
  server.handleClient();
}

//=====================

String processCommand(String scaleID, String cmd, int weight) {
  String command;

  DynamicJsonBuffer jsonBuffer(MAX_INPUT);
  JsonObject& root = jsonBuffer.createObject();
  root["command"] = cmd;
  root["scaleId"] = scaleID;
  root["weight"] = weight;

  root.printTo(command);

  sSerial.flush();
  sSerial.println(command);
  Serial.print("scaleRootAP: ");
  Serial.println(command);

  int retries = 5;
  while (!sSerial.available() && retries-- > 0) {
    delay(1000);
  }
  if (sSerial.available()) {
    String response = readString(&sSerial);
    Serial.printf("Recebido pela sSerial: %s\n", response.c_str());
    return response;
  } else {
    return "{\"status\":\"Failed, timeout.\"}";
  }

}

String readAll() {
  String response;

  DynamicJsonBuffer jsonBuffer(MAX_INPUT);
  JsonObject& root = jsonBuffer.createObject();
  root["response"] = "readAll";
  root["status"] = "OK";
  root["scales"] = 50;
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

String readString(SoftwareSerial *sSer) {
  String response;

  static char input_line [MAX_INPUT];
  static unsigned int input_pos = 0;

  boolean eol = false;
  while(sSer->available() && !eol) {
    byte inByte = sSer->read();
    switch (inByte) {
      case '\n':   // end of text
        input_line [input_pos] = 0;  // terminating null byte
        input_pos = 0;
        eol = true;
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
  return String(input_line);
}

