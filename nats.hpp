#ifndef NATS_H
#define NATS_H
#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#define TCPClient WiFiClient
#elif defined(SPARK)
#include "application.h"
#endif

#define NATS_CLIENT_LANG "photon-cpp"
#define NATS_CLIENT_VERSION "0.1.1"

#ifndef NATS_CONF_VERBOSE
#define NATS_CONF_VERBOSE false
#endif

#ifndef NATS_CONF_PEDANTIC
#define NATS_CONF_PEDANTIC false
#endif

#define NATS_DEFAULT_PORT 4222

#ifndef NATS_PING_INTERVAL
#define NATS_PING_INTERVAL 120000UL
#endif

#ifndef NATS_RECONNECT_INTERVAL
#define NATS_RECONNECT_INTERVAL 5000UL
#endif

#define NATS_MAX_ARGV 5

#define NATS_CR_LF "\r\n"
#define NATS_CTRL_MSG "MSG"
#define NATS_CTRL_OK "+OK"
#define NATS_CTRL_ERR "-ERR"
#define NATS_CTRL_PING "PING"
#define NATS_CTRL_PONG "PONG"
#define NATS_CTRL_INFO "INFO"

namespace NATSUtil {

	class MillisTimer {
		const unsigned long interval;
		unsigned long t;
		public:
		MillisTimer(const unsigned long interval) :
			interval(interval) {}
		bool process() {
			unsigned long ms = millis();
			if (ms < t || (ms - t) > interval) {
				t = ms;
				return true;
			}
			return false;
		}
	};

	template <typename T>
	class Array {
		private:

			T* data;
			size_t len;
			size_t cap;

		public:

			Array(size_t cap = 32) : len(0), cap(cap) {
				data = (T*)malloc(cap * sizeof(T));
			}

			~Array() {
				free(data);
			}

		private:

			void resize() {
				if (cap == 0) cap = 1;
				else cap *= 2;
				data = (T*)realloc(data, cap * sizeof(T));
			}

		public:

			size_t const size() const { return len; };

			void erase(size_t idx) {
				for (size_t i = idx; i < len; i++) {
					data[i] = data[i+1];
				}
				len--;
			}

			void empty() {
				len = 0;
				cap = 32;
				free(data);
				data = (T*)malloc(cap * sizeof(T));
			}

			T const& operator[](size_t i) const {
				return data[i];
			}

			T& operator[](size_t i) {
				while (i >= cap) resize();
				return data[i];
			}

			size_t push_back(T v) {
				size_t i = len++;
				if (len > cap) resize();
				data[i] = v;
				return i;
			}

			T* ptr() { return data; }

	};

	template <typename T>
		class Queue {

			private:
				class Node {
					public:
						T data;
						Node* next;
						Node(T data, Node* next = NULL) : data(data), next(next) {}
				};
				Node* root;
				size_t len;

			public:
				Queue() : root(NULL), len(0) {}
				~Queue() {
					Node* tmp;
					Node* n = root;
					while (n != NULL) {
						tmp = n->next;
						free(n);
						n = tmp;
					}
				}
				bool empty() const { return root == NULL; }
				size_t const size() const { return len; }
				void push(T data) {
					root = new Node(data, root);
					len++;
				}
				T pop() {
					Node n = *root;
					free(root);
					root = n.next;
					len--;
					return n.data;
				}
				T peek() {
					return root->data;
				}
		};

};

class NATS {

	// Structures/Types ----------------------------------------- //

	public:

		typedef struct {
			const char* subject;
			const int sid;
			const char* reply;
			const char* msg;
		} msg_event;

	private:

		typedef void (*sub_cb)(msg_event e);
		typedef void (*event_cb)();

		class Sub {
			public:
			sub_cb cb;
			int received;
			int max;
			Sub(sub_cb cb, int max = 0) :
				cb(cb), received(0), max(max) {}
			void call(msg_event& e) {
				received++;
				cb(e);
			}
			bool maxed() {
				return (max == 0)? false : received >= max;
			}
		};

