// Servidor TCP del repositorio de ROAs: HELLO + 1 comando + cierre, un hilo por conexión
// Uso: ./repoServer [trusted-roots.pem] [puerto]

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "repoCommands.h"

#define DEFAULT_TRUSTSTORE "/etc/rpki/truststore/trusted-roots.pem"
#define DEFAULT_PORT 9000
#define TIMEOUT_SECONDS 10
#define LOG_TEXT_SIZE 40

// Lo que recibe el hilo de cada conexión
typedef struct {
    int fd;
    char ip[INET_ADDRSTRLEN];
    char pending[MAX_LINE];  // Lo recibido que no se ha leído como línea
    size_t pendingLen;
} Client;

static volatile sig_atomic_t running = 1;

static void stopServer(int signalNumber) {
    (void)signalNumber;
    running = 0;
}

// Copia en line la siguiente línea, sin el \n
// Retorna 0 si la leyó, -1 si hubo timeout o el cliente cerró, y -2 si no cabe
static int readLine(Client *client, char *line) {
    while (1) {
        char *newline = memchr(client->pending, '\n', client->pendingLen);
        if (newline != NULL) {
            size_t lineLen = (size_t)(newline - client->pending);
            memcpy(line, client->pending, lineLen);
            line[lineLen] = '\0';
            if (lineLen > 0 && line[lineLen - 1] == '\r') {
                line[lineLen - 1] = '\0';
            }
            client->pendingLen -= lineLen + 1;
            memmove(client->pending, newline + 1, client->pendingLen);
            return 0;
        }
        if (client->pendingLen == sizeof client->pending) {
            return -2;
        }
        ssize_t received = recv(client->fd, client->pending + client->pendingLen,
                                sizeof client->pending - client->pendingLen, 0);
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received <= 0) {
            return -1;
        }
        client->pendingLen += (size_t)received;
    }
}

// Manda todo el texto. Con MSG_NOSIGNAL no muere por SIGPIPE si el cliente ya cerró
// Retorna 0, o -1 si no se pudo
static int sendAll(int fd, const char *text, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        ssize_t written = send(fd, text + sent, length - sent, MSG_NOSIGNAL);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return -1;
        }
        sent += (size_t)written;
    }
    return 0;
}

// Bitácora: hora, quién, qué pidió y qué se le contestó
static void logRequest(const Client *client, const char *line, const char *response) {
    char hour[16];
    time_t now = time(NULL);
    struct tm parts;
    localtime_r(&now, &parts);
    strftime(hour, sizeof hour, "%H:%M:%S", &parts);

    int responseLen = (int)strcspn(response, "\n");
    if (responseLen > LOG_TEXT_SIZE) {
        responseLen = LOG_TEXT_SIZE;
    }
    printf("%s %s %.*s -> %.*s\n", hour, client->ip, LOG_TEXT_SIZE, line, responseLen, response);
}

static void *serveClient(void *argument) {
    Client *client = argument;
    Session session;
    sessionInit(&session);
    char line[MAX_LINE];
    char response[MAX_RESPONSE];

    // HELLO + 1 comando
    for (int turn = 0; turn < 2; turn++) {
        int result = readLine(client, line);
        if (result == -1) {
            break;
        }
        if (result == -2) {
            snprintf(response, sizeof response, "ERR_FORMATO\n");
            sendAll(client->fd, response, strlen(response));
            break;
        }
        handleLine(&session, line, response, sizeof response);
        logRequest(client, line, response);
        if (sendAll(client->fd, response, strlen(response)) != 0 || !session.greeted) {
            break;
        }
    }

    sessionFree(&session);
    close(client->fd);
    free(client);
    return NULL;
}

// Abre el socket que recibe las conexiones. Retorna su descriptor, o -1 si no se pudo
static int openListener(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    // Para poder reiniciar el servidor enseguida en el mismo puerto
    int enable = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof enable);

    struct sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&address, sizeof address) != 0 || listen(fd, SOMAXCONN) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

// Atiende la conexión en su propio hilo
static void startClientThread(int clientFd, const struct sockaddr_in *address) {
    struct timeval timeout = {.tv_sec = TIMEOUT_SECONDS, .tv_usec = 0};
    setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);

    Client *client = malloc(sizeof *client);
    if (client == NULL) {
        close(clientFd);
        return;
    }
    client->fd = clientFd;
    client->pendingLen = 0;
    inet_ntop(AF_INET, &address->sin_addr, client->ip, sizeof client->ip);

    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    pthread_t thread;
    if (pthread_create(&thread, &attributes, serveClient, client) != 0) {
        close(clientFd);
        free(client);
    }
    pthread_attr_destroy(&attributes);
}

int main(int argc, char *argv[]) {
    if (argc > 3) {
        fprintf(stderr, "Uso: %s [trusted-roots.pem] [puerto]\n", argv[0]);
        return 2;
    }
    const char *trustStorePath = (argc >= 2) ? argv[1] : DEFAULT_TRUSTSTORE;
    int port = (argc == 3) ? atoi(argv[2]) : DEFAULT_PORT;
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Puerto no válido: %s\n", argv[2]);
        return 2;
    }

    // La bitácora sale línea por línea aunque vaya a un archivo
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (commandsInit(trustStorePath) != 0) {
        fprintf(stderr, "No se pudo cargar el truststore %s o preparar el almacenamiento\n", trustStorePath);
        return 1;
    }
    int listener = openListener(port);
    if (listener < 0) {
        perror("No se pudo abrir el puerto");
        commandsFree();
        return 1;
    }

    // Sin SA_RESTART, para que accept se interrumpa al cerrar
    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_handler = stopServer;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    printf("Repositorio de ROAs escuchando en el puerto %d\n", port);
    while (running) {
        struct sockaddr_in address;
        socklen_t addressLen = sizeof address;
        int clientFd = accept(listener, (struct sockaddr *)&address, &addressLen);
        if (clientFd < 0) {
            continue;
        }
        commandsRefreshTrustStore();
        startClientThread(clientFd, &address);
    }

    printf("Repositorio cerrado\n");
    close(listener);
    commandsFree();
    return 0;
}
