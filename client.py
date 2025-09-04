import asyncio
import websockets
import json
import sys

# Usage: python client.py <user_id> <channel_id> <room_id>
if len(sys.argv) != 5:
    print("Usage: python client.py <user_id> <channel_id> <room_id> <server_port>")
    sys.exit(1)

user_id, channel_id, room_id, server_port = str(sys.argv[1]), str(sys.argv[2]), str(sys.argv[3]), sys.argv[4]

SERVER_URL = f"ws://localhost:{server_port}/chat"


async def chat_client():
    async with websockets.connect(SERVER_URL) as websocket:
        # Step 1: Send join request
        join_msg = {
            "action": "join",
            "channel_id": channel_id,
            "room_id": room_id,
            "user_id": user_id,
        }
        await websocket.send(json.dumps(join_msg))
        print(f">>> Sent: {join_msg}")

        async def listen():
            async for message in websocket:
                data = json.loads(message)
                print(f"<<< Received: {data}")

        async def send():
            while True:
                text = await asyncio.get_event_loop().run_in_executor(None, input, "")
                if text.lower() == "/quit":
                    await websocket.send(json.dumps({"action": "disconnect"}))
                    break
                msg = {"action": "send", "payload": text}
                await websocket.send(json.dumps(msg))
                print(f">>> Sent: {msg}")

        # Run listening and sending concurrently
        await asyncio.gather(listen(), send())


if __name__ == "__main__":
    asyncio.run(chat_client())
