#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

#define MAX_MESSAGE_SIZE   512
#define MAX_EVENT_MESSAGE  256
#define MAX_BUFFER_SIZE    1024
#define MAX_PARTIDOS       20
#define NUM_TEMPLATES      4
#define MSGS_POR_TEMPLATE  10

#define ACK_TIMEOUT_MS     600
#define MAX_RETRIES        4

//le psue un ligero cambio a la implementacion de tcp
//entonces es un arreglo que tiene 4 templates distintos, y cada template tiene 10 mensajes 
//entonces para cada partido de forma random va a escoger una de esas plantillas y listo
const char *mensajesPorTemplate[NUM_TEMPLATES][MSGS_POR_TEMPLATE] = {
    {
        "Partido iniciado entre equipo %d y su rival",
        "Corner a favor de equipo %d",
        "!GOL! Equipo %d abre el marcador al minuto 12",
        "Tarjeta amarilla para numero 7 de equipo %d",
        "Cambio en equipo %d: entra numero 11 por numero 9",
        "HALFTIME: Primera mitad finaliza",
        "Segundo tiempo inicia con intensidad",
        "!GOL! Espectacular gol de equipo %d al minuto 67",
        "Tension maxima en los ultimos minutos",
        "FINAL: !El partido ha terminado!"
    },

    {
        "Partido iniciado entre equipo %d y su rival",
        "Tiro de equipo %d desviado por poco",
        "Tarjeta roja directa para numero 15 de equipo %d",
        "!GOL! Gol de penalti convertido por equipo %d",
        "Cambio defensivo en equipo %d para proteger resultado",
        "HALFTIME: Primera mitad finaliza",
        "Segundo tiempo inicia con intensidad",
        "Portero de equipo %d hace parada espectacular",
        "Equipo %d se defiende contra la avalancha rival",
        "FINAL: !El partido ha terminado!"
    },
    {
        "Partido iniciado entre equipo %d y su rival",
        "Balon recuperado por defensa de equipo %d",
        "!Gran jugada! Equipo %d se acerca al arco rival",
        "Remate de cabeza de equipo %d fuera del arco",
        "Triple cambio simultaneo en equipo %d",
        "HALFTIME: Primera mitad finaliza",
        "Segundo tiempo inicia con intensidad",
        "!GOL! Equipo %d empata al minuto 78",
        "Revisor VAR analiza jugada controversial",
        "FINAL: !El partido ha terminado!"
    },
    {
        "Partido iniciado entre equipo %d y su rival",
        "Tiro libre directo para equipo %d",
        "Posesion de balon: 60%% equipo %d, 40%% rival",
        "Amonestacion a numero 10 de equipo %d por protesta",
        "!GOL! Equipo %d marca de tiro libre al minuto 34",
        "HALFTIME: Primera mitad finaliza",
        "Segundo tiempo inicia con intensidad",
        "Segunda amarilla: expulsado jugador de equipo %d",
        "Gol confirmado tras revision del VAR",
        "FINAL: !El partido ha terminado!"
    }
};


static void generarMensaje(char *apuntadorBufferSalida, size_t tamanoMaximoBuffer,
                            const char *estructuraMensajeAGuardar, int equipo) {
    //notemos que el mensaje lo voy a escribir en apuntadorBufferSalida 
    //y lo que voy a escribir es estructuraMensajeAGuardar
    if (strstr(estructuraMensajeAGuardar, "%d") != NULL) {
        snprintf(apuntadorBufferSalida, tamanoMaximoBuffer, estructuraMensajeAGuardar, equipo);
    } else {
        snprintf(apuntadorBufferSalida, tamanoMaximoBuffer, "%s", estructuraMensajeAGuardar);
    }
}


