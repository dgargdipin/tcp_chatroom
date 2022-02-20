# Assignment 2 - Asynchronous Multithreaded TCP/IP Socket Chat Application

-- Dipin Garg

A TCP/IP chat room server application which can handle multiple clients. The server can receive multiple client requests at the same time and entertain each client request in parallel so that no client will have to wait for server time. The client makes use of `ncurses` library for terminal manipulation for user interface and asynchronous input. Thus, server responses and input never interfere with each other.

## Server

The `chat_server.cpp` file contains class definition of `TCP_Server` and `Logger` classes. The constructor of `TCP_Server` takes argument `int port` and  sets up a TCP chat room server at that port. `Logger` class is used to log the events of the server in a synchronized and detailed manner.

The server listens on the defined port waiting for new connections, and for each connection, it spawns a new thread which receives messages from the new client. For every message received from a client, the server broadcasts the message to each connected client. It does this so by maintaining an array of connected clients. After a connection has been closed by a client, the main thread spawns a detached thread to remove the stored client, and any other leftovers.

## Client

The client uses `ncurses` library to allow asynchronous input from the user, while receiving messages from the for standard output. It also provides the user a better user interface. `chat_client.cpp` contains class definitions for `TCP_Client`,`ClientWin` and `ClientApp`. `TCP_Client` provides interfaces to communicate with a server, `ClientWin` instantiates the terminal for `ncurses` usage, and provides methods to read input from the window, along with `write` for synchronized output in the output window. `ClientApp` is a composition of `TCP_Client` and `ClientApp` to unify the interfaces of both classes.	 

## Compiling and running

The server can be compiled using:

```
g++ chat_server.cpp -o chat_server -lpthread
```

The client can be compiled using:

```
g++ chat_client.cpp -o chat_client -lpthread -lncurses                                                                         
```

By default the server runs on port `3491`, which can be changed by simply changing the value passed into the constructor of `TCP_Server` class in main(). To run the client 2 additional arguments namely hostname and port are required. Example usage includes `./chat_client 127.0.0.1 3491`.

