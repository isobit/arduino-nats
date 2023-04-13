# ArduinoNATS example with JetStream

Here's why you should consider using JetStream for your IoT project: [JetStream concept](https://docs.nats.io/nats-concepts/jetstream)

Python 3.11.0 was used for the message sender. But any [Python 3.7+](https://docs.python.org/3.7/library/asyncio.html) should work as well.

- To get started, you need to [run NATS server](https://docs.nats.io/running-a-nats-service/nats_docker/jetstream_docker) locally with JetStream enabled. The easiest way to do this is by using Docker:
  
  ```sh
  docker run --network host -p 4222:4222 nats -js
  ```

- Navigate to the message-sender folder and install the dependencies:

  ```sh
  cd examples/jet-stream/message-sender
  python -m pip install -r requirements.txt
  ```

- Run message sender:

  ```sh
  python main.py
  ```

- Copy code from main.cpp and paste into your Arduino sketch file
- Build and upload to your board and open device monitor.