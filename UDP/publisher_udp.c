#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

int main () {
    //como se explica en el libro, el serverport se escogió arbitrariamente
    int serverPort = 12000;

    /**
     * llamado a la función clientSocjet del sistema (<sys/socket.h>)
     *  AF_INET declara que la red está usando IPv4
     * SOCK_DGRAM es el tipo de socket, indica que es un socket UDP
     * 0 indica que se está utilizando el protocolo por defecto que corresponde a AF_INET y SOCK_DGRAM
     * Quiere decir que no estamos definiendo un protocolo especîfico (nos lo da el s/o)
    **/
    int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);

    //Crear la direccion local del servidor para bind().
    struct sockaddr_in serverAddr;

    //inicializar la estructura, el tercer parametro es el numero de bytes para llenar
    //para evitar basura en memoria
    memset(&serverAddr, 0, sizeof(serverAddr));

    //dice que el tipo de direccion (sin_family) es IPv$(AF_INET)
    serverAddr.sin_family = AF_INET;

    /**Guardar el puerto del servidor
     * serverPort en este caso es 12000
     * sin_port espera el puerto en formato de red?????
     * htons (host a network short) convierte de formato del host al formato de red
     * Es decir, convierte un enter poqueño a bytes de red
     * Se hace porque las máquinas pueden guardar numeros en distinto orden interno pero en red se usa un formato estandar
    **/
    serverAddr.sin_port = htons(serverPort);

    /**
     * Guardar al IP del servidor
     * sin_addr es el campo de dirección IP
     * INADDR_ANY significa que acepta paquetes dirigidos a cualquiera de las direcciones IP de la máquina
     */
    serverAddr.sin_addr.s_addr = INADDR_ANY;


    /** Asociar el serverSocket a una direccion local y un puerto. Si no se hace, nadie sabe en qué puerto recibir mensajes.
     * serverAddr es la dirección local que se quere asignar al socket (la estructura creada antes)
     * Se hace un cast con struct sockaddr * (es lo que espera bind())
     * sizeof(serverAddr) indica cuantos bytes ocupa la dirección en memoria
     */
    bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

    printf("The server is ready to receive");


    while (1){

        char mensaje[2048];
        //se guarda el mensaje que llega desde el cliente. 
        char mensajeModificado[2048];

        //una variable para guardar el tamaño de la dirección del servidor. 
        //socklen_t es un tipo especail para tamaños de direcciones de sockets
        //para poder devolver la dirección de quien lo envió
        socklen_t serverLen = sizeof(serverAddr);

        /** Recibir un paquete UDP
         * Se espera a que llegue un mensaje al socket, guarda el contenido en mnesaje, guarda la dirección origen en serverAddr
         * Devuelve cuantos bytes llegaron.
         * serverSocket es el socket del cual se recibe
         * mensaje es donde se guardan los datos recibidos
         * sizeof(mensaje)-1 es el máximo de bytes para guardar. Se deja un byte libre para el "\0"
         * 0 indica que no hay flags
         * serverAddr es la direccion del emisor (se castea con (struct sockaddr *))
         * &serverLen es el tamaño de la estructura dirección
         * Es importante recalcar que recvfrom puede cambiar este tamaño
         * n es entonces el numero de bytes que se recibieron. Si hubo un fallo, n=-1
         */
        int n = recvfrom(serverSocket, mensaje, sizeof(mensaje) -1, 0, (struct sockaddr *)&serverAddr, &serverLen);
        //en c los strings terminan con '\0'. Hay que agregarlo.
        mensaje[n] = '\0';

        //cambiar el mensaje a upper case
        int i;
        for (i=0; mensaje[i] != '\0'; i++){
            mensajeModificado[i] = toupper((unsigned char)mensaje[i]);
        }
        //en c los strings terminan con '\0'. Hay que agregarlo.
        mensajeModificado[i] = '\0';


        sendto(serverSocket, mensajeModificado, strlen(mensajeModificado), 0, (struct sockaddr *)&serverAddr, serverLen);
    }

    return 0;
}