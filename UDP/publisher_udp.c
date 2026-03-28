#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

/**
 * publisher_udp.c
 * Este programa representa al publisher del sistema pub-sub por UDP.
 * Su trabajo es generar eventos de un partido especifico y enviarlos
 * al broker con el formato PUBLISH|topic|contenido.
 *
 * Secuencia de ejecucion de este programa:
 * 1. Lee ip, puerto, nombre y numero de partido desde consola.
 * 2. Calcula el topic del partido y los equipos correspondientes.
 * 3. Crea el socket UDP del publisher.
 * 4. Genera eventos de ejemplo del partido.
 * 5. Envia cada evento al broker con formato PUBLISH|topic|contenido.
 * 6. Espera un segundo entre mensajes para simular tiempo real.
 */

#define MAX_TOPIC_SIZE 100
#define MAX_MESSAGE_SIZE 1200
#define MAX_EVENTS 10
#define MAX_MATCHES 20

//construir el topic del partido con el formato match_X_vs_Y
static void build_match_topic(int match_number, char *topic, size_t topic_size) {
    int team_one = (2 * match_number) - 1;
    int team_two = 2 * match_number;
    snprintf(topic, topic_size, "match_%d_vs_%d", team_one, team_two);
}

//generar mensajes de ejemplo para simular eventos del partido
static void build_match_event(int event_number, int team_one, int team_two,
                              char *event, size_t event_size) {
    switch (event_number) {
        case 0:
            snprintf(event, event_size,
                     "Inicio del partido entre equipo %d y equipo %d",
                     team_one, team_two);
            break;
        case 1:
            snprintf(event, event_size,
                     "Minuto 12: llegada peligrosa del equipo %d",
                     team_one);
            break;
        case 2:
            snprintf(event, event_size,
                     "Minuto 18: atajada clave frente al equipo %d",
                     team_two);
            break;
        case 3:
            snprintf(event, event_size,
                     "Minuto 27: tiro de esquina para el equipo %d",
                     team_one);
            break;
        case 4:
            snprintf(event, event_size,
                     "Minuto 34: tarjeta amarilla para el equipo %d",
                     team_two);
            break;
        case 5:
            snprintf(event, event_size,
                     "Minuto 45: termina el primer tiempo %d vs %d",
                     team_one, team_two);
            break;
        case 6:
            snprintf(event, event_size,
                     "Minuto 52: gol del equipo %d",
                     team_one);
            break;
        case 7:
            snprintf(event, event_size,
                     "Minuto 67: empate del equipo %d",
                     team_two);
            break;
        case 8:
            snprintf(event, event_size,
                     "Minuto 81: cambio tactico en el equipo %d",
                     team_one);
            break;
        default:
            snprintf(event, event_size,
                     "Final del partido entre equipo %d y equipo %d",
                     team_one, team_two);
            break;
    }
}

int main(int argc, char *argv[]) {
    //el publisher recibe ip, puerto, nombre y numero de partido
    if (argc != 5) {
        printf("Uso: %s <broker_ip> <broker_port> <publisher_name> <match_number>\n", argv[0]);
        return 1;
    }

    //guardar los argumentos en variables mas faciles de manejar
    char *serverName = argv[1];
    int serverPort = atoi(argv[2]);
    char *publisherName = argv[3];
    int matchNumber = atoi(argv[4]);

    //validar que el numero de partido este dentro del rango permitido
    if (matchNumber < 1 || matchNumber > MAX_MATCHES) {
        printf("match_number debe estar entre 1 y %d\n", MAX_MATCHES);
        return 1;
    }

    /**
     * socket() crea el socket UDP del publisher.
     * Este socket se usa para enviar datagramas al broker.
     * No mantiene una conexion continua como en TCP.
     */
    int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (clientSocket < 0) {
        perror("Error al crear el socket del publisher");
        return 1;
    }

    //direccion del broker a donde se enviaran las publicaciones
    struct sockaddr_in serverAddr;
    //inicializar en cero para evitar basura en memoria
    memset(&serverAddr, 0, sizeof(serverAddr));
    //indicar que la direccion es IPv4
    serverAddr.sin_family = AF_INET;
    //guardar el puerto del broker en formato de red
    serverAddr.sin_port = htons(serverPort);
    //convertir la ip del broker de texto a formato binario
    serverAddr.sin_addr.s_addr = inet_addr(serverName);

    //topic del partido, mensaje completo a enviar y texto del evento
    char topic[MAX_TOPIC_SIZE];
    char message[MAX_MESSAGE_SIZE];
    char event[1024];

    //calcular los equipos a partir del numero del partido
    int team_one = (2 * matchNumber) - 1;
    int team_two = 2 * matchNumber;

    //construir el topic automaticamente usando el numero de partido
    build_match_topic(matchNumber, topic, sizeof(topic));

    for (int i = 0; i < MAX_EVENTS; i++) {
        //generar el evento y armar el mensaje en formato PUBLISH|topic|contenido
        build_match_event(i, team_one, team_two, event, sizeof(event));
        snprintf(message, sizeof(message), "PUBLISH|%s|%s", topic, event);

        //enviar la publicacion al broker
        int sent = sendto(clientSocket, message, strlen(message), 0,
                          (struct sockaddr *)&serverAddr, sizeof(serverAddr));

        if (sent < 0) {
            perror("Error al enviar publicacion");
        } else {
            //mostrar en consola que evento fue enviado correctamente
            printf("[%s] Mensaje %d enviado: %s\n", publisherName, i + 1, event);
        }

        //esperar un segundo entre eventos para simular tiempo real
        sleep(1);
    }

    //cerrar el socket cuando el publisher termina de enviar todos sus eventos
    close(clientSocket);
    return 0;
}
