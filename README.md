# NATS - Arduino Client
An Arduino / Spark Core / Particle Photon compatible C++ library for
communicating with a [NATS](http://nats.io) server.

## Features

* Header-only library
* Compatible with WiFi-cabable Arduinos, [Particle Photon / Spark
Core](https://www.particle.io/) devices, and even the ESP8266 (if using the
Arduino extension). (automagically detected from available headers)
* Familiar C++ object-oriented API, similar usage to the official NATS client
APIs

## Installation
Just download `nats.hpp` and include it in your main `ino` file.

## Example
```arduino
#include "nats.hpp"

NATS nats("mynatshost.com", NATS_DEFAULT_PORT);

void foo_handler(const char* msg, const char* reply) {
	nats.publish(reply, "bar");
}

void connect_wifi() {
	...
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
