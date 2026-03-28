#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

/**
 * subscriber_udp.c
 * Este programa representa al subscriber del sistema pub-sub por UDP.
 * Su trabajo es suscribirse a uno o varios topics y luego quedarse
 * esperando los mensajes que el broker le reenvia.
 *
 * Secuencia de ejecucion del sistema:
 * 1. Primero se ejecuta el broker.
 * 2. Despues se ejecuta el subscriber para registrarse en los topics.
 * 3. Finalmente se ejecutan uno o varios publishers para enviar eventos.
 */

#define MAX_TOPIC_SIZE 100
#define MAX_SUBSCRIPTION_MESSAGE_SIZE 128
#define MAX_RECEIVE_BUFFER_SIZE 2048
#define MAX_MATCHES 20

//construir el nombre del topic a partir del numero del partido
static void build_match_topic(int match_number, char *topic, size_t topic_size) {
    int team_one = (2 * match_number) - 1;
    int team_two = 2 * match_number;
    snprintf(topic, topic_size, "match_%d_vs_%d", team_one, team_two);
}

int main(int argc, char *argv[]) {
    //el subscriber recibe por consola la ip, puerto, nombre y cantidad de partidos
    if (argc != 5) {
        printf("Uso: %s <broker_ip> <broker_port> <subscriber_name> <num_matches>\n", argv[0]);
        return 1;
    }

    //guardar los argumentos de entrada en variables mas faciles de usar
    char *serverName = argv[1];
    int serverPort = atoi(argv[2]);
    char *subscriberName = argv[3];
    int numMatches = atoi(argv[4]);

    //validar que el numero de partidos este en el rango permitido
    if (numMatches < 1 || numMatches > MAX_MATCHES) {
        printf("num_matches debe estar entre 1 y %d\n", MAX_MATCHES);
        return 1;
    }

    //mostrar por consola la configuracion con la que arranca el subscriber
    printf("===== UDP PUB-SUB SUBSCRIBER =====\n");
    printf("Suscriptor ID: %s\n", subscriberName);
    printf("Numero de partidos: %d\n", numMatches);
    printf("Partidos a suscribirse:\n");

    for (int i = 1; i <= numMatches; i++) {
        char topic[MAX_TOPIC_SIZE];
        build_match_topic(i, topic, sizeof(topic));
        printf("  - %s\n", topic);
    }

    printf("Conectando a broker en %s:%d...\n\n", serverName, serverPort);

    /**
     * socket() crea el socket UDP del subscriber.
     * Como es UDP no hay conexion persistente como en TCP;
     * este socket se usara para enviar suscripciones y recibir
     * datagramas reenviados por el broker.
     */
    int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (clientSocket < 0) {
        perror("Error al crear el socket del subscriber");
        return 1;
    }

    printf("[OK] Socket creado\n");

    //direccion del broker a donde se enviaran las suscripciones
    struct sockaddr_in serverAddr;
    //llenar la estructura con 0 para evitar basura en memoria
    memset(&serverAddr, 0, sizeof(serverAddr));
    //indicar que la direccion es IPv4
    serverAddr.sin_family = AF_INET;
    //guardar el puerto del broker en formato de red
    serverAddr.sin_port = htons(serverPort);
    //convertir la ip escrita como texto al formato que usa el socket
    serverAddr.sin_addr.s_addr = inet_addr(serverName);

    printf("[OK] Broker configurado en %s:%d\n\n", serverName, serverPort);
    printf("Suscribiendo a partidos...\n\n");

    for (int i = 1; i <= numMatches; i++) {
        //topic de ese partido y mensaje SUBSCRIBE que se enviara al broker
        char topic[MAX_TOPIC_SIZE];
        char subscriptionMessage[MAX_SUBSCRIPTION_MESSAGE_SIZE];

        //generar el topic automaticamente con base en el numero de partido
        build_match_topic(i, topic, sizeof(topic));

        //formato del mensaje de suscripcion: SUBSCRIBE|topic
        snprintf(subscriptionMessage, sizeof(subscriptionMessage), "SUBSCRIBE|%s", topic);

        //enviar la suscripcion al broker
        int sent = sendto(clientSocket, subscriptionMessage, strlen(subscriptionMessage), 0,
                          (struct sockaddr *)&serverAddr, sizeof(serverAddr));

        if (sent < 0) {
            perror("Error al enviar suscripcion");
            close(clientSocket);
            return 1;
        }

        printf("[%s] Suscrito al tema: %s\n", subscriberName, topic);
    }

    printf("\n========================================\n");
    printf("Esperando mensajes (presiona Ctrl+C para salir)...\n");
    printf("========================================\n\n");

    while (1) {
        //buffer donde se guarda cada publicacion recibida
        char receivedMessage[MAX_RECEIVE_BUFFER_SIZE];
        //copia auxiliar para separar topic y contenido con strtok
        char receivedCopy[MAX_RECEIVE_BUFFER_SIZE];
        //tamaño de la direccion del broker
        socklen_t serverLen = sizeof(serverAddr);

        //esperar una publicacion reenviada por el broker
        int n = recvfrom(clientSocket, receivedMessage, sizeof(receivedMessage) - 1, 0,
                         (struct sockaddr *)&serverAddr, &serverLen);

        if (n < 0) {
            perror("Error al recibir mensaje");
            break;
        }

        //agregar el fin de string para tratar el buffer como texto
        receivedMessage[n] = '\0';

        //hacer una copia para poder tokenizar sin perder el mensaje original
        strncpy(receivedCopy, receivedMessage, sizeof(receivedCopy) - 1);
        receivedCopy[sizeof(receivedCopy) - 1] = '\0';

        //separar topic y contenido de la publicacion recibida
        char *topic = strtok(receivedCopy, "|");
        char *content = strtok(NULL, "");

        //si el mensaje viene en el formato esperado, mostrar topic y contenido
        if (topic != NULL && content != NULL) {
            printf("[%s] Mensaje de %s: %s\n", subscriberName, topic, content);
        } else {
            //si llega algo inesperado, imprimirlo completo para depuracion
            printf("[%s] Publicacion recibida: %s\n", subscriberName, receivedMessage);
        }
    }

    //cerrar el socket cuando el subscriber termine
    close(clientSocket);
    return 0;
}
