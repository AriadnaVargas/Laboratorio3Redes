#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

/**
 * broker_udp.c
 * Este programa representa al broker del sistema pub-sub por UDP.
 * Su trabajo es recibir suscripciones y publicaciones, guardar que
 * clientes estan suscritos a cada topic y reenviar cada publicacion
 * a los subscribers correspondientes.
 *
 * Secuencia de ejecucion de este programa:
 * 1. Lee el puerto desde consola o usa el puerto por defecto.
 * 2. Crea el socket UDP del broker.
 * 3. Hace bind() para quedar escuchando en ese puerto.
 * 4. Entra en un ciclo infinito esperando datagramas.
 * 5. Si recibe SUBSCRIBE, guarda la suscripcion.
 * 6. Si recibe PUBLISH, reenvia el mensaje a los subscribers del topic.
 */

#define MAX_BUFFER_SIZE 2048
#define MAX_TOPIC_SIZE 100
#define MAX_SUBSCRIBERS 100
#define MAX_PUBLISHERS 100

//cada subscriber guarda 2 cosas: el topic, y la direccion de red del cliente
struct Subscriber {
    char topic[MAX_TOPIC_SIZE];
    struct sockaddr_in addr;
};

//cada topic queda asociado a un solo publisher
struct Publisher {
    char topic[MAX_TOPIC_SIZE];
    struct sockaddr_in addr;
};

//validando si un cliente ya estaba suscrito a un tema. Para evitar duplicados
int subscriber_exists(struct Subscriber subscribers[], int subscriber_count,
                      const char *topic, struct sockaddr_in clientAddr) {
    for (int i = 0; i < subscriber_count; i++) {
        if (strcmp(subscribers[i].topic, topic) == 0 &&
            subscribers[i].addr.sin_addr.s_addr == clientAddr.sin_addr.s_addr &&
            subscribers[i].addr.sin_port == clientAddr.sin_port) {
            return 1;
        }
    }

    return 0;
}

//buscar si el topic ya tiene un publisher asignado
int find_publisher_for_topic(struct Publisher publishers[], int publisher_count,
                             const char *topic) {
    for (int i = 0; i < publisher_count; i++) {
        if (strcmp(publishers[i].topic, topic) == 0) {
            return i;
        }
    }

    return -1;
}

//validar si dos direcciones corresponden al mismo cliente UDP
int same_udp_client(struct sockaddr_in addr1, struct sockaddr_in addr2) {
    return addr1.sin_addr.s_addr == addr2.sin_addr.s_addr &&
           addr1.sin_port == addr2.sin_port;
}

