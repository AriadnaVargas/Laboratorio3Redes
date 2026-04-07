# Laboratorio 3 - Pub-Sub System con TCP y UDP

## Descripción General

Sistema de publicación-suscripción (pub-sub) implementado en C que permite la distribución de mensajes entre múltiples clientes a través de un broker central. El laboratorio incluye dos versiones: una implementada con TCP y otra con UDP, permitiendo comparar sus características de confiabilidad, orden y control de flujo.

---

## Tabla de Contenidos

1. [Estructura del Proyecto](#estructura-del-proyecto)
2. [Librerías y Funciones Socket Utilizadas](#librerías-y-funciones-socket-utilizadas)
3. [Concurrencia y Sincronización (POSIX Threads)](#concurrencia-y-sincronización-posix-threads)
4. [Ejecución](#ejecución)


---

## Estructura del Proyecto

```
Laboratorio3Redes/
├── README.md
├── TCP/
│   ├── broker_tcp.c          # Servidor broker TCP
│   ├── publisher_tcp.c       # Cliente publicador TCP
│   └── subscriber_tcp.c      # Cliente suscriptor TCP
└── UDP/
    ├── broker_udp.c          # Servidor broker UDP
    ├── publisher_udp.c       # Cliente publicador UDP
    └── subscriber_udp.c      # Cliente suscriptor UDP
```

---

## Librerías y Funciones Socket Utilizadas

### 1. **`<sys/socket.h>`** - Socket API Principal

Proporciona las funciones fundamentales para crear y manipular sockets en sistemas POSIX.

#### Funciones Utilizadas:

##### **`socket(int domain, int type, int protocol)`**
```c
int socket(AF_INET, SOCK_STREAM, 0);  // TCP
int socket(AF_INET, SOCK_DGRAM, 0);   // UDP
```
- **Propósito**: Crea un nuevo socket
- **Parámetros**:
  - `domain`: `AF_INET` para IPv4
  - `type`: `SOCK_STREAM` (TCP - confiable, orientado a conexión) o `SOCK_DGRAM` (UDP - no confiable)
  - `protocol`: 0 usa el protocolo por defecto para el tipo especificado
- **Retorna**: Descriptor del socket (>= 0) o -1 en caso de error
- **Uso en Lab**: Crear sockets en broker, publishers y subscribers

##### **`bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)`**
```c
bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
```
- **Propósito**: Vincula un socket a una dirección IP y puerto específicos
- **Parámetros**:
  - `sockfd`: Descriptor del socket
  - `addr`: Estructura con la dirección (IP + puerto)
  - `addrlen`: Tamaño de la estructura de dirección
- **Retorna**: 0 si éxito, -1 en caso de error
- **Uso en Lab**: Solo en broker (servidor), vincula a puerto 9001

##### **`listen(int sockfd, int backlog)`**
```c
listen(server_socket, 5);
```
- **Propósito**: Pone el socket en modo escucha para acepar conexiones entrantes
- **Parámetros**:
  - `sockfd`: Descriptor del socket
  - `backlog`: Número máximo de conexiones pendientes en la cola
- **Retorna**: 0 si éxito, -1 en caso de error
- **Uso en Lab**: Solo en broker TCP, prepara para aceptar clientes

##### **`accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)`**
```c
int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &len);
```
- **Propósito**: Acepta una conexión entrante y crea un nuevo socket para ella
- **Parámetros**:
  - `sockfd`: Socket del servidor en escucha
  - `addr`: Estructura que se rellena con datos del cliente
  - `addrlen`: Puntero al tamaño (se actualiza con el tamaño real)
- **Retorna**: Nuevo descriptor de socket para el cliente, o -1 en error
- **Uso en Lab**: Solo en broker TCP, acepta publishers y subscribers

##### **`connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)`**
```c
connect(client_socket, (struct sockaddr *)&broker_addr, sizeof(broker_addr));
```
- **Propósito**: Conecta un socket cliente a un servidor remoto
- **Parámetros**:
  - `sockfd`: Descriptor del socket cliente
  - `addr`: Estructura con dirección del servidor
  - `addrlen`: Tamaño de la estructura de dirección
- **Retorna**: 0 si éxito, -1 en caso de error
- **Uso en Lab**: En publishers y subscribers para conectarse al broker

##### **`send(int sockfd, const void *buf, size_t len, int flags)`**
```c
send(socket, message_buffer, strlen(message_buffer), 0);
```
- **Propósito**: Envía datos a través de un socket conectado
- **Parámetros**:
  - `sockfd`: Descriptor del socket
  - `buf`: Buffer con datos a enviar
  - `len`: Número de bytes a enviar
  - `flags`: Opciones (0 = sin opciones especiales)
- **Retorna**: Número de bytes enviados, o -1 en error
- **Uso en Lab**: Publishers envían mensajes; broker y subscribers envían datos
- **Nota TCP**: Garantiza entrega en orden
- **Nota UDP**: No garantiza entrega

##### **`recv(int sockfd, void *buf, size_t len, int flags)`**
```c
bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0);
```
- **Propósito**: Recibe datos de un socket conectado
- **Parámetros**:
  - `sockfd`: Descriptor del socket
  - `buf`: Buffer donde guardar datos
  - `len`: Tamaño máximo del buffer
  - `flags`: Opciones (0 = sin opciones especiales)
- **Retorna**: Número de bytes recibidos (0 = conexión cerrada, -1 = error)
- **Uso en Lab**: Broker recibe mensajes de publishers; subscribers reciben del broker
- **Nota TCP**: Datos completos y en orden
- **Nota UDP**: Cada datagrama puede perderse o llegar duplicado

##### **`sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)`**
```c
sendto(clientSocket, message, strlen(message), 0,
       (struct sockaddr *)&serverAddr, sizeof(serverAddr));
```
- **Propósito**: Envía un datagrama UDP a una dirección específica sin requerir una conexión previa
- **Parámetros**:
  - `sockfd`: Descriptor del socket
  - `buf`: Buffer con los datos a enviar
  - `len`: Número de bytes a enviar
  - `flags`: Opciones adicionales, normalmente `0`
  - `dest_addr`: Dirección de destino
  - `addrlen`: Tamaño de la estructura de dirección
- **Retorna**: Número de bytes enviados, o `-1` en error
- **Uso en Lab**:
  - `publisher_udp.c` envía publicaciones al broker
  - `subscriber_udp.c` envía suscripciones al broker
  - `broker_udp.c` reenvía publicaciones a los subscribers

##### **`recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)`**
```c
recvfrom(clientSocket, receivedMessage, sizeof(receivedMessage) - 1, 0,
         (struct sockaddr *)&serverAddr, &serverLen);
```
- **Propósito**: Recibe un datagrama UDP e informa desde qué dirección fue enviado
- **Parámetros**:
  - `sockfd`: Descriptor del socket
  - `buf`: Buffer donde guardar datos
  - `len`: Tamaño máximo del buffer
  - `flags`: Opciones adicionales, normalmente `0`
  - `src_addr`: Estructura donde se almacena la dirección del emisor
  - `addrlen`: Tamaño de la estructura de dirección
- **Retorna**: Número de bytes recibidos, o `-1` en error
- **Uso en Lab**:
  - `broker_udp.c` recibe publicaciones y suscripciones
  - `subscriber_udp.c` recibe mensajes reenviados por el broker

##### **`close(int fd)`**
```c
close(socket);
```
- **Propósito**: Cierra un descriptor de archivo (socket)
- **Parámetros**:
  - `fd`: Descriptor a cerrar
- **Retorna**: 0 si éxito, -1 en error
- **Uso en Lab**: Cierra sockets cuando finaliza conexión

---

### 2. **`<netinet/in.h>`** - Definiciones de Internet

Define estructuras y constantes para direcciones y puertos de internet.

#### Tipos y Constantes Utilizadas:

##### **`struct sockaddr_in`**
```c
struct sockaddr_in {
    sa_family_t sin_family;      // Familia de direcciones (AF_INET para IPv4)
    uint16_t sin_port;           // Puerto (debe estar en order de red - big endian)
    struct in_addr sin_addr;     // Dirección IP
    char sin_zero[8];            // Padding (debe estar en cero)
};
```
- **Uso**: Almacena dirección IP, puerto y familia de protocolos
- **En el Lab**: Configurar dirección del broker y clientes

##### **`htons(uint16_t hostshort)` - Host TO Network Short**
```c
server_addr.sin_port = htons(BROKER_PORT);  // Convierte 9001 a orden de red
```
- **Propósito**: Convierte un número de puerto de orden de host a orden de red
- **Orden de red**: Big endian (bytes más significativos primero)
- **En el Lab**: Convertir puerto 9001 antes de asignarlo

##### **`ntohs(uint16_t netshort)` - Network TO Host Short**
```c
printf("Puerto: %d\n", ntohs(client_addr.sin_port));
```
- **Propósito**: Convierte puerto de orden de red a orden de host
- **En el Lab**: Mostrar puertos de conexión en mensajes de debug

##### **`INADDR_ANY`**
```c
server_addr.sin_addr.s_addr = INADDR_ANY;
```
- **Propósito**: Vincula socket a todas las interfaces de red disponibles
- **Valor**: Típicamente 0.0.0.0
- **En el Lab**: Podría usarse para que broker escuche en todas las interfaces

---

### 3. **`<arpa/inet.h>`** - Internet Address Manipulation

Proporciona funciones para convertir direcciones IP entre notación de puntos y formato binario.

#### Funciones Utilizadas:

##### **`inet_aton(const char *cp, struct in_addr *inp)`**
```c
inet_aton("127.0.0.1", &server_addr.sin_addr);
```
- **Propósito**: Convierte dirección IP en notación decimal con puntos a formato binario
- **Parámetros**:
  - `cp`: Cadena con dirección IP (ej: "127.0.0.1")
  - `inp`: Puntero a estructura `in_addr` donde guardar el resultado
- **Retorna**: 1 si éxito, 0 si formato inválido
- **En el Lab**: Convertir "127.0.0.1" a formato binario

##### **`inet_ntoa(struct in_addr in)`**
```c
printf("IP: %s\n", inet_ntoa(client_addr.sin_addr));
```
- **Propósito**: Convierte dirección IP binaria a notación decimal con puntos
- **Parámetros**:
  - `in`: Estructura con dirección IP en formato binario
- **Retorna**: Cadena con dirección IP (ej: "127.0.0.1")
- **En el Lab**: Mostrar IP del cliente en mensajes del broker

##### **`inet_addr(const char *cp)`**
```c
serverAddr.sin_addr.s_addr = inet_addr(serverName);
```
- **Propósito**: Convierte una dirección IPv4 en texto a formato binario
- **Parámetros**:
  - `cp`: Cadena con dirección IP (ej: "127.0.0.1")
- **Retorna**: Dirección en formato de red
- **En el Lab**: Configurar la IP del broker en `publisher_udp.c` y `subscriber_udp.c`

---

### 4. **`<stdio.h>`** - Entrada/Salida Estándar

Funciones para lectura/escritura en consola y archivos.

#### Funciones Utilizadas:

##### **`printf(const char *format, ...)`**
- **Propósito**: Imprime mensajes formateados en stdout
- **En el Lab**: Mostrar estado del sistema, mensajes recibidos, etc.

##### **`fprintf(FILE *stream, const char *format, ...)`**
```c
fprintf(stderr, "[ERROR] Descripción del error\n");
```
- **Propósito**: Imprime en stream específico (stderr para errores)
- **En el Lab**: Mostrar errores críticos

##### **`snprintf(char *str, size_t size, const char *format, ...)`**
```c
snprintf(message_buffer, sizeof(message_buffer), "PUBLISH|%s|%s", topic, message);
```
- **Propósito**: Formatea cadena en buffer (sin overflow)
- **Seguro vs sprintf**: Previene buffer overflow especificando tamaño máximo
- **En el Lab**: Construir mensajes en formato PUBLISH|topic|message

---

### 5. **`<stdlib.h>`** - Utilidades Estándar

Funciones de utilidad general.

#### Funciones Utilizadas:

##### **`malloc(size_t size)` / `free(void *ptr)`**
- **Propósito**: Asignación dinámica de memoria
- **En el Lab**: Si se necesita allocar arrays dinámicos

##### **`exit(int status)`**
```c
exit(EXIT_FAILURE);  // Terminar con código de error
```
- **Propósito**: Termina el programa
- **Parámetros**: 0 = éxito, no-cero = error
- **En el Lab**: Salir ante errores críticos

##### **`atoi(const char *str)`**
```c
int broker_port = atoi(argv[2]);
```
- **Propósito**: Convierte cadena a entero
- **En el Lab**: Convertir puerto desde argumentos de línea de comandos

---

### 6. **`<string.h>`** - Manipulación de Cadenas

Funciones para trabajar con cadenas y bloques de memoria.

#### Funciones Utilizadas:

##### **`memset(void *s, int c, size_t n)`**
```c
memset(&server_addr, 0, sizeof(server_addr));
memset(buffer, 0, sizeof(buffer));
```
- **Propósito**: Llena un bloque de memoria con un valor
- **Uso típico**: Inicializar estructuras en cero
- **En el Lab**: Limpiar buffers y estructuras de red

##### **`strcpy(char *dest, const char *src)` / `strncpy(...)`**
```c
strcpy(topic, token);  // Copiar tema
```
- **Propósito**: Copiar cadena (strncpy es más segura)
- **En el Lab**: Copiar temas y mensajes

##### **`strlen(const char *s)`**
```c
send(socket, buffer, strlen(buffer), 0);
```
- **Propósito**: Obtiene longitud de cadena
- **En el Lab**: Calcular bytes a enviar

##### **`strcmp(const char *s1, const char *s2)`**
```c
if (strcmp(token, "PUBLISH") == 0)
```
- **Propósito**: Compara dos cadenas (0 = iguales)
- **En el Lab**: Identificar tipo de mensaje (PUBLISH vs SUBSCRIBE)

##### **`strncmp(const char *s1, const char *s2, size_t n)`**
```c
if (strncmp(initial_message, "PUBLISH", 7) == 0)
```
- **Propósito**: Compara primeros n caracteres
- **Más seguro**: Evita lectura de memoria más allá del esperado
- **En el Lab**: Validar tipos de mensaje

##### **`strtok(char *str, const char *delim)`**
```c
char *token = strtok(initial_message, "|");
```
- **Propósito**: Tokeniza cadena según delimitador
- **Uso**: Parsear "PUBLISH|topic|message" o "SUBSCRIBE|topic"
- **En el Lab**: Separar componentes de mensajes

---

### 7. **`<unistd.h>`** - POSIX API

Proporciona acceso a POSIX API, incluyendo funciones de sistema.

#### Funciones Utilizadas:

##### **`close(int fd)` - (Ya documentado arriba)**
```c
close(socket);
```

##### **`sleep(unsigned int seconds)`**
```c
sleep(1);  // Esperar 1 segundo
```
- **Propósito**: Pausa ejecución del programa
- **Parámetro**: Segundos a esperar
- **En el Lab**: Publishers esperan 1 segundo entre mensajes (simular eventos en vivo)

---

### 8. **`<errno.h>`** - Códigos de Error

Define variable y funciones para manejo de errores.

#### Funciones Utilizadas:

##### **`strerror(int errnum)`**
```c
fprintf(stderr, "[ERROR] No se pudo crear socket: %s\n", strerror(errno));
```
- **Propósito**: Convierte código de error a mensaje descriptivo
- **Variable `errno`**: Contiene código del último error de sistema
- **En el Lab**: Mostrar mensajes de error detallados y comprensibles

---

### 9. **`<signal.h>`** - Manejo de Señales (Solo Subscribers)

Permite capturar y manejar señales del sistema (como Ctrl+C).

#### Funciones Utilizadas:

##### **`signal(int signum, void (*handler)(int))`**
```c
signal(SIGINT, signal_handler);  // Capturar Ctrl+C
```
- **Propósito**: Registra manejador personalizado para una señal
- **Parámetro**: `SIGINT` = Ctrl+C
- **En el Lab**: Permitir cierre graceful del subscriber con Ctrl+C

---

## Concurrencia y Sincronización (POSIX Threads)

### Introducción a Threads

El broker TCP utiliza **threads POSIX (pthread)** para manejar múltiples publishers simultáneamente. Sin threading, el broker procesaría publishers secuencialmente, causando que otros esperen.

### Librería: **`<pthread.h>`** - POSIX Threads

Proporciona funciones para crear y sincronizar threads en sistemas POSIX.

#### Conceptos Clave:

##### **Mutex (Mutual Exclusion)**
Mecanismo de sincronización que garantiza que solo un thread acceda a una sección crítica a la vez.

#### Funciones Utilizadas:

##### **`pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)`**
```c
pthread_t publisher_thread;
publisher_args_t *args = malloc(sizeof(publisher_args_t));
args->socket = client_socket;
pthread_create(&publisher_thread, NULL, handle_publisher, args);
```
- **Propósito**: Crea un nuevo thread
- **Parámetros**:
  - `thread`: Puntero donde guardar el ID del thread creado
  - `attr`: Atributos (NULL = atributos por defecto)
  - `start_routine`: Función a ejecutar en el thread
  - `arg`: Argumentos para la función (void *)
- **Retorna**: 0 si éxito, número de error si falla
- **Uso en Lab**: Crear un thread para cada publisher que se conecte

##### **`pthread_detach(pthread_t thread)`**
```c
pthread_detach(publisher_thread);
```
- **Propósito**: Marca un thread para que se limpie automáticamente al terminar
- **Alternativa**: `pthread_join()` requeriría esperar explícitamente
- **Uso en Lab**: Permite que threads de publishers terminen sin bloquear el broker

##### **`pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)`**
```c
pthread_mutex_t subscribers_mutex = PTHREAD_MUTEX_INITIALIZER;
```
- **Propósito**: Inicializa un mutex
- **En Lab**: Inicialización estática con `PTHREAD_MUTEX_INITIALIZER`

##### **`pthread_mutex_lock(pthread_mutex_t *mutex)`**
```c
pthread_mutex_lock(&subscribers_mutex);
// Sección crítica - solo este thread puede ejecutar
```
- **Propósito**: Adquiere el mutex (bloquea si otro thread lo tiene)
- **Uso en Lab**: Antes de leer/modificar la lista de subscribers

##### **`pthread_mutex_unlock(pthread_mutex_t *mutex)`**
```c
pthread_mutex_unlock(&subscribers_mutex);
// Otros threads pueden ahora adquirir el mutex
```
- **Propósito**: Libera el mutex para que otros threads lo adquieran
- **Uso en Lab**: Después de terminar lectura/modificación de subscribers

#### Ejemplo de Sincronización en el Broker:

```c
// Función protegida por mutex
void forward_message_to_subscribers(const char *topic, const char *message) {
    pthread_mutex_lock(&subscribers_mutex);      // ADQUIRIR MUTEX
    
    // Sección crítica - acceso seguro a subscribers[]
    for (int i = 0; i < num_subscribers; i++) {
        if (strcmp(subscribers[i].topic, topic) == 0) {
            send(subscribers[i].socket, message, strlen(message), 0);
        }
    }
    
    pthread_mutex_unlock(&subscribers_mutex);    // LIBERAR MUTEX
}
```

---

## Ejecución

Esta guía te explica paso a paso cómo compilar y ejecutar el sistema **Publicador-Suscriptor (Pub-Sub) Multi-Match** usando TCP en WSL.

---

### 1. Abre tu terminal de WSL:

```bash
wsl
```

### 2. Ir a la carpeta del proyecto TCP

```bash
cd ../Laboratorio3Redes/TCP
```

### 3. Compilar los programas

```bash
gcc -Wall -Wextra -pthread -o broker_tcp broker_tcp.c
gcc -Wall -Wextra -o publisher_tcp publisher_tcp.c
gcc -Wall -Wextra -o subscriber_tcp subscriber_tcp.c
```

### 4. Ejecutar en múltiples terminales WSL

**Estructura Requerida:**
- **Terminal 1**: 1 broker central
- **Terminal 2**: 1 subscriber que se suscribe a `i` partidos
- **Terminales 3 a (i+2)**: `i` publishers, uno por cada partido

**Ejemplo: Si el subscriber se suscribe a 3 partidos (i=3)**
- Se necesitan exactamente **3 publishers** (pub1, pub2, pub3)
- Cada publisher debe publicar a su respectivo partido (1, 2, 3)

---

#### Terminal 1: Broker (Central)
```bash
./broker_tcp
```

---

#### Terminal 2: Subscriber (Recibe de i partidos)
```bash
./subscriber_tcp 127.0.0.1 9001 sub1 i
./subscriber_tcp 127.0.0.1 9001 sub2 i
```

**Donde `i` es la cantidad de partidos a los que se suscribe:**
- `i=1`: Se suscribe solo a `match_1_vs_2`
- `i=2`: Se suscribe a `match_1_vs_2` y `match_3_vs_4`
- `i=3`: Se suscribe a `match_1_vs_2`, `match_3_vs_4`, `match_5_vs_6`

**Salida esperada:**
```
[Subscriber sub1] Iniciando suscripción a 3 partidos...
[Subscriber sub1] Suscrito a topic: match_1_vs_2
[Subscriber sub1] Suscrito a topic: match_3_vs_4
[Subscriber sub1] Suscrito a topic: match_5_vs_6
```

---

#### Terminales 3 a (i+2): Publishers (Uno por cada partido)

**Si i=3, necesitas exactamente 3 publishers:**

**Terminal 3: Publisher para Partido 1**
```bash
./publisher_tcp 127.0.0.1 9001 pub1 1
```
Publica en: `match_1_vs_2` (equipos 1 vs 2)

**Terminal 4: Publisher para Partido 2**
```bash
./publisher_tcp 127.0.0.1 9001 pub2 2
```
Publica en: `match_3_vs_4` (equipos 3 vs 4)

**Terminal 5: Publisher para Partido 3**
```bash
./publisher_tcp 127.0.0.1 9001 pub3 3
```
Publica en: `match_5_vs_6` (equipos 5 vs 6)

**En general:**
```bash
./publisher_tcp 127.0.0.1 9001 pubi i
```
Donde i es un numero entero

**Salida esperada de cada publisher:**
```
[pub1] Mensaje 1 enviado: ¡GOL! Equipo 1 abre el marcador al minuto 3
[pub1] Mensaje 2 enviado: Segundo tiempo inicia con intensidad
[pub1] Mensaje 3 enviado: Tarjeta amarilla para número 7 de equipo 2
...
```

---

### ⚠️ REGLA IMPORTANTE: Coincidencia de Publishers con Partidos

| Si Subscriber se suscribe a | Entonces necesitas | Publishers |
|-----|-----------|----------|
| i=1 | 1 publisher | `pub1` con partido `1` |
| i=2 | 2 publishers | `pub1` con `1`, `pub2` con `2` |
| i=3 | 3 publishers | `pub1` con `1`, `pub2` con `2`, `pub3` con `3` |
| i=N | N publishers | `pub1` con `1`, `pub2` con `2`, ..., `pubN` con `N` |

**Fórmula:**
```
Para cada j = 1 hasta i:
  Terminal (j+2): ./publisher_tcp 127.0.0.1 9001 pubj j
```

---

### 5. Comportamiento Esperado

**Salida esperada del Subscriber (Terminal 2):**
```
[Subscriber sub1] Iniciando suscripción a 2 matches...
[Subscriber sub1] Suscrito a topic: match_1_vs_2
[Subscriber sub1] Suscrito a topic: match_3_vs_4
[Subscriber sub1] Mensaje de match_1_vs_2: Evento del equipo 1
[Subscriber sub1] Mensaje de match_3_vs_4: Evento del equipo 3
[Subscriber sub1] Mensaje de match_1_vs_2: Evento del equipo 2
[Subscriber sub1] Mensaje de match_3_vs_4: Evento del equipo 4
...
```

Los mensajes de diferentes matches aparecen **intercalados en tiempo real** según se publiquen.

---

### 6. Parámetros del Sistema

#### Para Subscribers
- **Formato**: `./subscriber_tcp <IP> <PUERTO> <NOMBRE> <NUM_MATCHES>`
- **IP**: 127.0.0.1
- **PUERTO**: 9001
- **NOMBRE**: nombre del subscriptor de la forma subj donde j=1,2,...
- **NUM_MATCHES**: Número de matches a los que suscribirse (1-20)
- **Tópicos generados**: `match_1_vs_2`, `match_3_vs_4`, `match_5_vs_6`, ...

#### Para Publishers
- **Formato**: `./publisher_tcp <IP> <PUERTO> <NOMBRE> <NUM_PARTIDO>`
- **IP**: 127.0.0.1
- **PUERTO**: 9001
- **NOMBRE**: nombre del publicador de la forma pubj donde j=1,2,...
- **NUM_PARTIDO**: Número del partido a publicar (1-20)
- **Equipos generados**: 
  - Partido 1 → Equipos 1 vs 2
  - Partido 2 → Equipos 3 vs 4
  - Partido 3 → Equipos 5 vs 6
  - Fórmula: Equipo 1 = `2*partido - 1`, Equipo 2 = `2*partido`

---

### 7. Tabla de Comparativa de Matches

| Partido | Topic | Equipo 1 | Equipo 2 |
|---------|-------|----------|----------|
| 1 | `match_1_vs_2` | 1 | 2 |
| 2 | `match_3_vs_4` | 3 | 4 |
| 3 | `match_5_vs_6` | 5 | 6 |
| ... | ... | ... | ... |
| N | `match_{2N-1}_vs_{2N}` | 2N-1 | 2N |


## Ejecución UDP

Esta guía complementa la sección de ejecución existente y muestra cómo correr la versión UDP del laboratorio.

### 1. Ir a la carpeta del proyecto UDP

```bash
cd ../Laboratorio3Redes/UDP
```

### 2. Compilar los programas UDP

```bash
gcc -Wall -Wextra -o broker_udp broker_udp.c
gcc -Wall -Wextra -o publisher_udp publisher_udp.c
gcc -Wall -Wextra -o subscriber_udp subscriber_udp.c
```

### 3. Ejecutar en múltiples terminales

**Estructura recomendada:**
- **Terminal 1**: broker UDP
- **Terminal 2**: uno o más subscribers UDP
- **Terminales siguientes**: uno o más publishers UDP

### 4. Terminal 1: Broker UDP

```bash
./broker_udp 9001
```

### 5. Terminal 2: Subscriber UDP

```bash
./subscriber_udp 127.0.0.1 9001 sub1 2
```

**Formato:**

```bash
./subscriber_udp <broker_ip> <broker_port> <subscriber_name> <num_matches>
```

- `broker_ip`: IP del broker.
- `broker_port`: puerto del broker.
- `subscriber_name`: nombre del subscriber.
- `num_matches`: cantidad de partidos a los que se suscribe automáticamente.

**Ejemplo de tópicos generados para `num_matches = 2`:**
- `match_1_vs_2`
- `match_3_vs_4`

### 6. Terminales siguientes: Publisher UDP

```bash
./publisher_udp 127.0.0.1 9001 pub1 1
```

**Formato:**

```bash
./publisher_udp <broker_ip> <broker_port> <publisher_name> <match_number>
```

- `broker_ip`: IP del broker.
- `broker_port`: puerto del broker.
- `publisher_name`: nombre del publisher.
- `match_number`: número de partido que se desea publicar.

**Relación entre número de partido y tópico:**
- Partido `1` -> `match_1_vs_2`
- Partido `2` -> `match_3_vs_4`
- Partido `3` -> `match_5_vs_6`

### 7. Orden recomendado

1. Iniciar `broker_udp`.
2. Iniciar uno o más `subscriber_udp`.
3. Iniciar uno o más `publisher_udp`.

### 8. Comportamiento esperado en UDP

- El subscriber envía suscripciones con formato `SUBSCRIBE|topic`.
- El publisher envía publicaciones con formato `PUBLISH|topic|contenido`.
- El broker recibe ambos tipos de datagramas y reenvía cada publicación a los subscribers del tópico correspondiente.
- Como UDP no establece conexión, no hay garantía de entrega, orden o retransmisión automática.


---

## Referencias

### Documentación de Socket API
- POSIX Socket API: https://pubs.opengroup.org/onlinepubs/9699919799/
- Beej's Guide to Network Programming: https://beej.us/guide/bgnet/

### Estándares
- RFC 793: TCP (Transmission Control Protocol)
- RFC 768: UDP (User Datagram Protocol)
- RFC 791: IP (Internet Protocol)

### Para mi bono de quic xd

La ruta en mi compu (para pegarlo de una vez):

cd /mnt/c/users/pc/desktop/redes3/laboratorio3redes/quic

Para compilarlos:

gcc -Wall -Wextra -pthread -o broker_tcp broker_tcp.c
gcc -Wall -Wextra -o publisher_tcp publisher_tcp.c
gcc -Wall -Wextra -o subscriber_tcp subscriber_tcp.c

Para correr el broker:

./broker_quic

Para correr los suscriptores:

./subscriber_quic 127.0.0.1 9003 sub1 2
./subscriber_quic 127.0.0.1 9003 sub2 2

Para correr los publicadores:

./publisher_quic 127.0.0.1 9003 pub1 1
./publisher_quic 127.0.0.1 9003 pub2 2

O para que en realidad lo pueda ver en WireShark CAPTURANDO DESDE WSL:

primero en una terminal cualqueira:

sudo tcpdump -i any udp port 9003 -w quic_pubsub.pcap

y luego en 5 terminales distintas:

./broker_quic

./subscriber_quic 172.24.254.165 9003 sub1 2

./subscriber_quic 172.24.254.165 9003 sub2 2

./publisher_quic 172.24.254.165 9003 pub1 1

./publisher_quic 172.24.254.165 9003 pub2 2

donde ese 172.24. bla bla sale de estar parado en wsl y poner:

hostname -I

y poner esa ip