# Laboratorio 3 - Pub-Sub System con TCP y UDP

## Descripción General

Sistema de publicación-suscripción (pub-sub) implementado en C que permite la distribución de mensajes entre múltiples clientes a través de un broker central. El laboratorio incluye dos versiones: una implementada con TCP y otra con UDP, permitiendo comparar sus características de confiabilidad, orden y control de flujo.

---

## Tabla de Contenidos

1. [Estructura del Proyecto](#estructura-del-proyecto)
2. [Librerías y Funciones Socket Utilizadas](#librerías-y-funciones-socket-utilizadas)
3. [TCP - Implementación](#tcp---implementación)
4. [UDP - Implementación](#udp---implementación)
5. [Compilación](#compilación)
6. [Ejecución](#ejecución)
7. [Protocolo de Comunicación](#protocolo-de-comunicación)

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

## TCP - Implementación

### Características de TCP
- ✅ **Confiable**: Garantiza entrega de datos
- ✅ **Ordenado**: Mensajes llegan en el mismo orden
- ✅ **Orientado a conexión**: Establece conexión antes de enviar datos
- ✅ **Control de flujo**: Ajusta velocidad de transmisión automáticamente

### Archivos TCP

#### **`TCP/broker_tcp.c`**
```
Tamaño: ~12 KB
Descripción: Servidor central que:
- Escucha en puerto 9001
- Acepta conexiones de publishers y subscribers
- Mantiene lista de subscriptores por tema
- Reenvía mensajes a subscribers interesados
```

**Funciones Socket Principales**:
- `socket()`, `bind()`, `listen()`, `accept()`, `send()`, `recv()`, `close()`

**Protocolo**:
- Broker identifica cliente según primer mensaje (PUBLISH o SUBSCRIBE)
- Publishers: envían formato `PUBLISH|topic|message`
- Subscribers: envían `SUBSCRIBE|topic` y reciben mensajes

#### **`TCP/publisher_tcp.c`**
```
Tamaño: ~5.4 KB
Descripción: Cliente publicador que:
- Se conecta al broker en 127.0.0.1:9001
- Envía 15 mensajes deportivos (> 10 requeridos)
- Espera 1 segundo entre mensajes
- Se desconecta después de enviar todos
```

**Funciones Socket Principales**:
- `socket()`, `connect()`, `send()`, `close()`

**Uso**:
```bash
./publisher_tcp <broker_ip> <broker_port> <publisher_id> <topic>
./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B
```

#### **`TCP/subscriber_tcp.c`**
```
Tamaño: ~6.7 KB
Descripción: Cliente suscriptor que:
- Se conecta al broker
- Se suscribe a 1 o más temas
- Recibe todos los mensajes de esos temas
- Permanece conectado hasta Ctrl+C
```

**Funciones Socket Principales**:
- `socket()`, `connect()`, `send()`, `recv()`, `close()`

**Funciones POSIX**:
- `signal()` para manejar Ctrl+C

**Uso**:
```bash
./subscriber_tcp <broker_ip> <broker_port> <subscriber_id> <topic1> [topic2] ...
./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B
./subscriber_tcp 127.0.0.1 9001 sub2 match_A_vs_B match_C_vs_D
```

---

## UDP - Implementación

### Características de UDP
- ❌ **No confiable**: Mensajes pueden perderse
- ❌ **Sin orden garantizado**: Pueden llegar desordenados
- ✅ **Sin conexión**: Envía datagramas independientes
- ✅ **Bajo overhead**: Más rápido que TCP

*Nota: Implementación UDP pendiente*

---

## Compilación

### En Linux/Mac/Unix:

```bash
# TCP
cd TCP
gcc -o broker_tcp broker_tcp.c
gcc -o publisher_tcp publisher_tcp.c
gcc -o subscriber_tcp subscriber_tcp.c

# UDP (cuando esté lista)
cd ../UDP
gcc -o broker_udp broker_udp.c
gcc -o publisher_udp publisher_udp.c
gcc -o subscriber_udp subscriber_udp.c
```

### Flags recomendados:
```bash
gcc -Wall -Wextra -o broker_tcp broker_tcp.c    # Con advertencias
gcc -std=c99 -o broker_tcp broker_tcp.c         # Especificar estándar C99
```

---

## Ejecución

### Caso de Prueba: 2 Publishers, 2 Subscribers

**Terminal 1 - Broker:**
```bash
cd TCP
./broker_tcp
```

**Terminal 2 - Subscriber 1 (match_A_vs_B):**
```bash
cd TCP
./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B
```

**Terminal 3 - Subscriber 2 (match_C_vs_D):**
```bash
cd TCP
./subscriber_tcp 127.0.0.1 9001 sub2 match_C_vs_D
```

**Terminal 4 - Publisher 1 (match_A_vs_B):**
```bash
cd TCP
./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B
```

**Terminal 5 - Publisher 2 (match_C_vs_D):**
```bash
cd TCP
./publisher_tcp 127.0.0.1 9001 pub2 match_C_vs_D
```

### Resultado Esperado:
```
✅ sub1 recibe 15 mensajes de pub1 (match_A_vs_B)
✅ sub2 recibe 15 mensajes de pub2 (match_C_vs_D)
✅ sub1 NO recibe mensajes de pub2
✅ sub2 NO recibe mensajes de pub1
✅ Todos los mensajes llegan en orden correcto
```

---

## Protocolo de Comunicación

### Formato de Mensajes

#### Publicador → Broker (TCP):
```
PUBLISH|<topic>|<mensaje>

Ejemplo:
PUBLISH|match_A_vs_B|Gol de Equipo A al minuto 32
```

#### Suscriptor → Broker (TCP):
```
SUBSCRIBE|<topic>

Ejemplo:
SUBSCRIBE|match_A_vs_B
```

#### Broker → Suscriptor (TCP):
```
[<topic>] <mensaje>

Ejemplo:
[match_A_vs_B] Gol de Equipo A al minuto 32
```

---

## Notas Importantes

### TCP vs UDP (En este lab)
| Aspecto | TCP | UDP |
|--------|-----|-----|
| **Confiabilidad** | ✅ Garantizada | ❌ No garantizada |
| **Orden** | ✅ Garantizado | ❌ Sin garantía |
| **Orientación** | Conexión | Sin conexión |
| **Overhead** | Mayor | Menor |
| **Velocidad** | Más lento | Más rápido |
| **Ideal para** | Chat, Email | Streaming, Gaming |

### Limitaciones Actuales
- ⚠️ Sin threads: Broker TCP procesa conexiones secuencialmente
- ⚠️ Max 100 subscribers simultáneos (limite configurable)
- ⚠️ Max 512 bytes por mensaje (tamaño configurable)
- ⚠️ Sin persistencia de mensajes

### Mejoras Futuras
- [ ] Usar pthreads o select()/poll() para multiplexing
- [ ] Aumentar límites dinámicamente
- [ ] Agregar persistencia con base de datos
- [ ] Implementar autenticación y autorización
- [ ] Comparación detallada TCP vs UDP con Wireshark

---

## Referencias

### Documentación de Socket API
- POSIX Socket API: https://pubs.opengroup.org/onlinepubs/9699919799/
- Linux man pages: `man socket`, `man send`, `man bind`, etc.
- Beej's Guide to Network Programming: https://beej.us/guide/bgnet/

### Estándares
- RFC 793: TCP (Transmission Control Protocol)
- RFC 768: UDP (User Datagram Protocol)
- RFC 791: IP (Internet Protocol)

---

**Autor**: Laboratorio de Redes - Universidad de los Andes
**Fecha**: Marzo 2026
**Estado**: TCP completado, UDP en desarrollo
