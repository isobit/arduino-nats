# NATS - Arduino Client
An Arduino / Spark Core / Particle Photon compatible C++ library for
communicating with a [NATS](http://nats.io) server.

## Features

* Header-only library
* Compatible with WiFi-cabable Arduinos, [Particle Photon / Spark
Core](https://www.particle.io/) devices, and even the ESP8266 (if using the
Arduino extension)
* Familiar C++ object-oriented API, similar usage to the official NATS client
APIs

## Installation
Just download `nats.hpp` and include it in your main `ino` file.

## API

```c
class NATS {
	typedef struct {
		const char* subject;
		const int sid;
		const char* reply;
		const char* msg;
	} msg_event;

	typedef void (*sub_cb)(msg_event e);
	typedef void (*event_cb)();

	NATS(const char* hostname, 
		 int port = NATS_DEFAULT_PORT, 
		 const char* user = NULL, 
		 const char* pass = NULL);

	bool connect();
	void disconnect();

	void publish(const char* subject, const char* msg = NULL, const char* replyto = NULL);
	void publish(const char* subject, const bool msg);
	void publish(const char* subject, const int msg);
	void publish(const char* subject, const long msg);
	void publish(const char* subject, const float msg);
	void publish(const char* subject, const double msg);

	int subscribe(const char* subject, sub_cb cb, const char* queue = NULL, const int max = 0);
	void unsubscribe(const int sid);

	int request(const char* subject, const char* msg, sub_cb cb, const int max = 1);

	void process();
}
```

## Example
```arduino
#include "nats.hpp"

NATS nats("12.34.56.78");

void quux_handler(NATS::msg_event e) {
	// maybe blink an LED
}

void foo_handler(NATS::msg_event e) {
	nats.publish(e.reply, "bar");
}

void echo_handler(NATS::msg_event e) {
	nats.publish(e.reply, e.msg);
}

void fizzbuzz_handler(NATS::msg_event e) {
	int n = atoi(e.msg);
	char buf[64];
	bool fizz = !(n % 3);
	bool buzz = !(n % 5);
	if (fizz) sprintf(buf, "fizz");
	if (buzz) sprintf(buf + (fizz * 4), "buzz");
	if (!fizz && !buzz) sprintf(buf, "%d", n);
	nats.publish(e.reply, buf);
}

void setup() {
	// connect to wifi

	nats.connect();
	nats.publish("hello", "I'm ready!");
	nats.subscribe("foo", foo_handler);
	nats.subscribe("fizzbuzz", fizzbuzz_handler);
	nats.request("quux", quux_handler);
}

void loop() {
	nats.process();
	// do some stuff
}
```
