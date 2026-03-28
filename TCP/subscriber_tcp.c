/*
 * TCP Pub-Sub Subscriber (Múltiples Partidos)
 * 
 * DESCRIPCION:
 * Cliente suscriptor que se conecta al broker y se suscribe a múltiples
 * partidos dinámicamente. El suscriptor recibe todos los mensajes publicados
 * en los partidos suscritos y los muestra en pantalla intercalados.
 * 
 * USO:
 * ./subscriber_tcp <broker_ip> <broker_port> <subscriber_id> <num_partidos>
 * Ejemplo (suscriptor a 3 partidos):
 * ./subscriber_tcp 127.0.0.1 9001 sub1 3
 * 
 * Esto genera automáticamente suscripciones a:
 * - match_1_vs_2
 * - match_3_vs_4
 * - match_5_vs_6
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
#define MAX_BUFFER_SIZE 1024
#define MAX_PARTIDOS 20

volatile int keep_running = 1;

/*
 * signal_handler()
 * Manejador para Ctrl+C (SIGINT)
 * Permite terminar la suscripcion de forma controlada
 */
void signal_handler(int sig) {
    (void)sig;  /* Suprimir advertencia de parámetro no utilizado */
    printf("\n[SUBSCRIBER] Recibido Ctrl+C, desconectando...\n");
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <broker_ip> <broker_port> <subscriber_id> <num_partidos>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 127.0.0.1 9001 sub1 3\n", argv[0]);
        fprintf(stderr, "\nEsto se suscribirá a 3 partidos:\n");
        fprintf(stderr, "  - match_1_vs_2\n");
        fprintf(stderr, "  - match_3_vs_4\n");
        fprintf(stderr, "  - match_5_vs_6\n");
        exit(EXIT_FAILURE);
    }
    
    const char *broker_ip = argv[1];
    int broker_port = atoi(argv[2]);
    const char *subscriber_id = argv[3];
    int num_partidos = atoi(argv[4]);
    
    /* Validar número de partidos */
    if (num_partidos <= 0 || num_partidos > MAX_PARTIDOS) {
        fprintf(stderr, "[ERROR] Número de partidos debe estar entre 1 y %d\n", MAX_PARTIDOS);
        exit(EXIT_FAILURE);
    }
    
    int subscriber_socket;
    struct sockaddr_in broker_addr;
    char message_buffer[MAX_BUFFER_SIZE];
    int bytes_received;
    
    /* Generar topics dinámicamente */
    char topics[MAX_PARTIDOS][100];
    for (int i = 0; i < num_partidos; i++) {
        int team1 = 2 * (i + 1) - 1;  /* 1, 3, 5, ... */
        int team2 = 2 * (i + 1);      /* 2, 4, 6, ... */
        snprintf(topics[i], 100, "match_%d_vs_%d", team1, team2);
    }
    
    printf("===== TCP PUB-SUB SUBSCRIBER =====\n");
    printf("Suscriptor ID: %s\n", subscriber_id);
    printf("Número de partidos: %d\n", num_partidos);
    printf("Partidos a suscribirse:\n");
    for (int i = 0; i < num_partidos; i++) {
        printf("  - %s\n", topics[i]);
    }
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
     
     /* Enviar mensaje de REGISTRO con nombre de suscriptor y cantidad de tópicos */
     printf("Enviando registro al broker...\n");
     memset(message_buffer, 0, sizeof(message_buffer));
     snprintf(message_buffer, sizeof(message_buffer), "REGISTER|%s|%d", subscriber_id, num_partidos);
     
     if (send(subscriber_socket, message_buffer, strlen(message_buffer), 0) < 0) {
         fprintf(stderr, "[ERROR] No se pudo enviar registro al broker: %s\n", strerror(errno));
     } else {
         printf("[OK] Registro enviado: %s\n\n", message_buffer);
     }
     
     /* Suscribirse a todos los partidos especificados */
     printf("Suscribiendo a partidos...\n\n");
     
     for (int i = 0; i < num_partidos; i++) {
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
