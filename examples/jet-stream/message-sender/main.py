import asyncio
import nats
from aioconsole import ainput

press_enter_prompt = 'Press enter to send message\n'

async def main():
  try:
    while True:
      await ainput(press_enter_prompt)
      nc = await nats.connect("localhost")
      js = nc.jetstream()

      # Persist messages on 'foo's subject.
      await js.add_stream(name="sample-stream", subjects=["foo"])

      ack = await js.publish("foo", "hello world".encode())
      print(ack)

      await nc.close() 
  except Exception as e:
    print(f'[ERROR] {e}')

if __name__ == '__main__':
    asyncio.run(main())