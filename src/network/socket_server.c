// src/network/socket_server.c
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
    volatile int running; // Добавлено volatile для потокобезопасности
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
    server->server_fd = -1; // Инициализируем как невалидный

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

    socket_server_stop(server); // Убедимся, что сервер остановлен

    // Закрываем сокет и удаляем файл, если они существуют
    if (server->server_fd >= 0) {
        close(server->server_fd);
        server->server_fd = -1;
    }
    if (server->socket_path) {
        unlink(server->socket_path);
        free(server->socket_path);
    }

    pthread_mutex_destroy(&server->mutex);
    free(server);
    log_info("Socket server destroyed");
}

void socket_server_run(SocketServer* server) {
    if (!server) {
        return;
    }

    struct sockaddr_un address;
    server->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server->server_fd < 0) {
        log_error("Failed to create socket: %s", strerror(errno));
        return;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    // Безопасное копирование пути
    snprintf(address.sun_path, sizeof(address.sun_path), "%s", server->socket_path);

    unlink(server->socket_path); // Удаляем старый сокет, если он есть
    if (bind(server->server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        log_error("Failed to bind socket: %s", strerror(errno));
        close(server->server_fd);
        server->server_fd = -1;
        return;
    }

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
        int client_fd = accept(server->server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (server->running) { // Логируем ошибку только если сервер не был остановлен
                log_error("Failed to accept connection: %s", strerror(errno));
            }
            break;
        }

        log_info("Client connected");

        // --- ЛОГИКА АККУМУЛЯЦИИ ДАННЫХ ---
        char* full_buffer = NULL;
        size_t total_bytes = 0;
        size_t buffer_capacity = 0;
        char temp_buffer[4096];
        ssize_t bytes_received;

        while ((bytes_received = recv(client_fd, temp_buffer, sizeof(temp_buffer), 0)) > 0) {
            if (total_bytes + bytes_received > buffer_capacity) {
                size_t new_capacity = (buffer_capacity == 0) ? sizeof(temp_buffer) : buffer_capacity * 2;
                if (new_capacity < total_bytes + bytes_received) {
                    new_capacity = total_bytes + bytes_received;
                }
                
                char* new_buffer = realloc(full_buffer, new_capacity);
                if (!new_buffer) {
                    log_error("Failed to reallocate buffer for client data");
                    free(full_buffer);
                    goto next_client;
                }
                full_buffer = new_buffer;
                buffer_capacity = new_capacity;
            }
            memcpy(full_buffer + total_bytes, temp_buffer, bytes_received);
            total_bytes += bytes_received;

            if (total_bytes > MAX_MESSAGE_SIZE) {
                log_warn("Message size exceeded limit (%d bytes), disconnecting client", MAX_MESSAGE_SIZE);
                break;
            }
        }

        if (bytes_received < 0) {
            log_error("Error receiving data from client: %s", strerror(errno));
        }

        if (total_bytes > 0 && server->callback) {
            // Вызываем колбэк с полным, аккумулированным буфером
            server->callback(full_buffer, total_bytes);
        }
        
        free(full_buffer);

    next_client:
        close(client_fd);
        log_info("Client disconnected");
    }

    // Очистка при выходе из цикла
    if (server->server_fd >= 0) {
        close(server->server_fd);
        unlink(server->socket_path);
        server->server_fd = -1;
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
        // Закрытие сокета заставит accept() выйти с ошибкой, что прервет цикл
        if (server->server_fd >= 0) {
            shutdown(server->server_fd, SHUT_RDWR); // Прерываем блокирующие вызовы
            close(server->server_fd);
            server->server_fd = -1;
        }
    }
    pthread_mutex_unlock(&server->mutex);
}