static int enviarMensajeALBroker(int numeroSocketUDP,
                                  const struct sockaddr_in *ipPuertoBroker,
                                  const char *mensaje, int longitudMensaje,
                                  int secuencia,
                                  const char *identificadorPublicador) {
    //el buffer donde voy a guardar ese ACK
    char bufferACK[MAX_BUFFER_SIZE];

    //el ack que voy a escribir
    char ACKEsperado[64];
    snprintf(ACKEsperado, sizeof(ACKEsperado), "ACK|%d", secuencia);

    int tiempoEspera = ACK_TIMEOUT_MS;

    int intentos;
    for (intentos = 0; intentos < MAX_RETRIES; intentos++) {
        const struct sockaddr *direccionGenerica = (const struct sockaddr *) ipPuertoBroker;
        socklen_t longitudDireccion = sizeof(struct sockaddr_in);

        int envioExitoso = sendto(numeroSocketUDP, mensaje, longitudMensaje, 0,
                                   direccionGenerica, longitudDireccion);

        if (envioExitoso < 0) {
            fprintf(stderr, "[%s] Error sendto: %s\n", identificadorPublicador, strerror(errno));
            return -1;
        }

        if (intentos == 0) {
            printf("[%s] Mensaje seq=%d enviado al broker\n", identificadorPublicador, secuencia);
        } else {
            printf("[%s] Retransmision #%d seq=%d\n", identificadorPublicador, intentos, secuencia);
        }

        fd_set socketsDeLectura;
        FD_ZERO(&socketsDeLectura);
        FD_SET(numeroSocketUDP, &socketsDeLectura);

        long segundos      = tiempoEspera / 1000;
        long microsegundos = (tiempoEspera % 1000) * 1000;
        struct timeval tv;
        tv.tv_sec  = segundos;
        tv.tv_usec = microsegundos;

        int ready = select(numeroSocketUDP + 1, &socketsDeLectura, NULL, NULL, &tv);

        if (ready > 0) {
            struct sockaddr_in direccionPuertoBrokerRespuesta;
            socklen_t slen = sizeof(direccionPuertoBrokerRespuesta);

            struct sockaddr *direccionGenericaRespuesta =
                (struct sockaddr *) &direccionPuertoBrokerRespuesta;

            int bytesRecibidos = recvfrom(numeroSocketUDP,
                                           bufferACK, sizeof(bufferACK) - 1,
                                           0,
                                           direccionGenericaRespuesta, &slen);

            if (bytesRecibidos > 0) {
                bufferACK[bytesRecibidos] = '\0';
                //el ACK que llegó debemos compararlo con el ACK esperado
                if (strcmp(bufferACK, ACKEsperado) == 0) {
                    printf("[%s] ACK|%d confirmado por el broker\n",
                           identificadorPublicador, secuencia);
                    return 0;
                }
            }
        } else if (ready == 0) {
            printf("[%s] Timeout (%dms) esperando ACK|%d\n",
                   identificadorPublicador, tiempoEspera, secuencia);
        }

        //esto la verdad si fue consejo de chat y es para evitar congestionamiento
        //es decir, que cada vez que hubo timeOut esperando el ACK, se incrementa el tiempo de espera 
        //para no saturar la red
        tiempoEspera = tiempoEspera * 2;
    }

    fprintf(stderr, "[%s] FALLO: sin ACK tras %d intentos para seq=%d\n",
            identificadorPublicador, MAX_RETRIES, secuencia);
    return -1;
}


