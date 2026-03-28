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
#include <time.h>

#define MAX_MESSAGE_SIZE 512
#define MAX_EVENT_MESSAGE 256
#define MAX_BUFFER_SIZE 1024
#define MAX_PARTIDOS 20

/*
 * Plantillas de mensajes deportivos para partidos
 * Los mensajes pueden incluir %d para el número de equipo
 * o ser independientes del equipo
 */
const char *message_templates[] = {
    /* Goles y acciones ofensivas */
    "¡GOL! Equipo %d abre el marcador al minuto 3",
    "¡GOL! Espectacular gol de equipo %d al minuto 12",
    "Tiro de equipo %d desviado por poco",
    "Remate de cabeza de %d fuera del arco",
    "¡GOL! Gol de penalti convertido por %d",
    "Equipo %d corre peligro de recibir gol",
    
    /* Tarjetas */
    "Tarjeta amarilla para número 7 de equipo %d",
    "Tarjeta roja directa para número 15 de equipo %d",
    "Segunda amarilla: expulsado jugador de equipo %d",
    "Amonestación a número 10 de %d por protesta",
    
    /* Cambios y estrategia */
    "Cambio en equipo %d: entra número 11 por número 9",
    "Cambio en equipo %d: sale número 5, entra número 23",
    "Equipo %d aumenta presión ofensiva",
    "Cambio defensivo en equipo %d para proteger resultado",
    "Triple cambio simultáneo en equipo %d",
    
    /* Momentos del partido */
    "Fuera de juego anulado a equipo %d",
    "Balón recuperado por defensa de %d",
    "Córner a favor de equipo %d",
    "Saque de banda para equipo %d",
    "Tiro libre directo para equipo %d",
    "Tiempo muerto solicitado por técnico de equipo %d",
    
    /* Estadísticas y análisis */
    "Primer tiempo: Equipo %d domina el juego",
    "Equipo %d incrementa velocidad de pases",
    "Racha de 5 remates consecutivos de equipo %d",
    "Posesión de balón: 60% equipo %d, 40% rival",
    "Centro de defensa de equipo %d realiza gran bloqueo",
    
    /* Momentos críticos */
    "¡Gran jugada! Equipo %d se acerca al arco rival",
    "Defensa de %d hace falta para evitar gol seguro",
    "Equipo %d pierde balón en zona peligrosa",
    "Portero de %d hace parada espectacular",
    "¡Atajada milagrosa del portero de equipo %d!",
    
    /* Contexto del partido */
    "HALFTIME: Primera mitad finaliza",
    "Segundo tiempo inicia con intensidad",
    "Equipos regresan al terreno de juego",
    "FINAL: ¡El partido ha terminado!",
    "Tension máxima en los últimos minutos",
    "Equipo %d presiona en busca del empate",
    "Equipo %d se defiende contra la avalancha rival",
    "Pitch invasion: Aficionados invaden el campo",
    "Revisor VAR analiza jugada controversial",
    "Gol confirmado tras revisión del VAR"
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
    
    /* Enviar mensajes al broker - ALEATORIO, no secuencial */
    printf("Enviando 20 mensajes aleatorios...\n\n");
    
    /* Inicializar seed del generador de números aleatorios */
    srand(time(NULL) + getpid());
    
    /* Array para rastrear qué mensajes ya se han usado */
    int used_messages[NUM_MESSAGES] = {0};
    int messages_sent = 0;
    
    while (messages_sent < 20) {
        memset(message_buffer, 0, sizeof(message_buffer));
        
        char event_message[MAX_EVENT_MESSAGE];
        memset(event_message, 0, sizeof(event_message));
        
        /* Seleccionar un mensaje aleatorio que no haya sido usado */
        int random_idx;
        do {
            random_idx = rand() % NUM_MESSAGES;
        } while (used_messages[random_idx] && messages_sent < NUM_MESSAGES);
        
        /* Si ya hemos usado todos los mensajes disponibles, reusarlos */
        if (messages_sent >= NUM_MESSAGES) {
            random_idx = rand() % NUM_MESSAGES;
        } else {
            used_messages[random_idx] = 1;
        }
        
        /* Decidir aleatoriamente si este mensaje incluye un equipo o no */
        int has_team = (strstr(message_templates[random_idx], "%d") != NULL);
        
        if (has_team) {
            /* Alternar entre equipo1 y equipo2 */
            int team = (messages_sent % 2 == 0) ? team1 : team2;
            generate_message(event_message, sizeof(event_message), 
                           message_templates[random_idx], team);
        } else {
            /* Mensaje sin equipo específico */
            generate_message(event_message, sizeof(event_message), 
                           message_templates[random_idx], 0);
        }
        
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
            fprintf(stderr, "[ERROR] No se pudo enviar mensaje %d: %s\n", 
                    messages_sent + 1, strerror(errno));
        } else {
            printf("[%s] Mensaje %d enviado: %s\n", 
                   publisher_id, messages_sent + 1, event_message);
        }
        
        messages_sent++;
        
        /* Pausa aleatoria entre 0.5 y 2.5 segundos para simular eventos reales */
        int random_delay = (rand() % 21) + 5; /* 5-25 decisimas de segundo */
        usleep(random_delay * 100000);  /* Convertir a microsegundos */
    }
    
    printf("\n[OK] Los 20 mensajes fueron enviados\n");
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
