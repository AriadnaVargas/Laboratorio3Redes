# Guía de Threading en el Broker TCP

## ¿Por qué Threading?

Sin threading, el broker procesaba publishers **secuencialmente** (uno después del otro). Esto significa:

```
Flujo sin Threading:
├─ Publisher 1 conecta
│  └─ Broker espera a que Publisher 1 termine (15 mensajes)
│     └─ Publisher 2 no puede conectar hasta que Publisher 1 se desconecte
│
└─ Publisher 2 finalmente conecta
   └─ Broker espera a que Publisher 2 termine
```

**Resultado**: Subscribers de Publisher 2 solo comienzan a recibir después de que Publisher 1 termina.

### Flujo con Threading:

```
Flujo con Threading:
├─ Publisher 1 conecta → THREAD 1 creado
│  ├─ Envía mensaje 1 (match_A_vs_B)
│  ├─ Subscribers de match_A_vs_B reciben inmediatamente
│  ├─ [Publisher 1 esperando 1 segundo...]
│  └─ Envía mensaje 2
│
├─ Publisher 2 conecta → THREAD 2 creado (mientras Thread 1 espera)
│  ├─ Envía mensaje 1 (match_C_vs_D)
│  ├─ Subscribers de match_C_vs_D reciben inmediatamente
│  └─ [Concurrent con Publisher 1]
│
└─ Ambos publishers envían mensajes SIMULTÁNEAMENTE
```

## Cambios Implementados en broker_tcp.c

### 1. **Estructura para argumentos del thread**

```c
typedef struct {
    int socket;
} publisher_args_t;
```

Cada thread recibe el socket del publisher en esta estructura.

### 2. **Mutex para sincronización**

```c
pthread_mutex_t subscribers_mutex = PTHREAD_MUTEX_INITIALIZER;
```

Protege la lista compartida de subscribers de race conditions.

### 3. **Función handle_publisher como thread**

Cambió de:
```c
void handle_publisher(int publisher_socket) { ... }
```

A:
```c
void *handle_publisher(void *arg) {
    publisher_args_t *args = (publisher_args_t *)arg;
    int publisher_socket = args->socket;
    free(args);
    ...
}
```

### 4. **Creación de thread en main()**

```c
// Detectar si es publicador
if (strncmp(initial_message, "PUBLISH", 7) == 0) {
    
    // Crear thread para manejar este publisher
    pthread_t publisher_thread;
    publisher_args_t *args = malloc(sizeof(publisher_args_t));
    args->socket = client_socket;
    
    pthread_create(&publisher_thread, NULL, handle_publisher, args);
    pthread_detach(publisher_thread);  // Se limpia automáticamente
}
```

### 5. **Protección de datos compartidos con mutex**

```c
void forward_message_to_subscribers(const char *topic, const char *message) {
    pthread_mutex_lock(&subscribers_mutex);      // Adquirir lock
    
    // Sección crítica - acceso seguro a subscribers[]
    for (int i = 0; i < num_subscribers; i++) {
        if (strcmp(subscribers[i].topic, topic) == 0) {
            send(subscribers[i].socket, full_message, strlen(full_message), 0);
        }
    }
    
    pthread_mutex_unlock(&subscribers_mutex);    // Liberar lock
}
```

## Compilación

**Flag importante**: `-pthread`

```bash
# Compilar broker con soporte para threads
gcc -Wall -Wextra -pthread -o broker_tcp broker_tcp.c

# Compilar publisher (sin pthread necesario, pero sin problemas)
gcc -Wall -Wextra -o publisher_tcp publisher_tcp.c

# Compilar subscriber
gcc -Wall -Wextra -o subscriber_tcp subscriber_tcp.c
```

## Testing Concurrencia

### Test 1: Dos publishers simultáneos

```bash
# Terminal 1: Broker
./broker_tcp

# Terminal 2: Subscriber para match_A_vs_B
./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B

# Terminal 3: Subscriber para match_C_vs_D
./subscriber_tcp 127.0.0.1 9001 sub2 match_C_vs_D

# Terminal 4: Publisher 1 (match_A_vs_B) - Inicia simultáneamente con Terminal 5
./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B &

# Terminal 5: Publisher 2 (match_C_vs_D) - Empieza mientras Publisher 1 está en sleep
./publisher_tcp 127.0.0.1 9001 pub2 match_C_vs_D
```

**Resultado esperado**:
- Sub1 ve mensajes de Barcelona vs Real Madrid
- Sub2 ve mensajes de Manchester City vs Liverpool
- **Ambos reciben mensajes intercalados** (no esperan a que uno termine)

### Test 2: Verificar race condition está solucionada

Si quitaras el mutex, verías:
```
[BROKER] [Thread 1] Reenviando mensaje del tema 'match_A_vs_B'
[BROKER] [Thread 2] Reenviando mensaje del tema 'match_C_vs_D'
[ERROR] Socket operations interleaved ← Race condition!
```

Con el mutex, cada operación es atómica:
```
[BROKER] [Thread 1] Reenviando mensaje del tema 'match_A_vs_B'
[BROKER] Mensaje enviado al suscriptor en socket 5
[BROKER] [Thread 2] Reenviando mensaje del tema 'match_C_vs_D'
[BROKER] Mensaje enviado al suscriptor en socket 6
```

## Conceptos de Threading

### Thread (Hilo)

Unidad de ejecución independiente dentro del mismo proceso:
- Comparten memoria (variables globales, heap)
- Tienen stack propio
- Se ejecutan en paralelo en multi-core

### Mutex (Mutual Exclusion Lock)

Mecanismo que asegura exclusión mutua:
```
Thread 1                          Thread 2
  ↓
pthread_mutex_lock()
  ↓
[Entra sección crítica]            ← pthread_mutex_lock() BLOQUEA aquí
  ↓
[Modifica subscribers[]]
  ↓
pthread_mutex_unlock()
  ↓                              [Finalmente entra en sección crítica]
                                    ↓
                                 [Lee subscribers[] con seguridad]
```

### Race Condition

Ocurre cuando múltiples threads acceden datos sin sincronización:

```c
// SIN MUTEX - INCORRECTO:
int count = 0;

void *increment(void *arg) {
    count++;  // Thread 1: Lee 0, escribe 1
    count++;  // Thread 2: Lee 0, escribe 1 ← Debería ser 2!
}
```

```c
// CON MUTEX - CORRECTO:
int count = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *increment(void *arg) {
    pthread_mutex_lock(&lock);
    count++;      // Solo este thread accede
    count++;
    pthread_mutex_unlock(&lock);
}
```

## Performance

### Escalabilidad

- **Sin Threading**: O(n) - tiempo proporcional al número total de publishers
- **Con Threading**: O(1) - cada publisher se procesa independientemente

### CPU Usage

Con 2 publishers enviando simultáneamente:
- **Sin threading**: Espera secuencial (100% CPU, pero lentamente)
- **Con threading**: Paralelismo real (utiliza múltiples cores)

## Debugging de Threading

### Para ver threads activos (en Linux/WSL2):

```bash
# Mientras el broker está corriendo en otra terminal:
ps -eLf | grep broker_tcp
# Verás múltiples filas (una por thread)
```

### Valgrind para detectar race conditions:

```bash
valgrind --tool=helgrind ./broker_tcp
# Detecta accesos sin sincronización
```

## Conclusión

El threading permite que el broker TCP maneje múltiples publishers **concurrentemente**, logrando que subscribers reciban mensajes de forma **inmediata** de todos los publishers activos, sin esperas innecesarias.

Esta es una mejora crucial para sistemas de pub-sub que se espera procesen múltiples publicadores simultáneamente.
