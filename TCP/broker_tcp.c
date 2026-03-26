/*
 * TCP Pub-Sub Broker (Versión Multi-threaded)
 * 
 * DESCRIPCION:
 * Servidor broker que implementa un modelo de publicacion-suscripcion usando TCP.
 * El broker recibe mensajes de publicadores y los redistribuye a los suscriptores
 * interesados en el tema correspondiente.
 * 
 * CARACTERISTICAS:
 * - Maneja múltiples publicadores concurrentemente usando threads POSIX (pthread)
 * - Cada publisher se ejecuta en su propio thread
 * - Usa mutex para proteger acceso a la lista de subscribers
 * - Soporta topic-based routing (filtrado por tema)
 * 
 * FUNCIONES SOCKET UTILIZADAS:
 * 1. socket() - Crea un socket TCP (SOCK_STREAM)
 * 2. bind() - Vincula el socket a una direccion IP y puerto
 * 3. listen() - Pone el socket en modo escucha para aceptar conexiones
 * 4. accept() - Acepta una conexion entrante de un cliente
 * 5. recv() - Recibe datos del socket
 * 6. send() - Envía datos a través del socket
 * 7. close() - Cierra el socket
 * 
 * FUNCIONES PTHREAD UTILIZADAS:
 * 1. pthread_create() - Crea un nuevo thread
 * 2. pthread_join() - Espera a que un thread termine
 * 3. pthread_mutex_init() - Inicializa un mutex
 * 4. pthread_mutex_lock() - Adquiere un mutex
 * 5. pthread_mutex_unlock() - Libera un mutex
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>

#define BROKER_PORT 9001
#define MAX_MESSAGE_SIZE 512
#define MAX_BUFFER_SIZE 1024
#define MAX_SUBSCRIBERS 100
#define MAX_TOPICS 20
#define BACKLOG 5

/* Estructura para almacenar informacion de suscriptores */
typedef struct {
    int socket;
    char topic[100];
} subscriber_t;

/* Estructura para pasar parámetros al thread del publisher */
typedef struct {
    int socket;
} publisher_args_t;

/* Variables globales para gestionar suscriptores */
subscriber_t subscribers[MAX_SUBSCRIBERS];
int num_subscribers = 0;

/*
 * Mutex para proteger acceso a la lista de subscribers
 * Necesario porque múltiples threads pueden acceder simultáneamente
 */
pthread_mutex_t subscribers_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * forward_message_to_subscribers()
 * Recibe un mensaje de un publicador y lo reenvía a todos los suscriptores
 * del tema correspondiente.
 * 
 * Parametros:
 *   - topic: tema del mensaje
 *   - message: contenido del mensaje a reenviar
 * 
 * Nota: Esta función usa el mutex para evitar race conditions
 */
void forward_message_to_subscribers(const char *topic, const char *message) {
    printf("[BROKER] Reenviando mensaje del tema '%s' a suscriptores\n", topic);
    
    /* Adquirir el mutex antes de acceder a subscribers */
    pthread_mutex_lock(&subscribers_mutex);
    
    for (int i = 0; i < num_subscribers; i++) {
        if (strcmp(subscribers[i].topic, topic) == 0) {
            char full_message[MAX_BUFFER_SIZE];
            snprintf(full_message, sizeof(full_message), "[%s] %s", topic, message);
            
            if (send(subscribers[i].socket, full_message, strlen(full_message), 0) < 0) {
                fprintf(stderr, "[ERROR] No se pudo enviar mensaje al suscriptor\n");
            } else {
                printf("[BROKER] Mensaje enviado al suscriptor en socket %d\n", subscribers[i].socket);
            }
        }
    }
    
    /* Liberar el mutex */
    pthread_mutex_unlock(&subscribers_mutex);
}

/*
 * handle_publisher()
 * Gestiona la conexion de un publicador en un thread separado.
 * Recibe mensajes en formato "PUBLISH|topic|message" y los reenvía
 * a los suscriptores correspondientes.
 * 
 * Parametros:
 *   - arg: puntero a publisher_args_t que contiene el socket del publisher
 * 
 * Retorna: NULL (requerido para funciones thread de pthread)
 */
