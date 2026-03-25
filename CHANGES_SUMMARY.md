# Resumen de Cambios - Sistema Pub-Sub TCP Multi-threaded

## 🎯 Problema Identificado

Cuando ejecutabas dos publishers simultáneamente:
```
Publisher 1 (match_A_vs_B) --\
                              --> BROKER --> Subscribers
Publisher 2 (match_C_vs_D) --/

PROBLEMA: El broker procesaba publishers SECUENCIALMENTE
- Publisher 1 enviaba 15 mensajes (15 segundos)
- Broker esperaba a que Publisher 1 terminara
- Solo entonces Publisher 2 podía enviar sus mensajes
```

## ✅ Solución Implementada

### Threading POSIX (pthread)

```c
// Cada publisher en su propio thread:
Publisher 1 (match_A_vs_B) ──┐
                              ├─ THREAD 1 ─┐
                              │             │
Publisher 2 (match_C_vs_D) ──┤             ├──> BROKER ──> Subscribers
                              │ THREAD 2 ─┤
                              └─┘
                        (Concurrentes)
```

**Resultado**: Ambos publishers envían mensajes SIMULTÁNEAMENTE

### Cambios Principales

#### 1️⃣ **Publisher TCP - Mensajes Consistentes**

**Archivo**: `TCP/publisher_tcp.c`

**Cambio**:
```c
// ANTES:
const char *sports_messages[] = {
    "Gol de Equipo A al minuto 5",      // ❌ Genérico, confuso
    "Gol de Equipo B al minuto 23",
    ...
}

// DESPUÉS:
struct MatchContext match_contexts[] = {
    {"Barcelona", "Real Madrid"},        // match_A_vs_B
    {"Manchester City", "Liverpool"},    // match_C_vs_D
    {"PSG", "Bayern Munich"},            // match_E_vs_F
    {"Juventus", "AC Milan"},            // match_G_vs_H
}

// Mensajes personalizados:
const char *message_templates[] = {
    "Gol de %s al minuto 5",             // ✅ Dinámicos
    "Saque de banda para %s",
    ...
}
```

**Beneficio**:
- ✅ Sub1 que se suscribe a `match_A_vs_B` ve: "Gol de Barcelona al minuto 5"
- ✅ Sub2 que se suscribe a `match_C_vs_D` ve: "Gol de Manchester City al minuto 5"
- ✅ Mensajes contextualizados y lógicos

#### 2️⃣ **Broker TCP - Multi-threaded**

**Archivo**: `TCP/broker_tcp.c`

**Cambios**:

##### a) Estructura para pasar argumentos al thread:
```c
typedef struct {
    int socket;
} publisher_args_t;
```

##### b) Mutex para sincronización:
```c
pthread_mutex_t subscribers_mutex = PTHREAD_MUTEX_INITIALIZER;
```

Protege la lista compartida `subscribers[]` de race conditions.

##### c) Función handle_publisher como thread:
```c
// ANTES:
void handle_publisher(int publisher_socket) {
    // Procesa TODO secuencialmente
    while (1) {
        recv(publisher_socket, ...);
        // Bloquea aquí hasta que termina
    }
}

// DESPUÉS:
void *handle_publisher(void *arg) {
    // Se ejecuta en su propio thread
    // Otros publishers pueden ejecutarse simultáneamente
    publisher_args_t *args = (publisher_args_t *)arg;
    int publisher_socket = args->socket;
    free(args);
    
    while (1) {
        recv(publisher_socket, ...);
        // Otro thread puede estar aquí también
    }
}
```

##### d) Creación de thread en el loop principal:
```c
if (strncmp(initial_message, "PUBLISH", 7) == 0) {
    pthread_t publisher_thread;
    publisher_args_t *args = malloc(sizeof(publisher_args_t));
    args->socket = client_socket;
    
    // CREAR UN NUEVO THREAD PARA ESTE PUBLISHER
    pthread_create(&publisher_thread, NULL, handle_publisher, args);
    
    // Permitir que el thread se limpie automáticamente
    pthread_detach(publisher_thread);
}
```

