/*
 * TCP Pub-Sub Publisher (Múltiples Partidos Dinámicos)
 * 
 * DESCRIPCION:
 * Cliente publicador que se conecta al broker y envía mensajes
 * sobre un partido específico. El publicador envía al menos 10 mensajes
 * y luego se desconecta. Los partidos se identifican por número.
 * 
 * USO:
 * ./publisher_tcp <broker_ip> <broker_port> <publisher_id> <num_partido>
 * Ejemplo:
 * ./publisher_tcp 127.0.0.1 9001 pub1 1
 * 
 * Esto publica en: match_1_vs_2 (equipos: 1 vs 2)
 * 
 * Mapeo de partidos:
 * - Entrada 1 → match_1_vs_2 (equipos 1 y 2)
 * - Entrada 2 → match_3_vs_4 (equipos 3 y 4)
 * - Entrada 3 → match_5_vs_6 (equipos 5 y 6)
 * - Entrada i → match_{2i-1}_vs_{2i} (equipos 2i-1 y 2i)
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
#define MAX_EVENT_MESSAGE 256
#define MAX_BUFFER_SIZE 1024
#define MAX_PARTIDOS 20

/*
 * Plantillas de mensajes que se personalizan con los números de equipo
 */
const char *message_templates[] = {
    "Gol de %d al minuto 5",
    "Saque de banda para %d",
    "Tarjeta amarilla al numero 7 de %d",
    "Cambio: jugador 10 entra por jugador 20 en %d",
    "Gol de %d al minuto 23",
    "Fuera de juego anulado",
    "Tarjeta roja al numero 15 de %d",
    "Gol de %d al minuto 45",
    "Fin del primer tiempo",
    "Inicia el segundo tiempo",
    "Gol de %d al minuto 67",
    "Cambio: jugador 5 entra por jugador 3 en %d",
    "Empate 2-2 en el marcador",
    "Ultimos minutos del partido",
    "Fin del partido 3-2 para %d"
};

#define NUM_MESSAGES (sizeof(message_templates) / sizeof(message_templates[0]))

/*
 * generate_message() - Genera un mensaje personalizado con el número del equipo
 * Parametros:
 *   buffer: buffer donde guardar el mensaje generado
 *   size: tamaño del buffer
 *   template: plantilla del mensaje
 *   team: número del equipo a insertar (o 0 si no aplica)
 * Retorna: void
 */
void generate_message(char *buffer, size_t size, const char *template, int team) {
    if (strstr(template, "%d") != NULL) {
        /* El mensaje tiene un placeholder %d para el equipo */
        snprintf(buffer, size, template, team);
    } else {
        /* El mensaje no tiene placeholders */
        snprintf(buffer, size, "%s", template);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <broker_ip> <broker_port> <publisher_id> <num_partido>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 127.0.0.1 9001 pub1 1\n", argv[0]);
        fprintf(stderr, "\nMapeo de partidos:\n");
        fprintf(stderr, "  1 → match_1_vs_2 (equipos 1 y 2)\n");
        fprintf(stderr, "  2 → match_3_vs_4 (equipos 3 y 4)\n");
        fprintf(stderr, "  3 → match_5_vs_6 (equipos 5 y 6)\n");
        exit(EXIT_FAILURE);
    }
    
    const char *broker_ip = argv[1];
    int broker_port = atoi(argv[2]);
    const char *publisher_id = argv[3];
    int num_partido = atoi(argv[4]);
    
    /* Validar número de partido */
    if (num_partido <= 0 || num_partido > MAX_PARTIDOS) {
        fprintf(stderr, "[ERROR] Número de partido debe estar entre 1 y %d\n", MAX_PARTIDOS);
        exit(EXIT_FAILURE);
    }
    
    int publisher_socket;
    struct sockaddr_in broker_addr;
    char message_buffer[MAX_BUFFER_SIZE];
    
    /* Calcular equipos y topic basado en número de partido */
    int team1 = 2 * num_partido - 1;  /* 1, 3, 5, ... */
    int team2 = 2 * num_partido;      /* 2, 4, 6, ... */
    char topic[100];
    snprintf(topic, 100, "match_%d_vs_%d", team1, team2);
    
    printf("===== TCP PUB-SUB PUBLISHER =====\n");
    printf("Publicador ID: %s\n", publisher_id);
    printf("Número de partido: %d\n", num_partido);
    printf("Tema: %s\n", topic);
    printf("Equipos: %d vs %d\n", team1, team2);
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
        
        char event_message[MAX_EVENT_MESSAGE];
        memset(event_message, 0, sizeof(event_message));
        
        /* Generar mensaje personalizado con el equipo correspondiente */
        /* Alternar entre equipo1 y equipo2 para que ambos tengan acciones */
        int team = (i % 2 == 0) ? team1 : team2;
        generate_message(event_message, sizeof(event_message), message_templates[i], team);
        
        /* Construir mensaje en formato "PUBLISH|topic|mensaje" */
        snprintf(message_buffer, sizeof(message_buffer), "PUBLISH|%s|%s", 
                 topic, event_message);
        
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
            printf("[%s] Mensaje %lu enviado: %s\n", publisher_id, i + 1, event_message);
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
