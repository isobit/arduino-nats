# NATS - Arduino Client
An Arduino / Spark Core / Particle Photon compatible C++ library for
communicating with a [NATS](http://nats.io) server.

## Installation
Just download `nats.hpp` and include it in your main `ino` file.

## Example
```arduino
NATS nats("mynatshost.com", NATS_DEFAULT_PORT);

void foo_handler(const char* msg, const char* reply) {
	nats.publish(reply, "bar");
}

void setup() {
	connect_wifi();

	nats.connect();
	nats.publish("hello", "I'm ready!");
	nats.subscribe("foo", foo_handler);
}

void loop() {
	if (WiFi.status() != WL_CONNECTED) connect_wifi();
	nats.process();

	// do some stuff

	delay(500);
}
```
