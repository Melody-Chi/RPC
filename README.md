# RPC System - Custom Implementation in C

## Overview
This project is about implementing a custom Remote Procedure Call (RPC) system in C, allowing seamless communication between distributed software applications over a network. This RPC system uses a client-server architecture.

## Requirements
- Must be written in C.
- Should compile and run on a Linux cloud VM.
- Do not use existing RPC libraries.
  
## RPC System Architecture
The entire system will be encapsulated within two primary files, `rpc.c` and `rpc.h`.

## API

### Data structures

The API makes use of a primary data structure named `rpc_data`. 

```c
typedef struct {
    int    data1;
    size_t data2_len;
    void   *data2;
} rpc_data;
```

Additionally, there will be structs for the state of the client and server.

```c
typedef struct rpc_client rpc_client;
typedef struct rpc_server rpc_server;
```

### Server-side API

1. `rpc_server *rpc_init_server(int port)`
2. `int rpc_register(rpc_server *srv, const char *name, rpc_data* (*handler)(rpc_data*))`
3. `void rpc_serve_all(rpc_server *srv)`

### Client-side API

1. `rpc_client *rpc_init_client(const char *addr, int port)`
2. `void rpc_close_client(rpc_client *cl)`
3. `rpc_handle *rpc_find(rpc_client *cl, const char *name)`
4. `rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, const rpc_data *data)`

### Shared API

1. `void *rpc_data_free(rpc_data* data)`

## Protocol
The protocol should be designed with a few key considerations:

1. Handling of size discrepancies with `size_t`.
2. Efficient encoding of data packet sizes.
3. Consideration of possible IP layer packet loss and duplication.
4. Adaptability with IP packet maximum size.
5. Operate over IPv6.

For more details, see `answers.txt`.

## Testing
To test, two executables named `rpc-server` and `rpc-client` will be used.

To run:
```bash
./rpc-server -p <port> &
./rpc-client -i <ip-address> -p <port>
```

## Stretch Goal: Non-blocking Performance
For enhanced performance, consider implementing a non-blocking version. This can be achieved through multi-threading, using `fork(2)`, or employing `select(2)`. 

If this is implemented, include the following in your code:

```c
#define NONBLOCKING
```

## Additional Considerations
For the full list of considerations, including possible questions and challenges related to the design, refer to the file `answers.txt`.

---

*This README provides a brief outline of the RPC system. For detailed specifications, data structures, and explanations, refer to the main project document.*