	// Members --------------------------------------------- //
	
	private:

		const char* hostname;
		const int port;
		const char* user;
		const char* pass;

		TCPClient client;

		NATSUtil::Array<Sub*> subs;
		NATSUtil::Queue<size_t> free_sids;

		NATSUtil::MillisTimer ping_timer;
		NATSUtil::MillisTimer reconnect_timer;

		int outstanding_pings;
		int reconnect_attempts;

	public:

		bool connected;

		int max_outstanding_pings;
		int max_reconnect_attempts;

		event_cb on_connect;
		event_cb on_disconnect;
		event_cb on_error;

	// Constructor ----------------------------------------- //
	
	public:

		NATS(const char* hostname, 
				int port = NATS_DEFAULT_PORT, 
				const char* user = NULL, 
				const char* pass = NULL) :
			hostname(hostname),
			port(port),
			user(user),
			pass(pass),
			ping_timer(NATS_PING_INTERVAL),
			reconnect_timer(NATS_RECONNECT_INTERVAL),
			outstanding_pings(0),
			reconnect_attempts(0),
			connected(false),
			max_outstanding_pings(3),
			max_reconnect_attempts(-1),
			on_connect(NULL),
			on_disconnect(NULL),
			on_error(NULL) {
			}

	// Methods --------------------------------------------- //

	private:

		void send(const char* msg) {
			if (msg == NULL) return;
			client.println(msg);
		}

		void send_fmt(const char* fmt, ...) {
			va_list args;
			va_start(args, fmt);
			char* buf;
			vasprintf(&buf, fmt, args);
			va_end(args);
			send(buf);
			free(buf);
		}

		void send_connect() {
			send_fmt(
					"CONNECT {"
					"\"verbose\": %s,"
					"\"pedantic\": %s,"
					"\"lang\": \"%s\","
					"\"version\": \"%s\","
					"\"user\":\"%s\","
					"\"pass\":\"%s\""
					"}", 
					NATS_CONF_VERBOSE? "true" : "false",
					NATS_CONF_PEDANTIC? "true" : "false",
					NATS_CLIENT_LANG,
					NATS_CLIENT_VERSION,
					(user == NULL)? "null" : user,
					(pass == NULL)? "null" : pass);
		}

		char* client_readline(size_t cap = 128) {
			char* buf = (char*)malloc(cap * sizeof(char));
			int i;
			for (i = 0; client.available();) {
				char c = client.read();
				if (c == '\r') continue;
				if (c == '\n') break;
				if (c == -1) break;
				if (i >= cap) buf = (char*)realloc(buf, (cap *= 2) * sizeof(char));
				buf[i++] = c;
			}
			buf[i] = '\0';
			return buf;
		}

		void recv() {
			// read line from client
			char* buf = client_readline();

			// tokenize line by space
			size_t argc = 0;
			const char* argv[NATS_MAX_ARGV] = {};
			for (int i = 0; i < NATS_MAX_ARGV; i++) {
				argv[i] = strtok((i == 0) ? buf : NULL, " ");
				if (argv[i] == NULL) break;
				argc++;
			}

			// switch off of control keyword 
			if (argc == 0) {}
			else if (strcmp(argv[0], NATS_CTRL_MSG) == 0) {
				// sanity check
				if (argc != 4 && argc != 5) { free(buf); return; }

				// get subscription id
				int sid = atoi(argv[2]);

				// make sure sub for sid is not null
				if (subs[sid] == NULL) { free(buf); return; };

				// receive payload
				int payload_size = atoi((argc == 5)? argv[4] : argv[3]);
				char* payload_buf = client_readline(payload_size);

				// put data into event struct
				msg_event e = {
					argv[1],
					sid,
					(argc == 5)? argv[3] : "",
					payload_buf
				};

				// call callback
				subs[sid]->call(e);
				if (subs[sid]->maxed()) unsubscribe(sid);

				free(payload_buf);
			}
			else if (strcmp(argv[0], NATS_CTRL_OK) == 0) {
			}
			else if (strcmp(argv[0], NATS_CTRL_ERR) == 0) {
				if (on_error != NULL) on_error();
				disconnect();
			}
			else if (strcmp(argv[0], NATS_CTRL_PING) == 0) {
				send(NATS_CTRL_PONG);
			}
			else if (strcmp(argv[0], NATS_CTRL_PONG) == 0) {
				outstanding_pings--;
			}
			else if (strcmp(argv[0], NATS_CTRL_INFO) == 0) {
				send_connect();
				connected = true;
				if (on_connect != NULL) on_connect();
			}

			free(buf);
		}

