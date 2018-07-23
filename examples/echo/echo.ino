#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoNATS.h>

const char* WIFI_SSID = "Internet";
const char* WIFI_PSK = "password";

WiFiClient client;
NATS nats(
	&client,
	"demo.nats.io", NATS_DEFAULT_PORT
);

void connect_wifi() {
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PSK);
	while (WiFi.status() != WL_CONNECTED) {
		yield();
	}
}

void nats_echo_handler(NATS::msg msg) {
	nats.publish(msg.reply, msg.data);
}

void nats_on_connect() {
	nats.subscribe("echo", nats_echo_handler);
}

void setup() {
	connect_wifi();

	nats.on_connect = nats_on_connect;
	nats.connect();
}

void loop() {
	if (WiFi.status() != WL_CONNECTED) connect_wifi();
	nats.process();
	yield();
}