void *handle_publisher(void *arg) {
    publisher_args_t *args = (publisher_args_t *)arg;
    int publisher_socket = args->socket;
    free(args); /* Liberar la estructura de argumentos */
    
    char buffer[MAX_MESSAGE_SIZE];
    int bytes_received;
    
    printf("[BROKER] Nuevo publicador en thread %lu, socket %d\n", pthread_self(), publisher_socket);
    
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        bytes_received = recv(publisher_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            printf("[BROKER] Publicador desconectado (socket %d, thread %lu)\n", 
                   publisher_socket, pthread_self());
            break;
        }
        
        buffer[bytes_received] = '\0';
        printf("[BROKER] [Thread %lu] Mensaje recibido: %s\n", pthread_self(), buffer);
        
        /* Parsear mensaje en formato "PUBLISH|topic|message" */
        char buffer_copy[MAX_MESSAGE_SIZE];
        strcpy(buffer_copy, buffer);
        char *token = strtok(buffer_copy, "|");
        
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
    return NULL;
}

/*
 * register_subscriber()
 * Registra un nuevo suscriptor en la lista global.
 * Usa mutex para evitar race conditions.
 * 
 * Parametros:
 *   - socket: socket del suscriptor
 *   - topic: tema al que se suscribe
 * 
 * Retorna: 1 si se registró exitosamente, 0 si la lista está llena
 */
int register_subscriber(int socket, const char *topic) {
    pthread_mutex_lock(&subscribers_mutex);
    
    if (num_subscribers >= MAX_SUBSCRIBERS) {
        pthread_mutex_unlock(&subscribers_mutex);
        return 0;
    }
    
    subscribers[num_subscribers].socket = socket;
    strcpy(subscribers[num_subscribers].topic, topic);
    num_subscribers++;
    
    pthread_mutex_unlock(&subscribers_mutex);
    return 1;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    
    printf("===== TCP PUB-SUB BROKER (Multi-threaded) =====\n");
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
    printf("Esperando conexiones de publicadores y suscriptores...\n");
    printf("(Presiona Ctrl+C para detener el broker)\n\n");
    
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
                /* 
                 * Es un publicador - crear un thread para manejarlo
                 * Esto permite que múltiples publicadores se ejecuten concurrentemente
                 */
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
                
                /* 
                 * pthread_create() - Crear un nuevo thread
                 * Parametros:
                 *   NULL: variable donde guardar el ID del thread (no usado)
                 *   NULL: atributos del thread (NULL = atributos por defecto)
                 *   handle_publisher: función que ejecutará el thread
                 *   args: argumentos a pasar a la función
                 * Retorna: 0 si exito, número de error si falla
                 */
                pthread_t publisher_thread;
                publisher_args_t *args = malloc(sizeof(publisher_args_t));
                if (args != NULL) {
                    args->socket = client_socket;
                    
                    if (pthread_create(&publisher_thread, NULL, handle_publisher, args) == 0) {
                        printf("[BROKER] Thread creado para publicador (ID: %lu)\n", publisher_thread);
                        /* Detach el thread para que se limpie automáticamente */
                        pthread_detach(publisher_thread);
                    } else {
                        fprintf(stderr, "[ERROR] No se pudo crear thread: %s\n", strerror(errno));
                        free(args);
                        close(client_socket);
                    }
                } else {
                    fprintf(stderr, "[ERROR] No se pudo asignar memoria para argumentos del thread\n");
                    close(client_socket);
                }
                
            } else if (strncmp(initial_message, "SUBSCRIBE", 9) == 0) {
                /* Es un suscriptor - registrarlo en la lista global */
                printf("[BROKER] Cliente identificado como SUSCRIPTOR\n");
                
                char msg_copy[MAX_MESSAGE_SIZE];
                strcpy(msg_copy, initial_message);
                char *token = strtok(msg_copy, "|");
                if (token != NULL && strcmp(token, "SUBSCRIBE") == 0) {
                    token = strtok(NULL, "");
                    
                    if (token != NULL) {
                        if (register_subscriber(client_socket, token)) {
                            printf("[BROKER] Suscriptor registrado para tema '%s'\n", token);
                            
                            char ack[100];
                            snprintf(ack, sizeof(ack), "Suscrito al tema: %s\n", token);
                            send(client_socket, ack, strlen(ack), 0);
                            
                            /* 
                             * El suscriptor permanece conectado.
                             * Espera a recibir mensajes en el socket.
                             * Los mensajes son enviados por otros threads
                             * que ejecutan handle_publisher()
                             */
                        } else {
                            fprintf(stderr, "[ERROR] No se pudo registrar suscriptor (lista llena)\n");
                            close(client_socket);
                        }
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