int main(int argc, char *argv[]) {
    int brokerPort = 9001;

    /**Leer el puerto desde la linea de comandos
     * argc es la cantidad de agumentos con los que se ejecutó el programa
     * argv es el arreglo de strings con dichos argumentos
     * argv[0] es el nombre del programa
     * argv[1] es el primer argumento que recibe el programa
    */
    if (argc == 2) {
        //se definió un puerto (no se va a usar el predeterminado 9001)
        brokerPort = atoi(argv[1]);
    } else if (argc > 2) {
        //se usaron demasiados argumentos, imprime el error
        printf("Uso: %s [broker_port]\n", argv[0]);
        return 1;
    }

    /**
     * llamado a la funcion socket() del sistema (<sys/socket.h>)
     * AF_INET declara que la red esta usando IPv4
     * SOCK_DGRAM es el tipo de socket, indica que es un socket UDP
     * 0 indica que se esta utilizando el protocolo por defecto que corresponde a AF_INET y SOCK_DGRAM
     */
    int brokerSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (brokerSocket < 0) {
        perror("Error al crear el socket del broker");
        return 1;
    }

    // crear la direccion local del broker para bind()
    struct sockaddr_in brokerAddr;
    //evitar basura en memoria poniendo toda la esctructura en 0
    memset(&brokerAddr, 0, sizeof(brokerAddr));
    //insicar que se usará IPv4
    brokerAddr.sin_family = AF_INET;
    //puerto. htons convierte el numero del puerto a bits
    brokerAddr.sin_port = htons(brokerPort);
    //el broker aceptará mensajes de cualquier interfaz de red de la máquina
    brokerAddr.sin_addr.s_addr = INADDR_ANY;

    /**
     * bind() asocia el brokerSocket con una direccion local y un puerto.
     * Asi cualquier datagrama que llegue al puerto en brokerPort sera entregado a este proceso.
     */
    if (bind(brokerSocket, (struct sockaddr *)&brokerAddr, sizeof(brokerAddr)) < 0) {
        perror("Error al hacer bind del broker");
        close(brokerSocket);
        return 1;
    }

    printf("Broker UDP listo para recibir mensajes en el puerto %d\n", brokerPort);

    // arreglo para guardar suscriptores y el tema al cual estan suscritos
    struct Subscriber subscribers[MAX_SUBSCRIBERS];
    int subscriber_count = 0;
    //arreglo para guardar que publisher fue el primero en publicar cada topic
    struct Publisher publishers[MAX_PUBLISHERS];
    int publisher_count = 0;

    //ciclo infinito para que el broker se quede corriendo indefinidamente
    while (1) {
        //buffer donde se guarda el datagrama recibido
        char buffer[MAX_BUFFER_SIZE];
        //direccion del cliente que envió el mensaje
        struct sockaddr_in clientAddr;
        //tamaño de la estructura de direccion
        socklen_t clientLen = sizeof(clientAddr);

        /**
         * recvfrom() espera un datagrama UDP.
         * Guarda el contenido en buffer y la direccion del emisor en clientAddr.
         */
        int n = recvfrom(brokerSocket, buffer, sizeof(buffer) - 1, 0,
                         (struct sockaddr *)&clientAddr, &clientLen);

        if (n < 0) {
            perror("Error al recibir mensaje");
            continue;
        }

        buffer[n] = '\0';

        printf("Mensaje recibido desde %s:%d -> %s\n",
               inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), buffer);

        //hacer una copia para poder usar strtok sin dañar el mensaje original
        char buffer_copy[MAX_BUFFER_SIZE];
        strncpy(buffer_copy, buffer, sizeof(buffer_copy) - 1);
        buffer_copy[sizeof(buffer_copy) - 1] = '\0';

        //separar el tipo del mensaje usando "|" como delimitador
        char *type = strtok(buffer_copy, "|");

        if (type == NULL) {
            continue;
        }

        if (strcmp(type, "SUBSCRIBE") == 0) {
            //extraer el topic al que se quiere suscribir el cliente
            char *topic = strtok(NULL, "|");

            if (topic == NULL) {
                printf("Suscripcion invalida\n");
                continue;
            }

            if (subscriber_exists(subscribers, subscriber_count, topic, clientAddr)) {
                printf("El suscriptor ya estaba registrado en el tema %s\n", topic);
                continue;
            }

            if (subscriber_count < MAX_SUBSCRIBERS) {
                //guardar el topic y la direccion del cliente en la lista
                strncpy(subscribers[subscriber_count].topic, topic, MAX_TOPIC_SIZE - 1);
                subscribers[subscriber_count].topic[MAX_TOPIC_SIZE - 1] = '\0';
                subscribers[subscriber_count].addr = clientAddr;
                subscriber_count++;

                printf("Nuevo suscriptor agregado al tema %s\n", topic);
            } else {
                printf("No hay espacio para mas suscriptores\n");
            }
        } else if (strcmp(type, "PUBLISH") == 0) {
            //extraer topic y contenido de la publicacion
            char *topic = strtok(NULL, "|");
            char *content = strtok(NULL, "");

            if (topic == NULL || content == NULL) {
                printf("Publicacion invalida\n");
                continue;
            }

            //cada topic solo puede ser manejado por un publisher
            int publisher_index = find_publisher_for_topic(publishers, publisher_count, topic);

            if (publisher_index == -1) {
                if (publisher_count < MAX_PUBLISHERS) {
                    strncpy(publishers[publisher_count].topic, topic, MAX_TOPIC_SIZE - 1);
                    publishers[publisher_count].topic[MAX_TOPIC_SIZE - 1] = '\0';
                    publishers[publisher_count].addr = clientAddr;
                    publisher_count++;

                    printf("Publisher %s:%d asignado al tema %s\n",
                           inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), topic);
                } else {
                    printf("No hay espacio para registrar mas publishers\n");
                    continue;
                }
            } else if (!same_udp_client(publishers[publisher_index].addr, clientAddr)) {
                printf("Publicacion rechazada: el tema %s ya tiene un publisher asignado\n",
                       topic);
                continue;
            }

            char outgoing[MAX_BUFFER_SIZE];
            snprintf(outgoing, sizeof(outgoing), "%s|%s", topic, content);

            //contador de a cuantos subscribers se logró reenviar
            int forwarded = 0;

            for (int i = 0; i < subscriber_count; i++) {
                if (strcmp(subscribers[i].topic, topic) == 0) {
                    //reenviar la publicacion a cada subscriber del mismo topic
                    int sent = sendto(brokerSocket, outgoing, strlen(outgoing), 0,
                                      (struct sockaddr *)&subscribers[i].addr,
                                      sizeof(subscribers[i].addr));

                    if (sent < 0) {
                        perror("Error al reenviar publicacion");
                    } else {
                        forwarded++;
                    }
                }
            }

            printf("Publicacion del tema %s reenviada a %d suscriptores\n",
                   topic, forwarded);
        } else {
            printf("Tipo de mensaje no reconocido\n");
        }
    }

    close(brokerSocket);
    return 0;
}
