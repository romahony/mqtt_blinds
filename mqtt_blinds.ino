/*
 *  Contains code from: 
 *  https://github.com/jjhtpc/esp8266MQTTBlinds
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Servo.h>
#include <ArduinoOTA.h>

MDNSResponder mdns;
ESP8266WebServer server(80);

#include <PubSubClient.h>

#define wifi_ssid "<SSID>" // Enter your WIFI SSID
#define wifi_password "<PASSWORD>" // Enter your WIFI Password

#define mqtt_server "<MQTT IP>" // Enter your MQTT server IP. 
#define mqtt_user "<MQTT USER>" // Enter your MQTT username
#define mqtt_password "<MQTT PASS>" // Enter your MQTT password

WiFiClient espClient;
PubSubClient client(espClient);
Servo myservo;

int val;
int cur_val;
int pos;
char msg[3];
int level;
int cur_off; 
int itsatrap = 0;
int servoPin = D3;
int max_angle = 133;


void setup() {
  Serial.begin(115200);

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  
  client.setCallback(callback);

}


void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    Serial.print(".");
  }

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if (mdns.begin("Blinds", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }
  
}

void callback(char* topic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;
    String message(p);
    String mytopic(topic);
    if (mytopic == "blinds/level/state") {
          cur_val = message.toInt();
          cur_val = map (cur_val, 0, 100, max_angle, 0);
    }
    if (itsatrap == 0 && mytopic == "blinds/cmd" && message.equalsIgnoreCase("on")) {
      myservo.attach(servoPin);
      delay(100);
      for (pos = max_angle; pos >= cur_val; pos -= 1) {
        myservo.write(pos);
        delay(20);
      }
    myservo.detach();
    client.publish("blinds/cmd/state", "on", true);
    cur_off = 0;
    delay(1000);
    }
    else if (mytopic == "blinds/cmd" && message.equalsIgnoreCase("off")) {
      myservo.attach(servoPin);
      delay(100);
      for (pos = cur_val; pos <= max_angle; pos += 1) {
        myservo.write(pos);
        delay(20);
      }
    myservo.detach();
    client.publish("blinds/cmd/state", "off", true);
    client.publish("blinds/level/state", "100", true); // Comment out if you want the blinds to retain the last position, otherwise they will open to 100% when turned on again.
    cur_off = 1;
    delay(1000); 
    }
    else if (mytopic == "blinds/level") {
      myservo.attach(servoPin);
      delay(100);
      val = message.toInt();
      level = val;
      val = map (val, 0, 100, max_angle, 0);
      if (cur_off == 1) {
        for (pos = max_angle; pos >= val; pos -= 1) {
          myservo.write(pos);
          delay(20);
        }
        cur_off = 0;
      } else if (cur_val > val) {
        for (pos = cur_val; pos >= val; pos -= 1) {
          myservo.write(pos);
          delay(20);
        }
      } else {
        for (pos = cur_val; pos <= val; pos += 1) {
          myservo.write(pos);
          delay(20);
        }
      }
    myservo.detach();
    sprintf(msg, "%ld", level);
    client.publish("blinds/level/state", msg, true);
    client.publish("blinds/cmd/state", "on", true);  
    itsatrap = 1;
    delay(1000);
    }
    else {
      itsatrap = 0;
    }
}


void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESPBlindstl", mqtt_user, mqtt_password)) {
      Serial.println("connected");

      client.subscribe("blinds/level/state");
      client.subscribe("blinds/cmd/state");
      client.subscribe("blinds/level");
      client.subscribe("blinds/cmd");

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
