#include "see_code/network/socket_server.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

struct SocketServer {
    char* socket_path;
    int server_fd;
    int running;
    SocketDataCallback callback;
    pthread_mutex_t mutex;
};

SocketServer* socket_server_create(const char* socket_path, SocketDataCallback callback) {
    if (!socket_path || !callback) {
        log_error("Invalid arguments to socket_server_create");
        return NULL;
    }
    
    SocketServer* server = malloc(sizeof(SocketServer));
    if (!server) {
        log_error("Failed to allocate memory for SocketServer");
        return NULL;
    }
    
    memset(server, 0, sizeof(SocketServer));
    
    server->socket_path = strdup(socket_path);
    if (!server->socket_path) {
        log_error("Failed to allocate memory for socket path");
        free(server);
        return NULL;
    }
    
    server->callback = callback;
    server->running = 0;
    
    if (pthread_mutex_init(&server->mutex, NULL) != 0) {
        log_error("Failed to initialize socket server mutex");
        free(server->socket_path);
        free(server);
        return NULL;
    }
    
    return server;
}

void socket_server_destroy(SocketServer* server) {
    if (!server) {
        return;
    }
    
    socket_server_stop(server);
    
    if (server->server_fd >= 0) {
        close(server->server_fd);
        unlink(server->socket_path);
    }
    
    pthread_mutex_destroy(&server->mutex);
    free(server->socket_path);
    free(server);
}

void socket_server_run(SocketServer* server) {
    if (!server) {
        return;
    }
    
    struct sockaddr_un address;
    socklen_t address_length = sizeof(address);
    
    // Create socket
    server->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        log_error("Failed to create socket: %s", strerror(errno));
        return;
    }
    
    // Prepare socket address
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, server->socket_path, sizeof(address.sun_path) - 1);
    
    // Unlink previous socket file if it exists
    unlink(server->socket_path);
    
    // Bind socket
    if (bind(server->server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        log_error("Failed to bind socket: %s", strerror(errno));
        close(server->server_fd);
        server->server_fd = -1;
        return;
    }
    
    // Listen for connections
    if (listen(server->server_fd, 5) < 0) {
        log_error("Failed to listen on socket: %s", strerror(errno));
        close(server->server_fd);
        unlink(server->socket_path);
        server->server_fd = -1;
        return;
    }
    
    log_info("Socket server listening on %s", server->socket_path);
    
    server->running = 1;
    
    while (server->running) {
        // Accept connection
        int client_fd = accept(server->server_fd, (struct sockaddr*)&address, &address_length);
        if (client_fd < 0) {
            if (server->running) {
                log_error("Failed to accept connection: %s", strerror(errno));
            }
            break;
        }
        
        log_info("Client connected");
        
        // Receive data
        char buffer[SOCKET_BUFFER_SIZE];
        ssize_t bytes_received;
        size_t total_bytes = 0;
        
        while ((bytes_received = recv(client_fd, buffer, sizeof(buffer), 0)) > 0) {
            total_bytes += bytes_received;
            
            // Process received data
            if (server->callback) {
                server->callback(buffer, bytes_received);
            }
            
            // Check for maximum message size
            if (total_bytes > MAX_MESSAGE_SIZE) {
                log_warn("Message size exceeded limit, disconnecting client");
                break;
            }
        }
        
        if (bytes_received < 0) {
            log_error("Error receiving data: %s", strerror(errno));
        }
        
        log_info("Client disconnected, received %zu bytes", total_bytes);
        close(client_fd);
    }
    
    log_info("Socket server stopped");
}

void socket_server_stop(SocketServer* server) {
    if (!server) {
        return;
    }
    
    pthread_mutex_lock(&server->mutex);
    
    if (server->running) {
        server->running = 0;
        
        // Close server socket to unblock accept()
        if (server->server_fd >= 0) {
            close(server->server_fd);
            server->server_fd = -1;
        }
    }
    
    pthread_mutex_unlock(&server->mutex);
}
