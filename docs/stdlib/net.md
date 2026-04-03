# net — TCP Networking

**Backend:** aria-libc (aria_libc_net)  
**Ported:** v0.10.0

## API Reference

```aria
net_connect(host: string, port: int32) → int64          // connect to TCP server, returns FD
net_send(fd: int64, data: string) → int64               // send string data, returns bytes sent
net_recv(fd: int64, max_bytes: int64) → string           // receive data (up to max_bytes)
net_close(fd: int64) → NIL                               // close connection
net_listen(port: int32, backlog: int32) → int64          // create listening socket, returns FD
net_accept(fd: int64) → int64                            // accept incoming connection, returns client FD
net_server_close(fd: int64) → NIL                        // close listening socket
net_resolve(hostname: string) → string                   // DNS resolve hostname to IP string
net_set_nonblocking(fd: int64) → NIL                     // set socket to non-blocking mode
```

## Example

```aria
use "stdlib/net.aria".*;

func:failsafe = int32(tbb32:err) { exit 1; };

func:main = int32() {
    // Server
    int64:srv = raw net_listen(8080i32, 5i32);
    int64:client = raw net_accept(srv);
    string:msg = raw net_recv(client, 1024i64);
    println(msg);
    int64:sent = raw net_send(client, "OK");
    drop net_close(client);
    drop net_server_close(srv);
    exit 0;
};
```

## Dependencies

Requires `libaria_libc_mem.so`, `libaria_libc_net.so`, `libaria_libc_string.so` at runtime.
