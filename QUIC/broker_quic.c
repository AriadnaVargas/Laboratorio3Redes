#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

#define BROKER_PORT        9003
#define MAX_MESSAGE_SIZE   512
#define MAX_BUFFER_SIZE    1024
#define MAX_SUBSCRIBERS    100
#define MAX_TOPICS         20
#define TOPIC_LEN          128

#define ACK_TIMEOUT_MS     600
#define MAX_RETRIES        4


typedef struct {
    /* Direccion IP y puerto del suscriptor. Como UDP no tiene conexiones,
     * identificamos a cada suscriptor por esta pareja ip:puerto */
    struct sockaddr_in ipPuertoSuscriptor;
    /* nombre del suscriptor para logs y seguimiento */
    char name[100];
    /* tema al que este suscriptor esta suscrito */
    char topic[TOPIC_LEN];
    /* siguiente numero de secuencia a enviarle */
    int  next_seq;
    /* 1 = activo, 0 = inactivo (sin ACKs o desconectado) */
    int  active;
} informacionRelevanteSuscriptores;


static informacionRelevanteSuscriptores suscriptoresEnTotal[MAX_SUBSCRIBERS];
static int numeroSuscriptores = 0;


static int comparacionIpPuerto(const struct sockaddr_in *primera, const struct sockaddr_in *segunda) {
    struct sockaddr_in ipPuertoA = *primera;
    struct sockaddr_in ipPuertoB = *segunda;

    int valorRetorno = 1;

    if(ipPuertoA.sin_addr.s_addr != ipPuertoB.sin_addr.s_addr) {
        valorRetorno = 0;
    }
    if(ipPuertoA.sin_port != ipPuertoB.sin_port) {
        valorRetorno = 0;
    }

    return valorRetorno;
}


static int registrarSuscriptorATema(const struct sockaddr_in *punteroDireccionPuertoSuscriptor,
                                    const char *nombreSuscriptor,
                                    const char *temaAInscribirse) {

    int i;
    for (i = 0; i < numeroSuscriptores; i++) {
        if (comparacionIpPuerto(&suscriptoresEnTotal[i].ipPuertoSuscriptor,
                                 punteroDireccionPuertoSuscriptor)) {
            int estaActivo   = suscriptoresEnTotal[i].active;
            int estaInscrito = strcmp(suscriptoresEnTotal[i].topic, temaAInscribirse) == 0;
            if (estaActivo && estaInscrito) {
                return 1;
            }
        }
    }

    if (numeroSuscriptores >= MAX_SUBSCRIBERS) {
        fprintf(stderr, "[BROKER QUIC] Lista de suscriptores llena\n");
        return 0;
    }

    int indiceNuevoSuscriptorAInscribir = numeroSuscriptores;

    //literalmente aca lo suscribo al tema y le pongo su informacion
    suscriptoresEnTotal[indiceNuevoSuscriptorAInscribir].ipPuertoSuscriptor = *punteroDireccionPuertoSuscriptor;
    suscriptoresEnTotal[indiceNuevoSuscriptorAInscribir].next_seq = 1;
    suscriptoresEnTotal[indiceNuevoSuscriptorAInscribir].active   = 1;

    strncpy(suscriptoresEnTotal[indiceNuevoSuscriptorAInscribir].name,
            nombreSuscriptor,
            sizeof(suscriptoresEnTotal[0].name) - 1);

    strncpy(suscriptoresEnTotal[indiceNuevoSuscriptorAInscribir].topic,
            temaAInscribirse,
            sizeof(suscriptoresEnTotal[0].topic) - 1);

    struct sockaddr_in copiaPrinteo = *punteroDireccionPuertoSuscriptor;
    printf("[BROKER QUIC] Suscriptor '%s' (%s:%d) registrado en tema '%s'\n",
           nombreSuscriptor,
           inet_ntoa(copiaPrinteo.sin_addr),
           ntohs(copiaPrinteo.sin_port),
           temaAInscribirse);

    numeroSuscriptores++;
    return 1;
}

