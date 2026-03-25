/*
 * TCP Pub-Sub Publisher
 * 
 * DESCRIPCION:
 * Cliente publicador que se conecta al broker y envía mensajes
 * sobre un tema específico. El publicador envía al menos 10 mensajes
 * y luego se desconecta.
 * 
 * USO:
 * ./publisher_tcp <broker_ip> <broker_port> <publisher_id> <topic>
 * Ejemplo:
 * ./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B
 * 
 * FUNCIONES SOCKET UTILIZADAS:
 * 1. socket() - Crea un socket TCP (SOCK_STREAM)
 * 2. connect() - Conecta el socket a la direccion del broker
 * 3. send() - Envía mensajes al broker
 * 4. close() - Cierra el socket
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_MESSAGE_SIZE 512

/*
 * Mensajes de ejemplo para eventos deportivos
 */
const char *sports_messages[] = {
    "Gol de Equipo A al minuto 5",
    "Saque de banda para Equipo A",
    "Tarjeta amarilla al numero 7 de Equipo B",
    "Cambio: jugador 10 entra por jugador 20 en Equipo A",
    "Gol de Equipo B al minuto 23",
    "Fuera de juego anulado",
    "Tarjeta roja al numero 15 de Equipo A",
    "Gol de Equipo A al minuto 45",
    "Fin del primer tiempo",
    "Inicia el segundo tiempo",
    "Gol de Equipo B al minuto 67",
    "Cambio: jugador 5 entra por jugador 3 en Equipo B",
    "Empate 2-2 en el marcador",
    "Ultimos minutos del partido",
    "Fin del partido 3-2 para Equipo A"
};

#define NUM_MESSAGES (sizeof(sports_messages) / sizeof(sports_messages[0]))

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <broker_ip> <broker_port> <publisher_id> <topic>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 127.0.0.1 9001 pub1 match_A_vs_B\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    const char *broker_ip = argv[1];
    int broker_port = atoi(argv[2]);
    const char *publisher_id = argv[3];
    const char *topic = argv[4];
    
    int publisher_socket;
    struct sockaddr_in broker_addr;
    char message_buffer[MAX_MESSAGE_SIZE];
    
    printf("===== TCP PUB-SUB PUBLISHER =====\n");
    printf("Publicador ID: %s\n", publisher_id);
    printf("Tema: %s\n", topic);
    printf("Conectando a broker en %s:%d...\n\n", broker_ip, broker_port);
    
    /*
     * socket() - Crear un socket TCP
     * Parametros:
     *   AF_INET: Familia de direcciones IPv4
     *   SOCK_STREAM: Tipo de socket TCP (confiable, orientado a conexion)
     *   0: Protocolo por defecto para SOCK_STREAM (TCP)
     * Retorna: descriptor del socket o -1 si hay error
     */
    publisher_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (publisher_socket < 0) {
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
        close(publisher_socket);
        exit(EXIT_FAILURE);
    }
    
    /*
     * connect() - Conectar el socket a la direccion del broker
     * Parametros:
     *   publisher_socket: descriptor del socket
     *   (struct sockaddr *)&broker_addr: direccion del broker
     *   sizeof(broker_addr): tamaño de la estructura de direccion
     * Retorna: 0 si exito, -1 si hay error
     */
    if (connect(publisher_socket, (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0) {
        fprintf(stderr, "[ERROR] No se pudo conectar al broker: %s\n", strerror(errno));
        close(publisher_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("[OK] Conectado al broker %s:%d\n\n", broker_ip, broker_port);
    
    /* Enviar mensajes al broker */
    printf("Enviando %lu mensajes...\n\n", NUM_MESSAGES);
    
    for (size_t i = 0; i < NUM_MESSAGES; i++) {
        memset(message_buffer, 0, sizeof(message_buffer));
        
        /* Construir mensaje en formato "PUBLISH|topic|mensaje" */
        snprintf(message_buffer, sizeof(message_buffer), "PUBLISH|%s|%s", 
                 topic, sports_messages[i]);
        
        /*
         * send() - Enviar datos al broker
         * Parametros:
         *   publisher_socket: descriptor del socket
         *   message_buffer: buffer con los datos a enviar
         *   strlen(message_buffer): cantidad de bytes a enviar
         *   0: flags (0 = sin opciones especiales)
         * Retorna: cantidad de bytes enviados o -1 si hay error
         */
        if (send(publisher_socket, message_buffer, strlen(message_buffer), 0) < 0) {
            fprintf(stderr, "[ERROR] No se pudo enviar mensaje %lu: %s\n", i + 1, strerror(errno));
        } else {
            printf("[%s] Mensaje %lu enviado: %s\n", publisher_id, i + 1, sports_messages[i]);
        }
        
        /* Pequena pausa entre mensajes para simular eventos en vivo */
        sleep(1);
    }
    
    printf("\n[OK] Todos los %lu mensajes fueron enviados\n", NUM_MESSAGES);
    printf("Desconectando del broker...\n");
    
    /*
     * close() - Cerrar el socket
     * Parametros:
     *   publisher_socket: descriptor del socket a cerrar
     * Retorna: 0 si exito, -1 si hay error
     */
    close(publisher_socket);
    
    printf("[OK] Conexion cerrada\n");
    
    return 0;
}
