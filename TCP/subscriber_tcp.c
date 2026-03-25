/*
 * TCP Pub-Sub Subscriber
 * 
 * DESCRIPCION:
 * Cliente suscriptor que se conecta al broker y se suscribe a uno o más
 * temas específicos. El suscriptor recibe todos los mensajes publicados
 * en los temas suscritos y los muestra en pantalla.
 * 
 * USO:
 * ./subscriber_tcp <broker_ip> <broker_port> <subscriber_id> <topic1> [topic2] ...
 * Ejemplo (suscriptor a un tema):
 * ./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B
 * 
 * Ejemplo (suscriptor a múltiples temas):
 * ./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B match_C_vs_D
 * 
 * FUNCIONES SOCKET UTILIZADAS:
 * 1. socket() - Crea un socket TCP (SOCK_STREAM)
 * 2. connect() - Conecta el socket a la direccion del broker
 * 3. send() - Envía suscripciones al broker
 * 4. recv() - Recibe mensajes del broker
 * 5. close() - Cierra el socket
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

#define MAX_MESSAGE_SIZE 512

volatile int keep_running = 1;

/*
 * signal_handler()
 * Manejador para Ctrl+C (SIGINT)
 * Permite terminar la suscripcion de forma controlada
 */
void signal_handler(int sig) {
    printf("\n[SUBSCRIBER] Recibido Ctrl+C, desconectando...\n");
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Uso: %s <broker_ip> <broker_port> <subscriber_id> <topic1> [topic2] ...\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 127.0.0.1 9001 sub1 match_A_vs_B\n", argv[0]);
        fprintf(stderr, "Ejemplo (multiples temas): %s 127.0.0.1 9001 sub1 match_A_vs_B match_C_vs_D\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    const char *broker_ip = argv[1];
    int broker_port = atoi(argv[2]);
    const char *subscriber_id = argv[3];
    int num_topics = argc - 4;
    const char **topics = (const char **)&argv[4];
    
    int subscriber_socket;
    struct sockaddr_in broker_addr;
    char message_buffer[MAX_MESSAGE_SIZE];
    int bytes_received;
    
    printf("===== TCP PUB-SUB SUBSCRIBER =====\n");
    printf("Suscriptor ID: %s\n", subscriber_id);
    printf("Temas a suscribirse: ");
    for (int i = 0; i < num_topics; i++) {
        printf("%s", topics[i]);
        if (i < num_topics - 1) printf(", ");
    }
    printf("\n");
    printf("Conectando a broker en %s:%d...\n\n", broker_ip, broker_port);
    
    /* Configurar manejador de señales para Ctrl+C */
    signal(SIGINT, signal_handler);
    
    /*
     * socket() - Crear un socket TCP
     * Parametros:
     *   AF_INET: Familia de direcciones IPv4
     *   SOCK_STREAM: Tipo de socket TCP (confiable, orientado a conexion)
     *   0: Protocolo por defecto para SOCK_STREAM (TCP)
     * Retorna: descriptor del socket o -1 si hay error
     */
    subscriber_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (subscriber_socket < 0) {
        fprintf(stderr, "[ERROR] No se pudo crear el socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    printf("[OK] Socket creado\n");
    
    /* Configurar estructura de direccion del broker */
    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(broker_port);
    
    if (inet_aton(broker_ip, &broker_addr.sin_addr) == 0) {
        fprintf(stderr, "[ERROR] Direccion IP invalida: %s\n", broker_ip);
        close(subscriber_socket);
        exit(EXIT_FAILURE);
    }
    
    /*
     * connect() - Conectar el socket a la direccion del broker
     * Parametros:
     *   subscriber_socket: descriptor del socket
     *   (struct sockaddr *)&broker_addr: direccion del broker
     *   sizeof(broker_addr): tamaño de la estructura de direccion
     * Retorna: 0 si exito, -1 si hay error
     */
    if (connect(subscriber_socket, (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0) {
        fprintf(stderr, "[ERROR] No se pudo conectar al broker: %s\n", strerror(errno));
        close(subscriber_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("[OK] Conectado al broker %s:%d\n\n", broker_ip, broker_port);
    
    /* Suscribirse a todos los temas especificados */
    printf("Suscribiendo a temas...\n\n");
    
    for (int i = 0; i < num_topics; i++) {
        memset(message_buffer, 0, sizeof(message_buffer));
        
        /* Construir mensaje de suscripcion en formato "SUBSCRIBE|topic" */
        snprintf(message_buffer, sizeof(message_buffer), "SUBSCRIBE|%s", topics[i]);
        
        /*
         * send() - Enviar datos al broker
         * Parametros:
         *   subscriber_socket: descriptor del socket
         *   message_buffer: buffer con los datos a enviar
         *   strlen(message_buffer): cantidad de bytes a enviar
         *   0: flags (0 = sin opciones especiales)
         * Retorna: cantidad de bytes enviados o -1 si hay error
         */
        if (send(subscriber_socket, message_buffer, strlen(message_buffer), 0) < 0) {
            fprintf(stderr, "[ERROR] No se pudo enviar suscripcion a %s: %s\n", 
                    topics[i], strerror(errno));
        } else {
            printf("[%s] Suscrito al tema: %s\n", subscriber_id, topics[i]);
        }
    }
    
    printf("\n========================================\n");
    printf("Esperando mensajes (presiona Ctrl+C para salir)...\n");
    printf("========================================\n\n");
    
    /* Loop para recibir mensajes del broker */
    while (keep_running) {
        memset(message_buffer, 0, sizeof(message_buffer));
        
        /*
         * recv() - Recibir datos del broker
         * Parametros:
         *   subscriber_socket: descriptor del socket
         *   message_buffer: buffer donde almacenar los datos
         *   sizeof(message_buffer) - 1: cantidad maxima de bytes a recibir
         *   0: flags (0 = sin opciones especiales)
         * Retorna: cantidad de bytes recibidos, 0 si conexion cerrada, -1 si error
         */
        bytes_received = recv(subscriber_socket, message_buffer, sizeof(message_buffer) - 1, 0);
        
        if (bytes_received < 0) {
            fprintf(stderr, "[ERROR] Error al recibir mensaje: %s\n", strerror(errno));
            break;
        } else if (bytes_received == 0) {
            printf("[SUBSCRIBER] Broker cerro la conexion\n");
            break;
        } else {
            message_buffer[bytes_received] = '\0';
            printf("[%s] %s\n", subscriber_id, message_buffer);
        }
    }
    
    printf("\n[OK] Desconectando del broker...\n");
    
    /*
     * close() - Cerrar el socket
     * Parametros:
     *   subscriber_socket: descriptor del socket a cerrar
     * Retorna: 0 si exito, -1 si hay error
     */
    close(subscriber_socket);
    
    printf("[OK] Conexion cerrada\n");
    
    return 0;
}
