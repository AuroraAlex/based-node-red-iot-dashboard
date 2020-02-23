#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Arduino_JSON.h>
#include <PubSubClient.h>
#include "DHTesp.h"

#ifndef STASSID
#define STASSID ""
#define STAPSK  ""
#endif

#define DHT11_GPIO 5home/dht

int count = 0;
boolean isConnectToMqtt ;
String publishData = "";

typedef struct SysConfig
{
  char ssid[32];
  char password[32];
  char mqttServer[64] = "";
  char mqttClientId[64] = "nodemcu";
  char mqttUsername[64] = "";
  char mqttPassword[64] = "";
  int mqttPort = 1883;
};

SysConfig sysConfig;

const char* ssid = STASSID;
const char* password = STAPSK;

ESP8266WebServer server(80);

const int led = 13;
const int LED = 4;
const byte DNS_PORT = 53; //DNS服务端口号，一般为53
WiFiClient espClient;
PubSubClient mqttClient("mqttbroker.janelive.cn", 1883, espClient);
DHTesp dht;

/*
 * 根目录
 */
void handleRoot() {
  digitalWrite(led, 1);
  server.send(200, "text/plain", "hello from esp8266!");
  digitalWrite(led, 0);
}

/*
 * 404
 */
void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

/*
 * 配置mdns
 */
void mdnsConfig(void) {
   if (MDNS.begin("myrelay")) {
    Serial.println("MDNS responder started");
  }
  
  server.on("/", handleRoot);

  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.on("/on", []() {
    digitalWrite(LED, 1);
    server.send(200, "text/plain", "LED_ON");
    Serial.println("LED_ON");
  });

  server.on("/off", []() {
    digitalWrite(LED, 0);
    server.send(200, "text/plain", "LED_OFF");
    Serial.println("LED_OFF");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

/*
 * 初始化mqtt
 */
void initMqtt() {
  Serial.println("-----------------SetUp mqtt-------------------");
  mqttClient.setCallback([](char* topic, byte * payload, unsigned int length)-> void{
    String msg ;
    for (int i = 0 ; i < length; i++) {
      msg += (char )payload[i];
    }
    JSONVar dataJson = JSON.parse(msg);
    if (JSON.typeof(dataJson) == "undefined") {
      Serial.println("Parsing input failed!");
      return;
    } else{
      Serial.println(dataJson);

    }


  });
  for (int i = 0 ; i < 10; i++) {
//    mqttClient.connect("NodeMcu10010");
    mqttClient.connect(sysConfig.mqttClientId, sysConfig.mqttUsername, sysConfig.mqttPassword);
    Serial.print("=>");
    delay(1000);
    if (mqttClient.connected()) {
      //连接成功
      mqttClient.subscribe(String("home/switchRec").c_str(),2);
      isConnectToMqtt = true;
      Serial.println("\n|Connect to mqtt success!");
      break;
    }
  }
  if (!mqttClient.connected() ) {
    isConnectToMqtt = false;
    Serial.println("\n|Connect to mqtt failure.Please try again!");
  }

}

void setup(void) {
  pinMode(led, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(led, 0);
  digitalWrite(LED, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  dht.setup(DHT11_GPIO, DHTesp::DHT11);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  mdnsConfig();
  initMqtt();
  dht.setup(DHT11_GPIO, DHTesp::DHT11);
}

/*
 *  读取DHT11
*/
JSONVar getDHT11Data() {
  JSONVar data;
  delay(dht.getMinimumSamplingPeriod());
  //湿度
  float h = dht.getHumidity();
  //摄氏度
  float c = dht.getTemperature();
  //热指数
  float cc = dht.computeHeatIndex(c, h, true);

  data["Humidity"] = h;
  data["Temperature"] = c;
  data["HeatIndex"] = cc;
  return data;
}

/*
 * json 转换
 */
JSONVar jsonDataPackage(JSONVar j2strData)
{
  JSONVar jsonData;
  jsonData["clientId"] = "dht11";
  //jsonData["value"] = JSON.stringify(j2strData);
  jsonData["value"] = j2strData;
  
  
  return jsonData;
}




void loop(void) {
  server.handleClient();
  MDNS.update();
  count += 1;
  if (isConnectToMqtt) {
    mqttClient.loop();
    if (count >= 100000) {
      publishData = JSON.stringify(jsonDataPackage(getDHT11Data()));
      mqttClient.beginPublish("home/dht",strlen(publishData.c_str()),false);
      mqttClient.print(publishData.c_str());
      mqttClient.endPublish();
      count = 0;
    }
  }
}
