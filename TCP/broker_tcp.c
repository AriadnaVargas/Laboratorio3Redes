/*
 * TCP Pub-Sub Broker
 * 
 * DESCRIPCION:
 * Servidor broker que implementa un modelo de publicacion-suscripcion usando TCP.
 * El broker recibe mensajes de publicadores y los redistribuye a los suscriptores
 * interesados en el tema correspondiente.
 * 
 * FUNCIONES SOCKET UTILIZADAS:
 * 1. socket() - Crea un socket TCP (SOCK_STREAM)
 * 2. bind() - Vincula el socket a una direccion IP y puerto
 * 3. listen() - Pone el socket en modo escucha para acepar conexiones
 * 4. accept() - Acepta una conexion entrante de un cliente
 * 5. recv() - Recibe datos del socket
 * 6. send() - Envía datos a través del socket
 * 7. close() - Cierra el socket
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define BROKER_PORT 9001
#define MAX_MESSAGE_SIZE 512
#define MAX_SUBSCRIBERS 100
#define MAX_TOPICS 20
#define BACKLOG 5

/* Estructura para almacenar informacion de suscriptores */
typedef struct {
    int socket;
    char topic[100];
} subscriber_t;

/* Variables globales para gestionar suscriptores */
subscriber_t subscribers[MAX_SUBSCRIBERS];
int num_subscribers = 0;

/*
 * forward_message_to_subscribers()
 * Recibe un mensaje de un publicador y lo reenvía a todos los suscriptores
 * del tema correspondiente.
 * 
 * Parametros:
 *   - topic: tema del mensaje
 *   - message: contenido del mensaje a reenviar
 */
void forward_message_to_subscribers(const char *topic, const char *message) {
    printf("[BROKER] Reenviando mensaje del tema '%s' a suscriptores\n", topic);
    
    for (int i = 0; i < num_subscribers; i++) {
        if (strcmp(subscribers[i].topic, topic) == 0) {
            char full_message[MAX_MESSAGE_SIZE];
            snprintf(full_message, sizeof(full_message), "[%s] %s", topic, message);
            
            if (send(subscribers[i].socket, full_message, strlen(full_message), 0) < 0) {
                fprintf(stderr, "[ERROR] No se pudo enviar mensaje al suscriptor\n");
            } else {
                printf("[BROKER] Mensaje enviado al suscriptor en socket %d\n", subscribers[i].socket);
            }
        }
    }
}

/*
 * handle_publisher()
 * Gestiona la conexion de un publicador.
 * Recibe mensajes en formato "PUBLISH|topic|message" y los reenvía
 * a los suscriptores correspondientes.
 * 
 * Parametros:
 *   - publisher_socket: socket conectado del publicador
 */
void handle_publisher(int publisher_socket) {
    char buffer[MAX_MESSAGE_SIZE];
    int bytes_received;
    
    printf("[BROKER] Nuevo publicador conectado en socket %d\n", publisher_socket);
    
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        bytes_received = recv(publisher_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            printf("[BROKER] Publicador desconectado (socket %d)\n", publisher_socket);
            break;
        }
        
        buffer[bytes_received] = '\0';
        printf("[BROKER] Mensaje recibido: %s\n", buffer);
        
        /* Parsear mensaje en formato "PUBLISH|topic|message" */
        char *token = strtok(buffer, "|");
        
        if (token != NULL && strcmp(token, "PUBLISH") == 0) {
            char topic[100] = {0};
            char message[MAX_MESSAGE_SIZE - 100] = {0};
            
            /* Extraer el tema */
            token = strtok(NULL, "|");
            if (token != NULL) {
                strcpy(topic, token);
            }
            
            /* Extraer el mensaje */
            token = strtok(NULL, "");
            if (token != NULL) {
                strcpy(message, token);
            }
            
            if (strlen(topic) > 0 && strlen(message) > 0) {
                forward_message_to_subscribers(topic, message);
            }
        }
    }
    
    close(publisher_socket);
}

/*
 * handle_subscriber()
 * Gestiona la conexion de un suscriptor.
 * El suscriptor envia un mensaje en formato "SUBSCRIBE|topic" y luego
 * permanece conectado recibiendo mensajes del broker.
 * 
 * Parametros:
 *   - subscriber_socket: socket conectado del suscriptor
 */
