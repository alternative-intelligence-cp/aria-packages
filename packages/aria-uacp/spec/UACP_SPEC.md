# Universal AI Communication Protocol (UACP) Specification v1.0

## Overview

UACP is a binary machine-to-machine communication protocol designed for AI-native systems.
It prioritizes efficiency, extensibility, and machine consumption over human readability.

## Design Priorities

1. **Machine-first**: Binary framing, no human-readable overhead in wire format
2. **Efficient**: Minimal header overhead (20 bytes), zero-copy for binary payloads
3. **Extensible**: New message types via 16-bit type field, no breaking changes
4. **Typed**: All messages carry explicit type tags; payloads are self-describing
5. **Bidirectional**: Both peers can send any message type after handshake

## Wire Format

### Frame Layout

```
Offset  Size   Field
──────  ────   ─────
0       4      Magic      "UACP" (0x55 0x41 0x43 0x50)
4       1      Version    Protocol version (currently 0x01)
5       1      Flags      Bit flags (see below)
6       2      Type       Message type (uint16, little-endian)
8       4      SeqID      Sequence ID (uint32, little-endian)
12      4      Length     Payload length in bytes (uint32, little-endian)
16      N      Payload    Type-specific payload
16+N    4      CRC32      CRC-32 of bytes [0..16+N) (if FLAG_CRC set)
```

Total header: 16 bytes. Maximum payload: 4 GiB.

### Flags (byte 5)

```
Bit 0 (0x01): FLAG_CRC      — CRC32 trailer present
Bit 1 (0x02): FLAG_COMPRESS — Payload is zlib-compressed
Bit 2 (0x04): FLAG_REPLY    — This message is a reply to SeqID
Bits 3-7:     Reserved (must be 0)
```

### Byte Order

All multi-byte integers are little-endian (matches x86/ARM).

## Message Types

### Connection

| Type   | Name       | Direction    | Description                     |
|--------|------------|--------------|---------------------------------|
| 0x0001 | HELLO      | Client→Svr   | Capability announcement         |
| 0x0002 | HELLO_ACK  | Server→Cli   | Capability response             |
| 0x0003 | PING       | Either       | Keepalive                       |
| 0x0004 | PONG       | Either       | Keepalive response              |

### Data

| Type   | Name        | Direction | Description                       |
|--------|-------------|-----------|-----------------------------------|
| 0x0010 | TEXT        | Either    | UTF-8 text message                |
| 0x0011 | BINARY      | Either    | Raw binary blob with tag          |
| 0x0012 | TENSOR      | Either    | Tensor data with shape metadata   |

### Tool Invocation

| Type   | Name        | Direction | Description                       |
|--------|-------------|-----------|-----------------------------------|
| 0x0020 | TOOL_CALL   | Either    | Invoke a named tool               |
| 0x0021 | TOOL_RESULT | Either    | Tool invocation result            |

### Streaming

| Type   | Name         | Direction | Description                      |
|--------|--------------|-----------|----------------------------------|
| 0x0030 | STREAM_START | Either    | Begin a named stream             |
| 0x0031 | STREAM_CHUNK | Either    | Stream data chunk                |
| 0x0032 | STREAM_END   | Either    | End a stream                     |

### State

| Type   | Name    | Direction | Description                          |
|--------|---------|-----------|--------------------------------------|
| 0x0040 | STATUS  | Either    | Health/status report                 |
| 0x0050 | STORE   | Either    | Request to persist named state       |
| 0x0051 | LOAD    | Either    | Request to retrieve named state      |

### Error

| Type   | Name    | Direction | Description                          |
|--------|---------|-----------|--------------------------------------|
| 0x00FF | ERROR   | Either    | Error response (references SeqID)    |

## Payload Formats

### HELLO (0x0001)
```
[name_len: uint16] [name: UTF-8]
[version_len: uint16] [version: UTF-8]
[cap_count: uint16] [capabilities: cap_count × UTF-8 strings (len-prefixed)]
```

### TEXT (0x0010)
```
[text: UTF-8 bytes, length = payload length]
```

### BINARY (0x0011)
```
[tag_len: uint16] [tag: UTF-8]
[data: remaining bytes]
```

### TENSOR (0x0012)
```
[ndim: uint8]
[shape: ndim × int32]
[dtype: uint8]  (0=float32, 1=float64, 2=int32, 3=int64)
[data: remaining bytes]
```

### TOOL_CALL (0x0020)
```
[name_len: uint16] [name: UTF-8]
[params: remaining bytes, UTF-8 JSON]
```

### TOOL_RESULT (0x0021)
```
[status: uint8]  (0=success, 1=error, 2=partial)
[data: remaining bytes, UTF-8]
```

### STREAM_START (0x0030)
```
[stream_id: uint32]
[tag_len: uint16] [tag: UTF-8]
```

### STREAM_CHUNK (0x0031)
```
[stream_id: uint32]
[data: remaining bytes]
```

### STREAM_END (0x0032)
```
[stream_id: uint32]
```

### STATUS (0x0040)
```
[code: uint16]  (0=healthy, 1=degraded, 2=overloaded)
[msg_len: uint16] [message: UTF-8]
```

### ERROR (0x00FF)
```
[error_code: uint32]
[message: remaining bytes, UTF-8]
```

## Sequence IDs

- Each peer maintains an independent sequence counter starting at 1
- Replies set FLAG_REPLY and SeqID = original message's SeqID
- SeqID 0 is reserved for unsolicited messages

## Connection Lifecycle

1. Client connects (TCP or Unix socket)
2. Client sends HELLO
3. Server sends HELLO_ACK
4. Bidirectional message exchange
5. Either peer closes connection

## Error Codes

| Code  | Meaning                     |
|-------|-----------------------------|
| 0     | No error                    |
| 1     | Unknown message type        |
| 2     | Malformed payload           |
| 3     | Tool not found              |
| 4     | Tool execution failed       |
| 5     | State not found             |
| 6     | Permission denied           |
| 7     | Resource exhausted          |
| 8     | Timeout                     |
| 100+  | Application-specific errors |

## Transport

- TCP (default port: 9317, "ARIA" on phone keypad)
- Unix domain sockets
- Future: shared memory, RDMA

## Versioning

- Protocol version in header byte 4
- Version 1 = this specification
- Unknown versions: respond with ERROR and close
- Forward compatibility: ignore unknown message types (log warning)
