# Real-Time WebSocket Chat Server


## Overview

This is a high-performance, real-time WebSocket chat server built using **uWebSockets** and **nlohmann::json** and written in C++.
The architecture is designed to be modular, low-footprint, and easily extendable.


---

## Features

- **Multi-channel, multi-room support**: Users can join different channels and rooms. Messages are scoped to the room they belong to.
- **Validations**: Messages are validated for size and schema before reaching further.
- **Rate limiting**: Users are limited to 5 messages per second to prevent spam. Excessive violations result in automatic disconnection.
- **Message history**: Keeps the last 50 messages per room for users joining late.
- **Real-time messaging**: Broadcast messages are sent to all participants in a room instantly.
- **JSON-based communication**: All client-server messages use JSON with strict schema validation.
- **Graceful shutdown**: SIGINT handling closes all connections cleanly.
- **Modular code structure**: Divided into headers and implementation for maintainability and scalability.
- **Logging**: Logs key events including connections, disconnections, rate-limit violations, and errors.


---

## Client-Server Communication

All communication is performed using **JSON messages** over WebSocket.

---

# 1. Client Requests

## `join`
Used to join a channel and room.

**Request:**
```
{
  "action": "join",
  "user_id": "user123",
  "channel_id": "general",
  "room_id": "room1"
}
```

**Responses:**

> `ACK`
```
{
  "action": "join_ack",
  "user_id": "user123",
  "channel_id": "general",
  "room_id": "room1",
  "messages": [ /* previous messages in the room */ ],
  "timestamp": "2025-09-04T12:00:00.123Z"
}
```

> `NACK`
```
{
  "action": "join_nack",
  "reason": "already_connected",
  "timestamp": "2025-09-04T12:00:00.123Z"
}
```

## `send`
Used to send a message to the current room.
```
**Request:**
{
  "action": "send",
  "payload": "Hello everyone!"
}
```

**Responses:**

> `Broadcast`
```
{
  "event": "broadcast",
  "payload": "Hello everyone!",
  "user_id": "user123",
  "channel_id": "general",
  "room_id": "room1",
  "timestamp": "2025-09-04T12:01:00.456Z"
}
```

> `NACK`
```
{
  "action": "send_nack",
  "reason": "rate_limited",
  "timestamp": "2025-09-04T12:01:00.456Z"
}
```

## `disconnect`
Request to voluntarily disconnect.

**Request:**
```
{
  "action": "disconnect"
}
```

**Responses:**

`Closes the WebSocket and broadcasts a disconnected event to all participants in the same room`
```
{
  "event": "disconnected",
  "user_id": "user123",
  "channel_id": "general",
  "room_id": "room1",
  "timestamp": "2025-09-04T12:02:00.789Z"
}
```

---

# 2. Server Notifications

#### `User joined a room`
```
{
  "event": "user_joined",
  "user_id": "user123",
  "channel_id": "general",
  "room_id": "room1",
  "timestamp": "2025-09-04T12:00:01.123Z"
}
```

#### `User disconnected`
```
{
  "event": "disconnected",
  "user_id": "user123",
  "channel_id": "general",
  "room_id": "room1",
  "timestamp": "2025-09-04T12:02:00.789Z"
}
```
