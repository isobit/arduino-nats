#ifndef NATS_H
#define NATS_H
#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#define TCPClient WiFiClient
#elif defined(SPARK)
#include "application.h"
#endif

#define NATS_CONF_VERBOSE false
#define NATS_CONF_PEDANTIC false
#define NATS_CONF_LANG "photon-cpp"

#define NATS_DEFAULT_PORT 4222

#define NATS_PING_INTERVAL 120000UL
#define NATS_RECONNECT_INTERVAL 5000UL

#define NATS_MAX_OUTSTANDING_PINGS 2

#define NATS_MAX_SUBS 10
#define NATS_MAX_BUFSIZE 128
#define NATS_MAX_ARGV 5

#define NATS_CR_LF "\r\n"
#define NATS_CTRL_MSG "MSG"
#define NATS_CTRL_OK "+OK"
#define NATS_CTRL_ERR "-ERR"
#define NATS_CTRL_PING "PING"
#define NATS_CTRL_PONG "PONG"
#define NATS_CTRL_INFO "INFO"

namespace NATSUtil {
	class Timer {
		const unsigned long interval;
		unsigned long t;
		public:
		Timer(const unsigned long interval) :
			interval(interval) {}
		bool process() {
			unsigned long ms = millis();
			if (ms < t ||
					ms - t > interval) {
				t = ms;
				return true;
			}
			return false;
		}
	};
};

typedef void (*nats_sub_cb)(const char* msg, const char* reply);

class NATS {
	private:

	// Members --------------------------------------------- //

	const char* hostname;
	const int port;
	const char* user = NULL;
	const char* pass = NULL;

	TCPClient client;

	nats_sub_cb sub_callbacks[NATS_MAX_SUBS];
	const char* sub_subjects[NATS_MAX_SUBS];
	int sub_size = 0;

	NATSUtil::Timer ping_timer;
	NATSUtil::Timer reconnect_timer;

	int outstanding_pings = 0;
	int reconnect_attempts = 0;

	// Methods --------------------------------------------- //

	void send(const char* msg) {
		client.print(msg);
		client.print(NATS_CR_LF);
	}

	bool client_readline(char* buf) {
		int i = 0;
		while (client.available()) {
			char c = client.read();
			if (c == '\r') continue;
			if (c == '\n') break;
			if (c == -1) break;
			if (i >= NATS_MAX_BUFSIZE) return false;
			buf[i++] = c;
		}
		buf[i] = '\0';
		return true;
	}

	bool recv() {
		// read line from client
		char buf[NATS_MAX_BUFSIZE];
		if (!client_readline(buf)) return false;

		// tokenize line by space
		size_t argc = 0;
		const char* argv[NATS_MAX_ARGV] = {};
		for (int i = 0; i < NATS_MAX_ARGV; i++) {
			argv[i] = strtok((i == 0) ? buf : NULL, " ");
			if (argv[i] == NULL) break;
			argc++;
		}
		if (argc == 0) return false;

		// switch off of control keyword 
		if (strcmp(argv[0], NATS_CTRL_MSG) == 0) {
			// sanity check
			if (argc != 4 && argc != 5) return false;

			// get subscription id
			int sid = atoi(argv[2]);

			// make sure sub_callback for sid is not null
			if (sub_callbacks[sid] == NULL) return false;

			// receive payload
			char payload_buf[atoi(argv[4])];
			if (!client_readline(payload_buf)) return false;

			// call callback
			sub_callbacks[sid](payload_buf, (argc == 5)? argv[3] : "");

			return true;
		}
		if (strcmp(argv[0], NATS_CTRL_OK) == 0) {
			return true;
		}
		if (strcmp(argv[0], NATS_CTRL_ERR) == 0) {
			return true;
		}
		if (strcmp(argv[0], NATS_CTRL_PING) == 0) {
			send(NATS_CTRL_PONG);
			return true;
		}
		if (strcmp(argv[0], NATS_CTRL_PONG) == 0) {
			outstanding_pings--;
			return true;
		}
		if (strcmp(argv[0], NATS_CTRL_INFO) == 0) {
			return true;
		}
		return false;
	}

	void ping() {
		if (outstanding_pings > NATS_MAX_OUTSTANDING_PINGS) {
			client.stop();
			return;
		}
		outstanding_pings++;
		send(NATS_CTRL_PING);
	}

	bool reconnect() {
		if (connect()) {
			for (int i = 0; i < sub_size; i++) {
				char send_buf[NATS_MAX_BUFSIZE];
				sprintf(send_buf, "SUB %s %s %d", sub_subjects[i], "", i);
				send(send_buf);
			}
			return true;
		}
		return false;
	}

	public:

	NATS(const char* hostname, int port) :
		hostname(hostname),
		port(port),
		ping_timer(NATS_PING_INTERVAL),
		reconnect_timer(NATS_RECONNECT_INTERVAL)
		{}
	NATS(const char* hostname, int port, const char* user, const char* pass) :
		hostname(hostname),
		port(port),
		user(user),
		pass(pass),
		ping_timer(NATS_PING_INTERVAL),
		reconnect_timer(NATS_RECONNECT_INTERVAL)
		{}

	bool connect() {
		if (client.connect(hostname, port)) {
			outstanding_pings = 0;
			reconnect_attempts = 0;
			char send_buf[NATS_MAX_BUFSIZE];
			sprintf(send_buf, 
					"CONNECT {\"verbose\": %s,\"pedantic\": %s,\"lang\": \"%s\",\"user\":\"%s\",\"pass\":\"%s\"}", 
					NATS_CONF_VERBOSE? "true" : "false",
					NATS_CONF_PEDANTIC? "true" : "false",
					NATS_CONF_LANG,
					(user == NULL)? "null" : user,
					(pass == NULL)? "null" : pass);
			send(send_buf);
			return true;
		}
		return false;
	}

	bool connected() {
		return client.connected();
	}

	bool publish(const char* subject, const char* msg) {
		char send_buf[NATS_MAX_BUFSIZE];
		sprintf(send_buf, "PUB %s %lu", 
				subject, (unsigned long)strlen(msg));
		send(send_buf);
		send(msg);
		return true;
	}
	bool publish(const char* subject) {
		return publish(subject, "");
	}

	int subscribe(const char* subject, nats_sub_cb cb, const char* queue) {
		int sid = sub_size++;
		if (sid > NATS_MAX_SUBS) return false;
		sub_callbacks[sid] = cb;
		sub_subjects[sid] = subject;

		char send_buf[NATS_MAX_BUFSIZE];
		sprintf(send_buf, "SUB %s %s %d", subject, queue, sid);
		send(send_buf);

		return sid;
	}
	int subscribe(const char* subject, nats_sub_cb cb) {
		return subscribe(subject, cb, "");
	}

	void process() {
		if (client.connected()) {
			if (client.available())
				recv();
			if (ping_timer.process())
				ping();
		} else {
			if (reconnect_timer.process())
				reconnect();
		}
	}

};

#endif
