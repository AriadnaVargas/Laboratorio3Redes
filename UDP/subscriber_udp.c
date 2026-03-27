#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main () {
    char *serverName = "127.0.0.1";
    //como se explica en el libro, el serverport se escogió arbitrariamente
    int serverPort = 12000;

    /**
     * llamado a la función clientSocjet del sistema (<sys/socket.h>)
     *  AF_INET declara que la red está usando IPv4
     * SOCK_DGRAM es el tipo de socket, indica que es un socket UDP
     * 0 indica que se está utilizando el protocolo por defecto que corresponde a AF_INET y SOCK_DGRAM
     * Quiere decir que no estamos definiendo un protocolo especîfico (nos lo da el s/o)
    **/
    int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);

    //equivalente a: message = input('input lowercase sentence')
    char mensaje[1024];
    printf("input lowercase sentence");
    fgets(mensaje, sizeof(mensaje), stdin);
    
    //Crear la direccion destino para el sendto().
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
     * s_addr es el valor numérico de la IP
     * inet_Addr convierte el string en el formato binario que necesita el socket
     */
    serverAddr.sin_addr.s_addr = inet_addr(serverName);

    /**Enviar el mensaje UDP al Servidor. 
     * clientSoocket es el socket desde el cual se va a enviar
     * mensaje es el texto que el usuario escribió
     * strlen es la cantidad de bytes que se van a enviar
     * 0 significa que es un envio normal, sin flags
     * serverAddr es la direccion del destino, utilizando la estructura antes creada serverAddr.
     * Se hace un cast para que se trate como una direccion generica(struct sockaddr)
     * sizeof(serverAddr) es el tamaño de la estructura de direccion. Indica cuantos bytes ocupa esa dirección en memoria
     */
    sendto(clientSocket, mensaje, strlen(mensaje), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

    //se guarda el mensaje que llega desde el servidor. 
    char mensajeModificado[2048];

    //una variable para guardar el tamaño de la dirección del servidor. 
    //socklen_t es un tipo especail para tamaños de direcciones de sockets
    //para poder devolver la dirección de quien lo envió
    socklen_t serverLen = sizeof(serverAddr);

    /** Recibir un paquete UDP
     * Se espera a que llegue un mensaje al socket, guarda el contenido en mnesajeModificado, guarda la dirección origen en serverAddr
     * Devuelve cuantos bytes llegaron.
     * clientSocket es el socket del cual se recibe
     * mensajeModificado es donde se guardan los datos recibidos
     * sizeof(mensajeModificado)-1 es el máximo de bytes para guardar. Se deja un byte libre para el "\0"
     * 0 indica que no hay flags
     * serverAddr es la direccion del emisor casteada
     * &serverLen es el tamaño de la estructura dirección
     * Es importante recalcar que recvfrom puede cambiar este tamaño
     * n es entonces el numero de bytes que se recibieron. Si hubo un fallo, n=-1
     */
    int n = recvfrom(clientSocket, mensajeModificado, sizeof(mensajeModificado) -1, 0, (struct sockaddr *)&serverAddr, &serverLen);

    //en c los strings terminan con '\0'. Hay que agregarlo.
    mensajeModificado[n] = '\0';

    //imprimir el mensaje recibido como texto.
    printf("%s", mensajeModificado);

    close(clientSocket);

    return 0;
}