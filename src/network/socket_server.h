#ifndef SEE_CODE_SOCKET_SERVER_H
#define SEE_CODE_SOCKET_SERVER_H

#include <stddef.h>

// Callback function type for handling received data
typedef void (*SocketDataCallback)(const char* data, size_t length);

// Socket server structure
typedef struct SocketServer SocketServer;

// Socket server functions
SocketServer* socket_server_create(const char* socket_path, SocketDataCallback callback);
void socket_server_destroy(SocketServer* server);
void socket_server_run(SocketServer* server);
void socket_server_stop(SocketServer* server);

#endif // SEE_CODE_SOCKET_SERVER_H
