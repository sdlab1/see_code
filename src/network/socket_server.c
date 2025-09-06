// src/socket/socket_server.c

#include "see_code/socket/socket_server.h"
#include "see_code/utils/logger.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h> // Для snprintf

#define SOCKET_BUFFER_SIZE 4096
#define MAX_MESSAGE_SIZE (10 * 1024 * 1024) // 10 MB

struct SocketServer {
    int server_fd;
    char* socket_path;
    SocketDataCallback callback;
};

// Вспомогательная функция для получения всех данных
// Читает из сокета до тех пор, пока не получит 0 (клиент закрыл соединение)
// или пока не превысит лимит MAX_MESSAGE_SIZE.
static ssize_t recv_all(int sockfd, void *buf, size_t len) {
    size_t to_read = len;
    ssize_t received = 0;
    char *buf_ptr = (char*)buf;

    while (to_read > 0) {
        ssize_t r = recv(sockfd, buf_ptr, to_read, 0); // Убираем флаги, они не нужны
        if (r < 0) {
            if (errno == EINTR) continue;
            // Логируем ошибку, но не возвращаем -1 сразу, чтобы вызывающая функция знала об ошибке
            log_error("recv_all: recv failed: %s", strerror(errno));
            return -1; // Ошибка
        }
        if (r == 0) {
             // Соединение закрыто клиентом, это нормальное завершение
             log_debug("recv_all: Client closed connection");
             break;
        }
        to_read -= r;
        buf_ptr += r;
        received += r;

        // Проверка на максимальный размер внутри цикла для раннего выхода
        if (received > MAX_MESSAGE_SIZE) {
             log_warn("recv_all: Message size exceeded limit (%d bytes) during read", MAX_MESSAGE_SIZE);
             // Мы уже получили больше разрешенного, обрезаем
             received = MAX_MESSAGE_SIZE;
             break;
        }
    }
    return received;
}

SocketServer* socket_server_create(const char* socket_path, SocketDataCallback callback) {
    if (!socket_path) {
        log_error("Socket path is NULL");
        return NULL;
    }

    SocketServer* server = malloc(sizeof(SocketServer));
    if (!server) {
        log_error("Failed to allocate memory for SocketServer");
        return NULL;
    }

    server->socket_path = strdup(socket_path);
    if (!server->socket_path) {
        log_error("Failed to duplicate socket path string");
        free(server);
        return NULL;
    }

    server->callback = callback;
    server->server_fd = -1;

    // Создаем Unix domain socket
    server->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server->server_fd == -1) {
        log_error("Failed to create socket: %s", strerror(errno));
        free(server->socket_path);
        free(server);
        return NULL;
    }

    // Удаляем сокет файл, если он существует
    unlink(socket_path);

    // Настраиваем адрес
    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, socket_path, sizeof(address.sun_path) - 1);

    // Привязываем сокет
    if (bind(server->server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        log_error("Failed to bind socket: %s", strerror(errno));
        close(server->server_fd);
        unlink(socket_path);
        free(server->socket_path);
        free(server);
        return NULL;
    }

    // Начинаем слушать
    if (listen(server->server_fd, 5) == -1) {
        log_error("Failed to listen on socket: %s", strerror(errno));
        close(server->server_fd);
        unlink(socket_path);
        free(server->socket_path);
        free(server);
        return NULL;
    }

    log_info("Socket server created and listening on %s", socket_path);
    return server;
}

void socket_server_destroy(SocketServer* server) {
    if (!server) {
        return;
    }

    if (server->server_fd != -1) {
        close(server->server_fd);
    }

    if (server->socket_path) {
        unlink(server->socket_path); // Удаляем файл сокета
        free(server->socket_path);
    }

    free(server);
    log_info("Socket server destroyed");
}

void socket_server_run(SocketServer* server) {
    if (!server) {
        return;
    }

    struct sockaddr_un address;
    socklen_t address_length = sizeof(address);

    log_info("Starting socket server loop on %s", server->socket_path);

    while (1) {
        int client_fd = accept(server->server_fd, (struct sockaddr *)&address, &address_length);
        if (client_fd == -1) {
            if (errno == EINTR) {
                log_debug("Accept interrupted by signal, continuing");
                continue;
            }
            log_error("Accept failed: %s", strerror(errno));
            break; // Критическая ошибка, выходим из цикла
        }

        log_info("Client connected");

        // --- Исправленная часть: Аккумуляция данных ---
        char* full_data_buffer = NULL;
        size_t buffer_capacity = SOCKET_BUFFER_SIZE;
        ssize_t total_received = 0;
        int error_occurred = 0;

        // Выделяем начальный буфер
        full_data_buffer = malloc(buffer_capacity);
        if (!full_data_buffer) {
            log_error("Failed to allocate initial buffer for client data");
            close(client_fd);
            continue; // Перейти к следующему клиенту
        }

        // Используем recv_all для чтения всех данных от клиента
        total_received = recv_all(client_fd, full_data_buffer, buffer_capacity);

        if (total_received < 0) {
            // Ошибка в recv_all
            log_error("Error occurred while receiving data from client");
            error_occurred = 1;
        } else if (total_received > MAX_MESSAGE_SIZE) {
             log_warn("Message size exceeded limit (%d bytes), disconnecting client", MAX_MESSAGE_SIZE);
             error_occurred = 1; // Обрезаем данные, но помечаем как ошибку
             total_received = MAX_MESSAGE_SIZE; // Обрезаем для обработки
        } else {
            log_info("Client disconnected, received %zd bytes", total_received);
        }

        // Закрываем соединение с клиентом как можно скорее
        close(client_fd);

        // --- Вызов колбэка с ПОЛНЫМИ данными ---
        if (!error_occurred && total_received > 0) {
            log_debug("Processing %zd bytes of data with callback", total_received);
            if (server->callback) {
                // Передаем ВСЕ полученные данные за сессию одним вызовом
                server->callback(full_data_buffer, total_received);
            }
        } else if (!error_occurred && total_received == 0) {
             log_info("Client disconnected, no data received.");
             // Можно вызвать колбэк с пустыми данными, если это ожидается логикой приложения
             // В текущей реализации app.c это, скорее всего, не нужно.
             // if (server->callback) server->callback("", 0);
        }

        // Освобождаем буфер
        if (full_data_buffer) {
            free(full_data_buffer);
        }
    }

    log_info("Socket server loop finished");
}