int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr,
                "Uso: %s <broker_ip> <broker_port> <publisher_id> <num_partido>\n",
                argv[0]);
        fprintf(stderr, "Ejemplo: %s 127.0.0.1 9003 pub1 1\n", argv[0]);
        fprintf(stderr, "\nMapeo de partidos:\n");
        fprintf(stderr, "  1 -> match_1_vs_2 (equipos 1 y 2)\n");
        fprintf(stderr, "  2 -> match_3_vs_4 (equipos 3 y 4)\n");
        fprintf(stderr, "  3 -> match_5_vs_6 (equipos 5 y 6)\n");
        exit(EXIT_FAILURE);
    }

    const char *broker_ip = argv[1];
    int broker_port = atoi(argv[2]);
    const char *pub_id = argv[3];
    int num_partido = atoi(argv[4]);

    if (num_partido <= 0 || num_partido > MAX_PARTIDOS) {
        fprintf(stderr, "[ERROR] Numero de partido debe estar entre 1 y %d\n",
                MAX_PARTIDOS);
        exit(EXIT_FAILURE);
    }

    int  team1 = 2 * num_partido - 1;
    int  team2 = 2 * num_partido;
    char topic[100];
    snprintf(topic, sizeof(topic), "match_%d_vs_%d", team1, team2);

    printf("===== PUBLISHER QUIC HIBRIDO =====\n");
    printf("Publisher ID:%s\n", pub_id);
    printf("Numero partido:%d\n", num_partido);
    printf("Tema:%s\n", topic);
    printf("Equipos:%d vs %d\n", team1, team2);
    printf("Broker:%s:%d\n", broker_ip, broker_port);
    printf("Confiabilidad:ACKs + Retransmision (backoff x%d) + Seq numbers\n\n", 2);

    //de nuevo el socket utiliza socket udp para el transporte
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "[ERROR] No se pudo crear el socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    printf("[OK] Socket UDP creado\n");

    struct sockaddr_in puertoIpBroker;
    memset(&puertoIpBroker, 0, sizeof(puertoIpBroker));
    puertoIpBroker.sin_family = AF_INET;
    puertoIpBroker.sin_port   = htons(broker_port);

    if (inet_pton(AF_INET, broker_ip, &puertoIpBroker.sin_addr) <= 0) {
        fprintf(stderr, "[ERROR] Direccion IP invalida: %s\n", broker_ip);
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("[OK] Broker configurado en %s:%d\n\n", broker_ip, broker_port);

    //esto sirve para generar un numero random, entonces cada publisher que se cree va a escoger un template random entre los 4
    srand((unsigned int)(time(NULL)) ^ (unsigned int)getpid());
    int indiceTemplateCorrespondiente = rand() % NUM_TEMPLATES;

    printf("Template escogido: %d\n", indiceTemplateCorrespondiente);
    printf("Enviando %d mensajes en orden cronologico...\n\n", MSGS_POR_TEMPLATE);


    int contador;
    for (contador = 0; contador < MSGS_POR_TEMPLATE; contador++) {
        int seq = contador + 1;
        int equipoDelMensajeEnContador = -1;
        //esto hace que en cada mensaje, se alterne equipo 1 y equipo 2 y asi no salga
        //siempre mensajes del mismo equipo xd
        if(contador % 2 == 0) {
            equipoDelMensajeEnContador = team1;
        } else {
            equipoDelMensajeEnContador = team2;
        }
        
        const char *plantilla = mensajesPorTemplate[indiceTemplateCorrespondiente][contador];

        char event[MAX_EVENT_MESSAGE];
        generarMensaje(event, sizeof(event), plantilla, equipoDelMensajeEnContador);

        char bufferMensaje[MAX_BUFFER_SIZE];
        int  len = snprintf(bufferMensaje, sizeof(bufferMensaje), "PUBLISH|%d|%s|%s", seq, topic, event);

        printf("[%s] Mensaje %d/%d (seq=%d): %s\n", pub_id, contador + 1, MSGS_POR_TEMPLATE, seq, event);

        int envioExitosoAlBroker = enviarMensajeALBroker(sockfd, &puertoIpBroker,bufferMensaje, len,seq, pub_id);

        int seEnviaronTodos = 0;
        int numeroMensajesExitosos = 0;
        int numeroFallos = 0;
        if (envioExitosoAlBroker == 0) {
            numeroMensajesExitosos++;
        } else {
            numeroFallos++;
            printf("[%s] Mensaje seq=%d no confirmado, continuando...\n",pub_id, seq);
        }

        int delay = (rand() % 21) + 5;
        usleep(delay * 100000);
    }
    //osea logro enviar todos sin problemas ya al final de la ejecucions
    if(numeroMensajesExitosos == 10){
        seEnviaronTodos = 1;
    }
    printf("\n========================================\n");
    printf("[%s] Resumen: %d/%d mensajes confirmados, %d no confirmados\n",
           pub_id, numeroMensajesExitosos, MSGS_POR_TEMPLATE, numeroFallos);
    printf("========================================\n\n");

    close(sockfd);
    printf("[OK] Socket cerrado\n");

    return 0;
}