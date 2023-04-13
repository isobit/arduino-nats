#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoNATS.h>

#define SSID your_wifi_ssid
#define PSWD your_wifi_pswd
#define NATS_URL_SERVER "192.168.0.XXX"

WiFiClient client;
NATS nats(&client, NATS_URL_SERVER, NATS_DEFAULT_PORT);

void connect_wifi() {
  WiFi.begin(SSID, PSWD);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
}

void nats_foo_handler(NATS::msg msg) {
  Serial.print((String)"[]" + msg.data); 
  device.msgHandler(msg);

  if (msg.reply != NULL)
    msg.respond("HaLLo !!!")
}

void nats_on_connect() {
  Serial.println("NATS connected. Device topic: \"foo\"");
  nats->subscribe("foo", nats_foo_handler);
}

void setup() {
  Serial.begin(115200);
  connect_wifi()

  nats->on_connect = nats_on_connect;
  nats->connect();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connect_wifi();
  nats->process();
}