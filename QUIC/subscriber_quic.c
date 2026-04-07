#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

#define MAX_MESSAGE_SIZE    512
#define MAX_BUFFER_SIZE     1024
#define MAX_PARTIDOS        20

#define REORDER_WINDOW      8

#define RECV_TIMEOUT_MS     100

volatile int keep_running = 1;


static void signal_handler(int sig) {
    (void)sig;
    printf("\n[SUBSCRIBER QUIC] Recibido Ctrl+C, desconectando...\n");
    keep_running = 0;
}


static void enviarACK(int numeroSocketCorrespondiente,
                       const struct sockaddr_in *direccionPuertoBroker,
                       int seq,
                       const char *identificadorSuscriptor) {
    //de nuevo, esta parte es la misma que en broker para enviar el ack
    //ahora que lo pienso, se podia haber creado como una clase adicional
    //y definir esta funcion e importarla segun fuese suscriptor, broker o publicador
    //porque las 3 igual la tienen que usar
    char ack[64];
    snprintf(ack, sizeof(ack), "ACK|%d", seq);

    const struct sockaddr *dir = (const struct sockaddr *) direccionPuertoBroker;

    int envioExitoso = sendto(numeroSocketCorrespondiente,
                               ack, strlen(ack),
                               0,
                               dir, sizeof(struct sockaddr_in));

    if (envioExitoso < 0) {
        fprintf(stderr, "[%s] Error enviando ACK|%d: %s\n",
                identificadorSuscriptor, seq, strerror(errno));
    } else {
        printf("[%s] ACK|%d enviado al broker\n", identificadorSuscriptor, seq);
    }
}


static void enviarSuscripcionABroker(int numeroSocket,
                                      const struct sockaddr_in *direccionPuertoBroker,
                                      const char *topic,
                                      const char *identificadorSuscriptor) {

    char mensaje[MAX_MESSAGE_SIZE];
    int  longitudMensaje = snprintf(mensaje, sizeof(mensaje),
                                    "SUBSCRIBE|%s", topic);

    char respuesta[MAX_BUFFER_SIZE];

    int attempt;
    for (attempt = 0; attempt < 3; attempt++) {

        const struct sockaddr *direccionDestino = (const struct sockaddr *) direccionPuertoBroker;

        sendto(numeroSocket,
               mensaje, longitudMensaje,
               0,
               direccionDestino, sizeof(struct sockaddr_in));

        //de nuevo usamos este conjunto de sockets para poder estar pilas del momento
        //en que se recibe el mesnaje por medio de ese socket
        fd_set socketsLectura;
        FD_ZERO(&socketsLectura);
        FD_SET(numeroSocket, &socketsLectura);
        struct timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        int ready = select(numeroSocket + 1, &socketsLectura, NULL, NULL, &tv);

        if (ready > 0) {
            struct sockaddr_in direccionRespuesta;
            socklen_t slen = sizeof(direccionRespuesta);

            struct sockaddr *direccionGenerica = (struct sockaddr *) &direccionRespuesta;

            int bytesRecibidos = recvfrom(numeroSocket,
                                           respuesta, sizeof(respuesta) - 1,
                                           0,
                                           direccionGenerica, &slen);

            if (bytesRecibidos > 0) {
                respuesta[bytesRecibidos] = '\0';

                if (strncmp(respuesta, "SUBSCRIBED|", 11) == 0) {
                    printf("[%s] Suscrito al tema: %s\n",
                           identificadorSuscriptor, topic);
                    return;
                }
            }
        }

        printf("[%s] Reintentando suscripcion a '%s' (#%d)...\n",
               identificadorSuscriptor, topic, attempt + 1);
    }

    fprintf(stderr, "[%s] No se pudo confirmar suscripcion a '%s'\n",
            identificadorSuscriptor, topic);
}