##### e) Protección con mutex:
```c
void forward_message_to_subscribers(const char *topic, const char *message) {
    pthread_mutex_lock(&subscribers_mutex);      // 🔒 LOCK
    
    // Sección crítica - acceso seguro
    for (int i = 0; i < num_subscribers; i++) {
        if (strcmp(subscribers[i].topic, topic) == 0) {
            send(subscribers[i].socket, full_message, strlen(full_message), 0);
        }
    }
    
    pthread_mutex_unlock(&subscribers_mutex);    // 🔓 UNLOCK
}
```

## 📊 Comparación: Antes vs Después

### Arquitectura

```
ANTES (Sin Threading):
┌──────────────────────────────────────────────────┐
│                    BROKER                        │
│  ┌────────────────────────────────────────────┐  │
│  │ Loop Principal                             │  │
│  ├─ Accept Publisher 1 ──────────────────┐   │  │
│  │                                        │   │  │
│  │ handle_publisher(sock1) ◄─────────────┘   │  │
│  │   recv() ◄─ Message 1                     │  │
│  │   send() ──► Subscribers                  │  │
│  │   sleep(1) ┐ BLOQUEADO AQUÍ               │  │
│  │            │ 1 segundo                    │  │
│  │            └─────────────────────────────┐ │  │
│  │   recv() ◄─ Message 2                    │ │  │
│  │   ... (total 15 mensajes = 15 segundos) │ │  │
│  │   close()                                │ │  │
│  │                                          │ │  │
│  ├─ Ahora Accept Publisher 2 ◄──────────────┘ │  │
│  │ (Publisher 2 estaba esperando todo este   │  │
│  │  tiempo sin poder conectar)               │  │
│  └────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────┘

TIMELINE: ~15+ segundos (secuencial)


DESPUÉS (Con Threading):
┌──────────────────────────────────────────────────┐
│                    BROKER                        │
│  ┌────────────────────────────────────────────┐  │
│  │ Loop Principal (Thread Principal)          │  │
│  ├─ Accept Publisher 1 ──────┐               │  │
│  │                            │               │  │
│  │ pthread_create() ┐         │               │  │
│  │ Thread 1 creado │         │               │  │
│  │                 │         │               │  │
│  │ Accept Publisher 2 ──┐    │               │  │
│  │                      │    │               │  │
│  │ pthread_create() ┐   │    │               │  │
│  │ Thread 2 creado │   │    │               │  │
│  │                 │   │    │               │  │
│  │ [Ambos threads se ejecutan CONCURRENTEMENTE]│  │
│  │                 │   │    │               │  │
│  │           ┌─────┴──┬┴────┴──┐            │  │
│  │           ▼        ▼        ▼            │  │
│  │ ┌─────────────────┬────────────────┐     │  │
│  │ │ THREAD 1        │ THREAD 2       │     │  │
│  │ │ handle_pub(s1)  │ handle_pub(s2) │     │  │
│  │ │ recv()-msg1     │ recv()-msg1    │     │  │
│  │ │ send()          │ send()         │     │  │
│  │ │ sleep(1)        │ sleep(1)       │     │  │
│  │ │   [T1 esperando]│   [T2 esperando]    │  │
│  │ │ recv()-msg2     │ recv()-msg2    │     │  │
│  │ │ send()          │ send()         │     │  │
│  │ │ ...             │ ...            │     │  │
│  │ └─────────────────┴────────────────┘     │  │
│  └────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────┘

TIMELINE: ~15 segundos (paralelo - ambos envían juntos!)
```

### Tiempo de Ejecución

| Escenario | Antes | Después | Mejora |
|-----------|-------|---------|--------|
| 1 Publisher | 15s | 15s | - (igual) |
| 2 Publishers | 30s | 15s | ⚡ 2x más rápido |
| 3 Publishers | 45s | 15s | ⚡ 3x más rápido |
| N Publishers | 15*N | 15s | ⚡ N x más rápido |

### Mensajes Recibidos

| Subscriber | Antes | Después |
|------------|-------|---------|
| Sub A (match_A_vs_B) | Espera 15s, luego recibe | Recibe inmediatamente |
| Sub B (match_C_vs_D) | Espera 30s, luego recibe | Recibe inmediatamente |

## 🔧 Compilación

**Importante**: Usar flag `-pthread` para el broker