//ACK del broker al publicador que confirma que el broker recibio el mensaje
static void enviarACKPublicador(int numeroSocketCorrespondienteBroker,
                                const struct sockaddr_in *direccionpuertoPublicadorInteres,
                                int seq) {
    //en la variable ack se va a guardar el mensaje para enviarle el ack al publicador                                
    char ack[64];
    snprintf(ack, sizeof(ack), "ACK|%d", seq);
    

    const struct sockaddr *dir = (const struct sockaddr *) direccionpuertoPublicadorInteres;

    int envioExitoso = sendto(numeroSocketCorrespondienteBroker,
                              ack, strlen(ack), 0,
                              dir, sizeof(struct sockaddr_in));

    if (envioExitoso < 0) {
        fprintf(stderr, "[BROKER QUIC] Error enviando ACK al publisher: %s\n",
                strerror(errno));
    } else {
        struct sockaddr_in copiaPrinteoDos = *direccionpuertoPublicadorInteres;
        printf("[BROKER QUIC] ACK|%d enviado al publisher %s:%d\n",
               seq,
               inet_ntoa(copiaPrinteoDos.sin_addr),
               ntohs(copiaPrinteoDos.sin_port));
    }
}


static int enviarMensajeASuscriptor(int numeroCorrespondienteBroker,
                                    const char *mensajeAEnviar,
                                    int longitudMensaje,
                                    informacionRelevanteSuscriptores *dest,
                                    int seq) {
    //en esta variable literal guardo el ack de respuesta
    char bufferGuardarACKRespuesta[MAX_BUFFER_SIZE];
    //aca esta el ack que se va a enviar
    char ACKqueEsperoRespuesta[64];
    snprintf(ACKqueEsperoRespuesta, sizeof(ACKqueEsperoRespuesta), "ACK|%d", seq);

    int tiempoEspera = ACK_TIMEOUT_MS;

    int attempt;
    informacionRelevanteSuscriptores copiaPrinteoTres = *dest;
    const struct sockaddr *direccionGenerica = (const struct sockaddr *) &copiaPrinteoTres.ipPuertoSuscriptor;

    for (attempt = 0; attempt < MAX_RETRIES; attempt++) {

        
        int exitoDelMensaje = sendto(numeroCorrespondienteBroker,
                                     mensajeAEnviar,
                                     longitudMensaje,
                                     0,
                                     direccionGenerica,
                                     sizeof(struct sockaddr_in));

        if (exitoDelMensaje < 0) {
            fprintf(stderr, "[BROKER QUIC] Error sendto suscriptor '%s': %s\n",
                    copiaPrinteoTres.name, strerror(errno));
            return -1;
        }

        if (attempt == 0) {
            printf("[BROKER QUIC] MSG seq=%d enviado a '%s' (%s:%d)\n",
                   seq, copiaPrinteoTres.name,
                   inet_ntoa(copiaPrinteoTres.ipPuertoSuscriptor.sin_addr),
                   ntohs(copiaPrinteoTres.ipPuertoSuscriptor.sin_port));
        } else {
            printf("[BROKER QUIC] Retransmision #%d (timeout=%dms) seq=%d -> '%s'\n",
                   attempt, tiempoEspera, seq, copiaPrinteoTres.name);
        }
        //esta variable (bueno en realidad conjunto xd) sirve para estar pilas de cuando se
        //llega a tocar el socket, osea voy a leer ese socket
        fd_set socketsDeLectura;
        FD_ZERO(&socketsDeLectura);
        FD_SET(numeroCorrespondienteBroker, &socketsDeLectura);

        long segundos     = tiempoEspera / 1000;
        long microsegundos = (tiempoEspera % 1000) * 1000;
        struct timeval tv;
        tv.tv_sec  = segundos;
        tv.tv_usec = microsegundos;

        int ready = select(numeroCorrespondienteBroker + 1, &socketsDeLectura, NULL, NULL, &tv);

        if (ready > 0) {
            struct sockaddr_in direccionPuertoSuscriptorRespuesta;
            socklen_t longitudSocket = sizeof(direccionPuertoSuscriptorRespuesta);

            struct sockaddr *direccionGenericaRespuesta = (struct sockaddr *) &direccionPuertoSuscriptorRespuesta;

            int bytesRecibidos = recvfrom(numeroCorrespondienteBroker,
                                          bufferGuardarACKRespuesta,
                                          sizeof(bufferGuardarACKRespuesta) - 1,
                                          0,
                                          direccionGenericaRespuesta,
                                          &longitudSocket);

            if (bytesRecibidos > 0) {
                bufferGuardarACKRespuesta[bytesRecibidos] = '\0';

                if (comparacionIpPuerto(&direccionPuertoSuscriptorRespuesta,
                                         &copiaPrinteoTres.ipPuertoSuscriptor) &&
                    strcmp(bufferGuardarACKRespuesta, ACKqueEsperoRespuesta) == 0) {

                    printf("[BROKER QUIC] ACK|%d confirmado por '%s'\n",
                           seq, copiaPrinteoTres.name);
                    return 0;
                }
            }
        }

        tiempoEspera = tiempoEspera;
    }

    fprintf(stderr, "[BROKER QUIC] Sin ACK tras %d intentos para '%s' seq=%d. "
                    "Marcando como inactivo.\n",
            MAX_RETRIES, copiaPrinteoTres.name, seq);
    copiaPrinteoTres.active = 0;
    return -1;
}