void handle_subscriber(int subscriber_socket) {
    char buffer[MAX_MESSAGE_SIZE];
    int bytes_received;
    
    printf("[BROKER] Nuevo suscriptor conectado en socket %d\n", subscriber_socket);
    
    /* Recibir mensaje de suscripcion */
    memset(buffer, 0, sizeof(buffer));
    bytes_received = recv(subscriber_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        printf("[BROKER] Suscriptor desconectado antes de enviar suscripcion\n");
        close(subscriber_socket);
        return;
    }
    
    buffer[bytes_received] = '\0';
    printf("[BROKER] Suscripcion recibida: %s\n", buffer);
    
    /* Parsear mensaje en formato "SUBSCRIBE|topic" */
    char *token = strtok(buffer, "|");
    
    if (token != NULL && strcmp(token, "SUBSCRIBE") == 0) {
        token = strtok(NULL, "");
        
        if (token != NULL && num_subscribers < MAX_SUBSCRIBERS) {
            subscribers[num_subscribers].socket = subscriber_socket;
            strcpy(subscribers[num_subscribers].topic, token);
            num_subscribers++;
            
            printf("[BROKER] Suscriptor registrado para tema '%s'\n", token);
            
            /* El suscriptor permanece conectado para recibir mensajes */
            char ack[100];
            snprintf(ack, sizeof(ack), "Suscrito al tema: %s\n", token);
            send(subscriber_socket, ack, strlen(ack), 0);
        }
    } else {
        close(subscriber_socket);
    }
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    
    printf("===== TCP PUB-SUB BROKER =====\n");
    printf("Iniciando broker en puerto %d...\n", BROKER_PORT);
    
    /* 
     * socket() - Crear un socket TCP
     * Parametros:
     *   AF_INET: Familia de direcciones IPv4
     *   SOCK_STREAM: Tipo de socket TCP (confiable, orientado a conexion)
     *   0: Protocolo por defecto para SOCK_STREAM (TCP)
     * Retorna: descriptor del socket o -1 si hay error
     */
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        fprintf(stderr, "[ERROR] No se pudo crear el socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    printf("[OK] Socket creado (descriptor: %d)\n", server_socket);
    
    /* Configurar estructura de direccion del servidor */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BROKER_PORT);
    server_addr.sin_addr.s_addr = inet_aton("127.0.0.1", &server_addr.sin_addr) ? 
                                   server_addr.sin_addr.s_addr : INADDR_ANY;
    
    /*
     * bind() - Vincular el socket a una direccion y puerto
     * Parametros:
     *   server_socket: descriptor del socket
     *   (struct sockaddr *)&server_addr: direccion a vincular
     *   sizeof(server_addr): tamaño de la estructura de direccion
     * Retorna: 0 si exito, -1 si hay error
     */
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "[ERROR] No se pudo vincular el socket al puerto %d: %s\n", 
                BROKER_PORT, strerror(errno));
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("[OK] Socket vinculado a 127.0.0.1:%d\n", BROKER_PORT);
    
    /*
     * listen() - Poner el socket en modo escucha
     * Parametros:
     *   server_socket: descriptor del socket
     *   BACKLOG: numero maximo de conexiones pendientes en la cola
     * Retorna: 0 si exito, -1 si hay error
     */
    if (listen(server_socket, BACKLOG) < 0) {
        fprintf(stderr, "[ERROR] No se pudo poner en escucha: %s\n", strerror(errno));
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("[OK] Broker en escucha...\n");
    printf("Esperando conexiones de publicadores y suscriptores...\n\n");
    
    /* Loop principal: aceptar conexiones */
    while (1) {
        client_addr_len = sizeof(client_addr);
        
        /*
         * accept() - Aceptar una conexion entrante
         * Parametros:
         *   server_socket: descriptor del socket del servidor
         *   (struct sockaddr *)&client_addr: estructura para almacenar info del cliente
         *   &client_addr_len: puntero al tamaño de la estructura
         * Retorna: descriptor del socket del cliente o -1 si hay error
         */
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, 
                              &client_addr_len);
        
        if (client_socket < 0) {
            fprintf(stderr, "[ERROR] No se pudo aceptar conexion: %s\n", strerror(errno));
            continue;
        }
        
        printf("[BROKER] Conexion aceptada de %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        /* 
         * Leer el primer mensaje para determinar si es publicador o suscriptor
         * Primera linea debe ser "PUBLISH|..." o "SUBSCRIBE|..."
         */
        char initial_message[MAX_MESSAGE_SIZE];
        memset(initial_message, 0, sizeof(initial_message));
        int bytes = recv(client_socket, initial_message, sizeof(initial_message) - 1, 0);
        
        if (bytes > 0) {
            initial_message[bytes] = '\0';
            
            if (strncmp(initial_message, "PUBLISH", 7) == 0) {
                /* Es un publicador - procesamos el primer mensaje y luego entramos en loop */
                printf("[BROKER] Cliente identificado como PUBLICADOR\n");
                
                /* Procesar el primer mensaje */
                char msg_copy[MAX_MESSAGE_SIZE];
                strcpy(msg_copy, initial_message);
                char *token = strtok(msg_copy, "|");
                if (token != NULL && strcmp(token, "PUBLISH") == 0) {
                    char topic[100] = {0};
                    char message[MAX_MESSAGE_SIZE - 100] = {0};
                    
                    token = strtok(NULL, "|");
                    if (token != NULL) strcpy(topic, token);
                    
                    token = strtok(NULL, "");
                    if (token != NULL) strcpy(message, token);
                    
                    if (strlen(topic) > 0 && strlen(message) > 0) {
                        forward_message_to_subscribers(topic, message);
                    }
                }
                
                /* Continuar recibiendo mensajes del publicador */
                handle_publisher(client_socket);
                
            } else if (strncmp(initial_message, "SUBSCRIBE", 9) == 0) {
                /* Es un suscriptor */
                printf("[BROKER] Cliente identificado como SUSCRIPTOR\n");
                
                char msg_copy[MAX_MESSAGE_SIZE];
                strcpy(msg_copy, initial_message);
                char *token = strtok(msg_copy, "|");
                if (token != NULL && strcmp(token, "SUBSCRIBE") == 0) {
                    token = strtok(NULL, "");
                    
                    if (token != NULL && num_subscribers < MAX_SUBSCRIBERS) {
                        subscribers[num_subscribers].socket = client_socket;
                        strcpy(subscribers[num_subscribers].topic, token);
                        num_subscribers++;
                        
                        printf("[BROKER] Suscriptor registrado para tema '%s'\n", token);
                        
                        char ack[100];
                        snprintf(ack, sizeof(ack), "Suscrito al tema: %s\n", token);
                        send(client_socket, ack, strlen(ack), 0);
                    }
                }
            } else {
                printf("[BROKER] Mensaje invalido, cerrando conexion\n");
                close(client_socket);
            }
        }
    }
    
    close(server_socket);
    return 0;
}