```bash
# Broker (REQUIERE pthread)
gcc -Wall -Wextra -pthread -o broker_tcp broker_tcp.c

# Publishers y Subscribers (opcional -pthread, pero no daña)
gcc -Wall -Wextra -o publisher_tcp publisher_tcp.c
gcc -Wall -Wextra -o subscriber_tcp subscriber_tcp.c
```

Script de compilación incluido: `TCP/compile.sh`

## 📝 Documentación Añadida

### 1. **README.md** - Actualizado
- ✅ Sección completa sobre "Concurrencia y Sincronización (POSIX Threads)"
- ✅ Explicación de pthread_create, pthread_mutex_lock/unlock, pthread_detach
- ✅ Ejemplos de race conditions y cómo se evitan
- ✅ Tabla de contenidos actualizada

### 2. **THREADING_GUIDE.md** - Nuevo
- ✅ Guía completa sobre threading
- ✅ Explicación del problema y la solución
- ✅ Todos los cambios implementados
- ✅ Ejemplos de testing
- ✅ Conceptos de threading (threads, mutex, race conditions)
- ✅ Tips de debugging y performance

### 3. **TCP/compile.sh** - Nuevo
- ✅ Script de compilación automático
- ✅ Valida compilación exitosa
- ✅ Muestra comandos para ejecutar

## 🚀 Cómo Probar

### Paso 1: Compilar
```bash
cd TCP
./compile.sh
```

### Paso 2: Ejecutar (5 terminales)

**Terminal 1 - Broker:**
```bash
./broker_tcp
```

**Terminal 2 - Subscriber A:**
```bash
./subscriber_tcp 127.0.0.1 9001 sub_A match_A_vs_B
```

**Terminal 3 - Subscriber B:**
```bash
./subscriber_tcp 127.0.0.1 9001 sub_B match_C_vs_D
```

**Terminal 4 - Publisher A:** (Iniciar simultáneamente con Terminal 5)
```bash
./publisher_tcp 127.0.0.1 9001 pub_A match_A_vs_B
```

**Terminal 5 - Publisher B:** (Mientras Publisher A está en sleep)
```bash
./publisher_tcp 127.0.0.1 9001 pub_B match_C_vs_D
```

### Resultado Esperado

- Sub_A recibe: "Gol de Barcelona", "Saque de banda para Real Madrid", etc.
- Sub_B recibe: "Gol de Manchester City", "Saque de banda para Liverpool", etc.
- **Ambos reciben mensajes intercalados** (no esperan a que uno termine)
- Los logs del broker muestran `[Thread XXX]` para cada publisher

## 📈 Ventajas Logradas

1. ✅ **Concurrencia**: Múltiples publishers se ejecutan en paralelo
2. ✅ **Sin bloqueos**: Los subscribers no esperan a que termine un publisher
3. ✅ **Thread-safe**: Mutex protege datos compartidos
4. ✅ **Escalable**: Funciona con N publishers (solo limitado por recursos)
5. ✅ **Mensajes consistentes**: Cada topic tiene equipos específicos
6. ✅ **Sin warnings**: Código compilable con `-Wall -Wextra`

## 📚 Git Commits

```
8d20e98 - Feat: Implement multi-threaded broker with concurrent publisher handling
          (Main commit - threading + documentación)
```

## 🎓 Conceptos Aprendidos

- **Threading POSIX (pthread)**: `pthread_create()`, `pthread_join()`, `pthread_detach()`
- **Sincronización**: `pthread_mutex_t`, `pthread_mutex_lock()`, `pthread_mutex_unlock()`
- **Race Conditions**: Problemas cuando múltiples threads acceden datos sin sincronización
- **Mutex**: Mecanismo para garantizar exclusión mutua (Mutual Exclusion)
- **Compilación**: Flag `-pthread` necesario para enlazar librería pthread

## 🔍 Próximos Pasos (Futuro)

- [ ] UDP implementation (Phase 2)
- [ ] Comparación TCP vs UDP con Wireshark
- [ ] Performance testing
- [ ] Connection pooling para optimizar recursos
- [ ] Graceful shutdown con signal handlers

---

**Estado**: ✅ **COMPLETADO**

El broker TCP ahora maneja múltiples publishers concurrentemente de forma segura, permitiendo que los subscribers reciban mensajes de todos ellos sin esperas innecesarias.
