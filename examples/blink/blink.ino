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

void nats_blink_handler(NATS::msg msg) {
	int count = atoi(msg.data);
	while (count-- > 0) {
		digitalWrite(LED_BUILTIN, LOW);
		delay(100);
		digitalWrite(LED_BUILTIN, HIGH);
		delay(100);
	}
}

void nats_on_connect() {
	nats.subscribe("blink", nats_blink_handler);
}

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	connect_wifi();

	nats.on_connect = nats_on_connect;
	nats.connect();
}

void loop() {
	if (WiFi.status() != WL_CONNECTED) connect_wifi();
	nats.process();
	yield();
}