		void ping() {
			if (outstanding_pings > max_outstanding_pings) {
				client.stop();
				return;
			}
			outstanding_pings++;
			send(NATS_CTRL_PING);
		}

		char* generate_inbox_subject() {
			char* buf;
			asprintf(&buf, 
					"_INBOX.%d%d%d%d%d",
					(int)random(0x0010000),
					(int)random(0x0010000),
					(int)random(0x0010000),
					(int)random(0x0010000),
					(int)random(0x1000000));
			return buf;
		}

	public:

		bool connect() {
			if (client.connect(hostname, port)) {
				outstanding_pings = 0;
				reconnect_attempts = 0;
				return true;
			}
			reconnect_attempts++;
			return false;
		}

		void disconnect() {
			connected = false;
			client.stop();
			subs.empty();
			if (on_disconnect != NULL) on_disconnect();
		}

		void publish(const char* subject, const char* msg = NULL, const char* replyto = NULL) {
			if (subject == NULL || subject[0] == 0) return;
			if (!connected) return;
			send_fmt("PUB %s %s %lu", 
					subject, 
					(replyto == NULL)? "" : replyto,
					(unsigned long)strlen(msg));
			send((msg == NULL)? "" : msg);
		}
		void publish(const char* subject, const bool msg) {
			publish(subject, (msg)? "true" : "false");
		}
		void publish(const char* subject, const int msg) {
			char* buf;
			asprintf(&buf, "%d", msg);
			publish(subject, buf);
			free(buf);
		}
		void publish(const char* subject, const long msg) {
			char* buf;
			asprintf(&buf, "%ld", msg);
			publish(subject, buf);
			free(buf);
		}
		void publish(const char* subject, const float msg) {
			char buf[64];
			sprintf(buf, "%f", msg);
			publish(subject, buf);
		}
		void publish(const char* subject, const double msg) {
			char buf[64];
			sprintf(buf, "%lf", msg);
			publish(subject, buf);
		}

		int subscribe(const char* subject, sub_cb cb, const char* queue = NULL, const int max = 0) {
			if (!connected) return -1;
			Sub* sub = new Sub(cb, max);

			int sid;
			if (free_sids.empty()) {
				sid = subs.push_back(sub);
			} else {
				sid = free_sids.pop();
				subs[sid] = sub;
			}

			send_fmt("SUB %s %s %d", 
					subject, 
					(queue == NULL)? "" : queue, 
					sid);
			return sid;
		}

		void unsubscribe(const int sid) {
			if (!connected) return;
			send_fmt("UNSUB %d", sid);
			free(subs[sid]);
			subs[sid] = NULL;
			free_sids.push(sid);
		}

		int request(const char* subject, const char* msg, sub_cb cb, const int max = 1) {
			if (subject == NULL || subject[0] == 0) return -1;
			if (!connected) return -1;
			char* inbox = generate_inbox_subject();
			int sid = subscribe(inbox, cb, NULL, max);
			publish(subject, msg, inbox);
			free(inbox);
			return sid;
		}

		void process() {
			if (client.connected()) {
				if (client.available())
					recv();
				if (ping_timer.process())
					ping();
			} else {
				disconnect();
				if (max_reconnect_attempts == -1 || reconnect_attempts < max_reconnect_attempts) {
					if (reconnect_timer.process())
						connect();
				}
			}
		}

};

#endif