static void enviarMensajeSuscriptoresActivos(int numeroCorrespondienteSocketBroker,
                                             const char *tema,
                                             const char *mensaje) {
    int suscriptoresEncontrados = 0;

    printf("[BROKER QUIC] Distribuyendo mensaje del tema '%s'...\n", tema);

    int i;
    for (i = 0; i < numeroSuscriptores; i++) {

        int estaActivo   = suscriptoresEnTotal[i].active != 0;
        int coincideTema = strcmp(suscriptoresEnTotal[i].topic, tema) == 0;

        if (estaActivo && coincideTema) {
            suscriptoresEncontrados = 1;

            int seq = suscriptoresEnTotal[i].next_seq;
            suscriptoresEnTotal[i].next_seq++;

            char buffer[MAX_BUFFER_SIZE];
            int len = snprintf(buffer, sizeof(buffer), "MSG|%d|[%s] %s", seq, tema, mensaje);

            enviarMensajeASuscriptor(numeroCorrespondienteSocketBroker,
                                      buffer, len,
                                      &suscriptoresEnTotal[i],
                                      seq);
        }
    }

    if (suscriptoresEncontrados == 0) {
        printf("[BROKER QUIC] Tema '%s' no tiene suscriptores activos\n", tema);
    }
}


int main(int argc, char *argv[]) {
    int puerto = BROKER_PORT;

    if (argc == 2) {
        puerto = atoi(argv[1]);
    } else if (argc > 2) {
        fprintf(stderr, "Uso: %s [puerto]\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 9003\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("===== BROKER QUIC HIBRIDO (UDP + Confiabilidad de aplicacion) =====\n");
    printf("Puerto: %d\n", puerto);
    printf("ACK timeout: %d ms | Reintentos: %d \n\n",
           ACK_TIMEOUT_MS, MAX_RETRIES);

    //notemos que justamente el socket utiliza el sock_dgram que usamos en la implementación de UDP
    int socketUDP = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketUDP < 0) {
        fprintf(stderr, "[ERROR] No se pudo crear el socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("[OK] Socket UDP creado (descriptor: %d)\n", socketUDP);

    int valorActivar = 1;
    int configuracionSocket = setsockopt(socketUDP, SOL_SOCKET, SO_REUSEADDR,
                                          &valorActivar, sizeof(valorActivar));
    if (configuracionSocket < 0) {
        fprintf(stderr, "[WARN] setsockopt SO_REUSEADDR fallo: %s\n", strerror(errno));
    }

    //aca creamos la direccion del servidor que el broker va a escuchar
    //osea el broker va a escuchar en varias direcciones IP/interfaces de red
    //y eso se lo decimos en INADDR_ANY
    struct sockaddr_in direccionDelServidor;
    memset(&direccionDelServidor, 0, sizeof(direccionDelServidor));
    direccionDelServidor.sin_family = AF_INET;
    direccionDelServidor.sin_addr.s_addr = INADDR_ANY;
    direccionDelServidor.sin_port = htons(puerto);

    struct sockaddr *direccionGenerica = (struct sockaddr *) &direccionDelServidor;

    int vinculacionSocket = bind(socketUDP, direccionGenerica, sizeof(direccionDelServidor));
    if (vinculacionSocket < 0) {
        fprintf(stderr, "[ERROR] No se pudo vincular al puerto %d: %s\n",
                puerto, strerror(errno));
        close(socketUDP);
        exit(EXIT_FAILURE);
    }
    printf("[OK] Socket vinculado a 0.0.0.0:%d\n", puerto);
    printf("[OK] Broker en escucha...\n");
    printf("Esperando mensajes de publicadores y suscriptores...\n");
    printf("(Presiona Ctrl+C para detener el broker)\n\n");

    char buffer[MAX_BUFFER_SIZE];

    while (1) {
        struct sockaddr_in informacionDelQueEnvio;
        socklen_t longitudInformacionEnvio = sizeof(informacionDelQueEnvio);
        memset(buffer, 0, sizeof(buffer));

        //notemos que ese mensaje que recibimos pudo haber sido recibido tanto por
        //publicador o por suscriptor
        //por eso luego toca verificar el tipo de mensaje
        int bytesRecibidos = recvfrom(socketUDP,
                                      buffer, sizeof(buffer) - 1,
                                      0,
                                      (struct sockaddr *) &informacionDelQueEnvio,
                                      &longitudInformacionEnvio);

        int correctamenteProcesado = 0;

        if (bytesRecibidos >= 0) {
            buffer[bytesRecibidos] = '\0';

            char *saltoDeLinea = strchr(buffer, '\n');
            if (saltoDeLinea != NULL) {
                *saltoDeLinea = '\0';
            }
            correctamenteProcesado = 1;
        } else {
            fprintf(stderr, "[ERROR] recvfrom: %s\n", strerror(errno));
        }

        if (correctamenteProcesado != 0) {
            printf("[BROKER QUIC] Datagrama de %s:%d -> '%s'\n",
                   inet_ntoa(informacionDelQueEnvio.sin_addr),
                   ntohs(informacionDelQueEnvio.sin_port),
                   buffer);

            
            char tipoMensaje[20];
            memset(tipoMensaje, 0, sizeof(tipoMensaje));

            char *separador = strchr(buffer, '|');
            if (separador != NULL) {
                int longitud = separador - buffer;
                strncpy(tipoMensaje, buffer, longitud);
                tipoMensaje[longitud] = '\0';
            }

            if (strcmp(tipoMensaje, "PUBLISH") == 0) {

                char *puntero = separador + 1;
                char *sepSecuencia = strchr(puntero, '|');

                if (sepSecuencia != NULL) {
                    *sepSecuencia = '\0';
                    int secuencia = atoi(puntero);

                    char *inicioTema = sepSecuencia + 1;
                    char *sepTema    = strchr(inicioTema, '|');

                    if (sepTema != NULL) {
                        *sepTema      = '\0';
                        char *tema    = inicioTema;
                        char *mensaje = sepTema + 1;

                        printf("[BROKER QUIC] PUBLISH seq=%d tema='%s' msg='%s'\n",
                               secuencia, tema, mensaje);

                        enviarACKPublicador(socketUDP,
                                             &informacionDelQueEnvio,
                                             secuencia);

                        enviarMensajeSuscriptoresActivos(socketUDP, tema, mensaje);
                    }
                }
            } else if (strcmp(tipoMensaje, "SUBSCRIBE") == 0) {

                char *tema = separador + 1;

                char nombreSuscriptor[100];
                strncpy(nombreSuscriptor, "desconocido", sizeof(nombreSuscriptor) - 1);

                int i;
                //aca buscamos el nombre del suscriptor que se haya inscrito a ese tema
                for (i = 0; i < numeroSuscriptores; i++) {
                    if (comparacionIpPuerto(&suscriptoresEnTotal[i].ipPuertoSuscriptor,
                                             &informacionDelQueEnvio)) {
                        strncpy(nombreSuscriptor,
                                suscriptoresEnTotal[i].name,
                                sizeof(nombreSuscriptor) - 1);
                        break;
                    }
                }

                registrarSuscriptorATema(&informacionDelQueEnvio, nombreSuscriptor, tema);

                char mensajeConfirmacion[MAX_MESSAGE_SIZE];
                snprintf(mensajeConfirmacion, sizeof(mensajeConfirmacion),
                         "SUBSCRIBED|%s", tema);
                sendto(socketUDP,
                       mensajeConfirmacion, strlen(mensajeConfirmacion),
                       0,
                       (const struct sockaddr *) &informacionDelQueEnvio,
                       sizeof(informacionDelQueEnvio));

            } else if (strcmp(tipoMensaje, "REGISTER") == 0) {

                char *puntero   = separador + 1;
                char *sepNombre = strchr(puntero, '|');

                if (sepNombre != NULL) {
                    *sepNombre   = '\0';
                    char *nombre = puntero;

                    printf("[BROKER QUIC] REGISTER de suscriptor '%s' desde %s:%d\n",
                           nombre,
                           inet_ntoa(informacionDelQueEnvio.sin_addr),
                           ntohs(informacionDelQueEnvio.sin_port));

                    if (numeroSuscriptores < MAX_SUBSCRIBERS) {
                        int encontrado = 0;
                        int i;
                        for (i = 0; i < numeroSuscriptores; i++) {
                            if (comparacionIpPuerto(&suscriptoresEnTotal[i].ipPuertoSuscriptor,
                                                     &informacionDelQueEnvio)) {
                                encontrado = 1;
                                break;
                            }
                        }
                        if (encontrado == 0) {
                            suscriptoresEnTotal[numeroSuscriptores].ipPuertoSuscriptor =
                                informacionDelQueEnvio;
                            suscriptoresEnTotal[numeroSuscriptores].next_seq = 1;
                            suscriptoresEnTotal[numeroSuscriptores].active = 1;
                            strncpy(suscriptoresEnTotal[numeroSuscriptores].name,
                                    nombre,
                                    sizeof(suscriptoresEnTotal[0].name) - 1);
                            strcpy(suscriptoresEnTotal[numeroSuscriptores].topic, "");
                            numeroSuscriptores++;
                        }
                    }

                    char mensajeRegistro[64];
                    snprintf(mensajeRegistro, sizeof(mensajeRegistro),
                             "REGISTERED|%s", nombre);
                    
                    const struct sockaddr *direccionGenerica = (const struct sockaddr *) &informacionDelQueEnvio;

                    sendto(socketUDP, mensajeRegistro, strlen(mensajeRegistro), 0,
                        direccionGenerica,
                        sizeof(informacionDelQueEnvio));
                }

            } else if (strcmp(tipoMensaje, "ACK") == 0) {

                printf("[BROKER QUIC] ACK tardio recibido de %s:%d: '%s' (ignorado)\n",
                       inet_ntoa(informacionDelQueEnvio.sin_addr),
                       ntohs(informacionDelQueEnvio.sin_port),
                       buffer);

            } else {
                printf("[BROKER QUIC] Mensaje desconocido de %s:%d: '%s'\n",
                       inet_ntoa(informacionDelQueEnvio.sin_addr),
                       ntohs(informacionDelQueEnvio.sin_port),
                       buffer);
            }
        }
    }

    close(socketUDP);
    return 0;
}