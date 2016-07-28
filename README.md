# NATS - Arduino Client
An Arduino / Spark Core / Particle Photon compatible C++ library for
communicating with a [NATS](http://nats.io) server.

## Features

* Header-only library
* Compatible with Ethernet and WiFi-cabable Arduinos, [Particle Photon / Spark
Core](https://www.particle.io/) devices, and even the ESP8266 (if using the
Arduino extension)
* Familiar C++ object-oriented API, similar usage to the official NATS client
APIs

## Installation
Just download `arduino-nats.h` and include it in your main `ino` file.

## API

```c
class NATS {
	typedef struct {
		const char* subject;
		const int sid;
		const char* reply;
		const char* data;
		const int size;
	} msg;

	typedef void (*sub_cb)(msg e);
	typedef void (*event_cb)();

	NATS(Client* client, const char* hostname,
		 int port = NATS_DEFAULT_PORT,
		 const char* user = NULL,
		 const char* pass = NULL);

	bool connect();			// initiate the connection
	void disconnect();k

	event_cb on_connect;    // called after NATS finishes connecting to server
	event_cb on_disconnect; // called when a disconnect happens
	event_cb on_error;		// called when an error is received

	void publish(const char* subject, const char* msg = NULL, const char* replyto = NULL);
	void publish(const char* subject, const bool msg);
	void publishf(const char* subject, const char* fmt, ...);

	int subscribe(const char* subject, sub_cb cb, const char* queue = NULL, const int max_wanted = 0);
	void unsubscribe(const int sid);

	int request(const char* subject, const char* msg, sub_cb cb, const int max_wanted = 1);

	void process();			// process pending messages from the buffer, must be called regularly in loop()
}
```

## Example
```arduino
#include "arduino-nats.h"

WiFiClient wifi;
NATS nats(&wifi, ...);

void quux_handler(NATS::msg msg) {
	// maybe blink an LED
}

void foo_handler(NATS::msg msg) {
	nats.publishf(msg.reply, "the answer is %d", 42);
}

void echo_handler(NATS::msg msg) {
	nats.publish(msg.reply, msg.data);
}

void fizzbuzz_handler(NATS::msg msg) {
	int n = atoi(msg.data);
	char buf[64];
	bool fizz = !(n % 3);
	bool buzz = !(n % 5);
	if (fizz) sprintf(buf, "fizz");
	if (buzz) sprintf(buf + (fizz * 4), "buzz");
	if (!fizz && !buzz) sprintf(buf, "%d", n);
	nats.publish(msg.reply, buf);
}

void nats_on_connect() {
	nats.publish("hello", "I'm ready!");
	nats.subscribe("foo", foo_handler);
	nats.subscribe("fizzbuzz", fizzbuzz_handler);
	nats.request("quux", quux_handler);
}

void setup() {
	wifi.connect(...);
	nats.on_connect = nats_on_connect;
	nats.connect();
}

void loop() {
	// do some stuff here
	nats.process();
}
```