int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr,
                "Uso: %s <broker_ip> <broker_port> <subscriber_id> <num_partidos>\n",
                argv[0]);
        fprintf(stderr, "Ejemplo: %s 127.0.0.1 9003 sub1 2\n", argv[0]);
        fprintf(stderr, "\nEsto se suscribira a 2 partidos:\n");
        fprintf(stderr, "  - match_1_vs_2\n");
        fprintf(stderr, "  - match_3_vs_4\n");
        exit(EXIT_FAILURE);
    }

    const char *broker_ip = argv[1];
    int broker_port       = atoi(argv[2]);
    const char *sub_id    = argv[3];
    int num_partidos      = atoi(argv[4]);

    if (num_partidos <= 0 || num_partidos > MAX_PARTIDOS) {
        fprintf(stderr, "[ERROR] Numero de partidos debe estar entre 1 y %d\n",
                MAX_PARTIDOS);
        exit(EXIT_FAILURE);
    }

    /* Construir los nombres de los temas a suscribirse */
    char topics[MAX_PARTIDOS][100];
    int i;
    for (i = 0; i < num_partidos; i++) {
        int numeroPartido   = i + 1;
        int equipoLocal     = 2 * numeroPartido - 1;
        int equipoVisitante = 2 * numeroPartido;
        snprintf(topics[i], sizeof(topics[i]), "match_%d_vs_%d", equipoLocal, equipoVisitante);
    }

    printf("===== SUBSCRIBER QUIC HIBRIDO =====\n");
    printf("Suscriptor ID:   %s\n", sub_id);
    printf("Numero partidos: %d\n", num_partidos);
    printf("Partidos a suscribir:\n");
    for (i = 0; i < num_partidos; i++) {
        printf("  - %s\n", topics[i]);
    }
    printf("Broker:          %s:%d\n", broker_ip, broker_port);
    printf("Confiabilidad:   ACKs por mensaje + deteccion de orden\n\n");

    signal(SIGINT, signal_handler);

    //este suscriptor envia ACK por cada mensaje recibido adicionalmente
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "[ERROR] No se pudo crear el socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("[OK] Socket UDP creado\n");

    // Vincular el socket a un puerto local asignado automaticamente 
    struct sockaddr_in ipPuertoLocalPropio;
    memset(&ipPuertoLocalPropio, 0, sizeof(ipPuertoLocalPropio));
    ipPuertoLocalPropio.sin_family      = AF_INET;
    ipPuertoLocalPropio.sin_addr.s_addr = INADDR_ANY;
    ipPuertoLocalPropio.sin_port        = htons(0);  // puerto dinamico 

    struct sockaddr *direccionGenerica = (struct sockaddr *) &ipPuertoLocalPropio;
    int vinculacion = bind(sockfd, direccionGenerica, sizeof(ipPuertoLocalPropio));
    if (vinculacion < 0) {
        fprintf(stderr, "[ERROR] No se pudo vincular el socket: %s\n", strerror(errno));
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    socklen_t local_len = sizeof(ipPuertoLocalPropio);
    struct sockaddr *direccionGenericaDos = (struct sockaddr *) &ipPuertoLocalPropio;
    getsockname(sockfd, direccionGenericaDos, &local_len);
    printf("[OK] Socket vinculado al puerto local %d\n",
           ntohs(ipPuertoLocalPropio.sin_port));

    //configuracion ipPuerto del broker
    struct sockaddr_in ipPuertoBroker;
    memset(&ipPuertoBroker, 0, sizeof(ipPuertoBroker));
    ipPuertoBroker.sin_family = AF_INET;
    ipPuertoBroker.sin_port   = htons(broker_port);

    if (inet_pton(AF_INET, broker_ip, &ipPuertoBroker.sin_addr) <= 0) {
        fprintf(stderr, "[ERROR] Direccion IP invalida: %s\n", broker_ip);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /* Enviar mensaje de registro al broker */
    printf("\nEnviando registro al broker...\n");
    char mensajeTipoRegistro[MAX_MESSAGE_SIZE];
    int  reg_len = snprintf(mensajeTipoRegistro, sizeof(mensajeTipoRegistro),
                            "REGISTER|%s|%d", sub_id, num_partidos);

    struct sockaddr *direccionGenericaTres = (struct sockaddr *) &ipPuertoBroker;
    int envioExitoso = sendto(sockfd,
                               mensajeTipoRegistro, reg_len,
                               0,
                               direccionGenericaTres, sizeof(struct sockaddr_in));
    if (envioExitoso < 0) {
        fprintf(stderr, "[ERROR] Error enviando REGISTER: %s\n", strerror(errno));
    }

    //bufer para guardar la respuesta del ACK
    char respuestaACK[MAX_BUFFER_SIZE];
    fd_set socketLecturaDos;
    FD_ZERO(&socketLecturaDos);
    FD_SET(sockfd, &socketLecturaDos);
    struct timeval tv_respuestaRegistro;
    tv_respuestaRegistro.tv_sec  = 2;
    tv_respuestaRegistro.tv_usec = 0;

    int ready = select(sockfd + 1, &socketLecturaDos, NULL, NULL,
                        &tv_respuestaRegistro);
    if (ready > 0) {
        struct sockaddr_in enviadorBroker;
        socklen_t longitud = sizeof(enviadorBroker);

        struct sockaddr *direccionGenericaCuatro = (struct sockaddr *) &enviadorBroker;
        int bytesRecibidos = recvfrom(sockfd,
                                       respuestaACK, sizeof(respuestaACK) - 1,
                                       0,
                                       direccionGenericaCuatro, &longitud);
        if (bytesRecibidos > 0) {
            respuestaACK[bytesRecibidos] = '\0';
            if (strncmp(respuestaACK, "REGISTERED|", 11) == 0) {
                printf("[OK] Registro confirmado por el broker: %s\n\n", respuestaACK);
            }
        }
    } else {
        printf("[WARN] El broker no confirmo el registro, continuando...\n\n");
    }

    printf("Suscribiendo a partidos...\n\n");
    for (i = 0; i < num_partidos; i++) {
        enviarSuscripcionABroker(sockfd, &ipPuertoBroker, topics[i], sub_id);
    }

    printf("\n========================================\n");
    printf("Esperando mensajes (presiona Ctrl+C para salir)...\n");
    printf("========================================\n\n");

    typedef struct {
        int  secuenciaEsperada;
        char bufferMensajes[REORDER_WINDOW][MAX_BUFFER_SIZE];
        int  secuencias[REORDER_WINDOW];
        int  ocupado[REORDER_WINDOW];
    } estadoPartido;

    estadoPartido partidos[MAX_PARTIDOS];


    //int i;
    int j;
    for (i = 0; i < num_partidos; i++) {
        partidos[i].secuenciaEsperada = 1;
        for (j = 0; j < REORDER_WINDOW; j++) {
            partidos[i].ocupado[j]    = 0;
            partidos[i].secuencias[j] = 0;
        }
    }

    char buffer[MAX_BUFFER_SIZE];

    while (keep_running) {

        fd_set socketsLecturaCinco;
        FD_ZERO(&socketsLecturaCinco);
        FD_SET(sockfd, &socketsLecturaCinco);

        struct timeval intervalo;
        intervalo.tv_sec  = RECV_TIMEOUT_MS / 1000;
        intervalo.tv_usec = (RECV_TIMEOUT_MS % 1000) * 1000;

        int listos = select(sockfd + 1, &socketsLecturaCinco, NULL, NULL, &intervalo);

        if (listos < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "[ERROR] select: %s\n", strerror(errno));
            break;
        }
        //mientras no reciba nada sigamos con el while
        if (listos == 0) {
            continue;
        }

        memset(buffer, 0, sizeof(buffer));

        struct sockaddr_in brokerEnviadorMensaje;
        socklen_t longitudAsociada = sizeof(brokerEnviadorMensaje);

        struct sockaddr *direccionGenericaSeis = (struct sockaddr *) &brokerEnviadorMensaje;

        int bytesRecibidos = recvfrom(sockfd,
                                       buffer, sizeof(buffer) - 1,
                                       0,
                                       direccionGenericaSeis, &longitudAsociada);

        if (bytesRecibidos < 0) {
            fprintf(stderr, "[ERROR] recvfrom: %s\n", strerror(errno));
            continue;
        }
        if (bytesRecibidos == 0) {
            continue;
        }

        buffer[bytesRecibidos] = '\0';   
        if (strncmp(buffer, "MSG|", 4) == 0) {

            char *inicio    = buffer + 4;
            char *separador = strchr(inicio, '|');

            if (separador != NULL) {
                *separador = '\0';

                int  numeroSecuencia = atoi(inicio);
                char *contenidoMensaje = separador + 1;

                enviarACK(sockfd, &brokerEnviadorMensaje,
                           numeroSecuencia, sub_id);

                int indiceTopic = -1;
                for (i = 0; i < num_partidos; i++) {
                    char prefijo[120];
                    snprintf(prefijo, sizeof(prefijo), "[%s]", topics[i]);
                    if (strstr(contenidoMensaje, prefijo) != NULL) {
                        indiceTopic = i;
                        break;
                    }
                }

                if (indiceTopic >= 0) {

                    if (numeroSecuencia == partidos[indiceTopic].secuenciaEsperada) {

                        printf("[%s] (seq=%d) %s\n",
                               sub_id, numeroSecuencia, contenidoMensaje);
                        partidos[indiceTopic].secuenciaEsperada++;


                        int seAvanzo = 1;
                        while (seAvanzo) {
                            seAvanzo = 0;
                            for (j = 0; j < REORDER_WINDOW; j++) {
                                int estaOcupado = partidos[indiceTopic].ocupado[j];
                                int esElSiguiente = partidos[indiceTopic].secuencias[j] == partidos[indiceTopic].secuenciaEsperada;

                                if (estaOcupado && esElSiguiente) {
                                    printf("[%s] (seq=%d, reordenado) %s\n",
                                        sub_id,
                                        partidos[indiceTopic].secuencias[j],
                                        partidos[indiceTopic].bufferMensajes[j]);

                                    partidos[indiceTopic].secuenciaEsperada++;
                                    partidos[indiceTopic].ocupado[j] = 0;
                                    seAvanzo = 1;
                                }
                            }
                        }
                        } else if (numeroSecuencia > partidos[indiceTopic].secuenciaEsperada) {

                            printf("[%s] (seq=%d fuera de orden, esperado=%d) guardando...\n",
                                sub_id, numeroSecuencia, partidos[indiceTopic].secuenciaEsperada);

                            for (j = 0; j < REORDER_WINDOW; j++) {
                                if (partidos[indiceTopic].ocupado[j] == 0) {
                                    strncpy(partidos[indiceTopic].bufferMensajes[j],
                                            contenidoMensaje,
                                            MAX_BUFFER_SIZE - 1);
                                    partidos[indiceTopic].secuencias[j] = numeroSecuencia;
                                    partidos[indiceTopic].ocupado[j]    = 1;
                                    break;
                                }
                            }

                    } else {
                        printf("[%s] (seq=%d duplicado, ignorado)\n",
                               sub_id, numeroSecuencia);
                    }

                } else {
                    printf("[%s] %s\n", sub_id, contenidoMensaje);
                }

            } else {
                printf("[%s] Mensaje mal formado: %s\n", sub_id, buffer);
            }

        } else {
            
            printf("[%s] Mensaje del broker: %s\n", sub_id, buffer);
        }
    }

    printf("\n[OK] Desconectando del broker...\n");
    close(sockfd);
    printf("[OK] Conexion cerrada\n");

    return 0;
}