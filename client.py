import asyncio
import websockets
import json
import sys

# Usage: python client.py <user_id> <channel_id> <room_id>
if len(sys.argv) != 4:
    print("Usage: python client.py <user_id> <channel_id> <room_id>")
    sys.exit(1)

user_id, channel_id, room_id = sys.argv[1], sys.argv[2], sys.argv[3]

SERVER_URL = "ws://localhost:9003/chat"


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
                msg = {"action": "send", "payload": text, "user_id": user_id}
                await websocket.send(json.dumps(msg))
                print(f">>> Sent: {msg}")

        # Run listening and sending concurrently
        await asyncio.gather(listen(), send())


if __name__ == "__main__":
    asyncio.run(chat_client())
