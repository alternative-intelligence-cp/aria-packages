# aria-websocket

WebSocket client and server for Aria — connect, send, receive, broadcast via libwebsockets

## Installation

```bash
aria-pkg install aria-websocket
```

## API

| Function | Description |
|----------|-------------|
| `ws_connect(string:host, int32:port, string:path) -> int32` | Connect to a WebSocket server. Returns connection ID or negative on error. |
| `ws_listen(string:bind_addr, int32:port, int32:backlog) -> int32` | Start a WebSocket server. Returns listener FD or negative on error. |
| `ws_accept(int32:listen_fd, int32:timeout_ms) -> int32` | Accept an incoming connection. Returns connection ID or negative on timeout. |
| `ws_send(int32:conn_id, string:msg) -> int32` | Send a text message on a connection. Returns 0 on success. |
| `ws_recv(int32:conn_id, int32:timeout_ms) -> int32` | Receive a message. Returns 0 on success; retrieve with `ws_message()`. |
| `ws_broadcast(string:msg) -> int32` | Broadcast a message to all connected clients. |
| `ws_ping(int32:conn_id) -> int32` | Send a ping frame. |
| `ws_message() -> string` | Get the last received message content. |
| `ws_message_len() -> int32` | Get the length of the last received message. |
| `ws_error() -> string` | Get the last error message. |
| `ws_close(int32:conn_id) -> NIL` | Close a connection. |
| `ws_close_listener(int32:listen_fd) -> NIL` | Close a listener socket. |
| `ws_connection_count() -> int32` | Get the number of active connections. |

## Example

```aria
use aria_websocket;

func:failsafe = NIL(int32:code) { pass(NIL); };

func:main = NIL() {
    // Start a server
    int32:srv = ws_listen("127.0.0.1", 9001i32, 5i32);

    // Connect a client
    int32:client = ws_connect("127.0.0.1", 9001i32, "/");

    // Accept on server side
    int32:peer = ws_accept(srv, 5000i32);

    // Send from client, receive on server
    drop(ws_send(client, "Hello from Aria!"));
    drop(ws_recv(peer, 2000i32));
    string:msg = ws_message();
    drop(println("Received: &{msg}"));

    // Clean up
    ws_close(client);
    ws_close(peer);
    ws_close_listener(srv);
};
```

## Dependencies

None beyond libc. The package includes its own WebSocket implementation via `libaria_websocket_shim.so`.
