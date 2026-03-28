# Guía de Ejecución – Sistema TCP Pub-Sub Multi-Match

Esta guía explica paso a paso cómo compilar y ejecutar el sistema **Publicador-Suscriptor (Pub-Sub) con Múltiples Partidos** usando TCP en WSL.

---

## 1. Archivos del proyecto

###  Implementación (carpeta TCP/)
- `broker_tcp.c` → Servidor central (broker)
- `publisher_tcp.c` → Publicador (envía mensajes de múltiples equipos)
- `subscriber_tcp.c` → Suscriptor (recibe mensajes de múltiples partidos)

###  Documentación
- `README.md` → Documentación completa de sockets y threading
- `MULTI_MATCH_GUIDE.md` → Guía del sistema multi-partido

---

## 2. Abrir entorno de trabajo

### 1. Abre tu terminal de WSL:

```bash
wsl
```

### 2. Ir a la carpeta del proyecto
```bash
cd ../Laboratorio3Redes/TCP
```

### 3. Compilar (con -pthread para el broker)
```bash
gcc -Wall -Wextra -pthread -o broker_tcp broker_tcp.c
gcc -Wall -Wextra -o publisher_tcp publisher_tcp.c
gcc -Wall -Wextra -o subscriber_tcp subscriber_tcp.c
```

### 4. Abrir 5 terminales diferentes en WSL y ejecutar:

| Terminal | Comando | Descripción |
|----------|---------|-------------|
| 1 | `./broker_tcp` | Broker central (escucha en puerto 9001) |
| 2 | `./subscriber_tcp 127.0.0.1 9001 sub1 2` | Suscriptor a 2 partidos (match_1_vs_2, match_3_vs_4) |
| 3 | `./subscriber_tcp 127.0.0.1 9001 sub2 3` | Suscriptor a 3 partidos (match_1_vs_2, match_3_vs_4, match_5_vs_6) |
| 4 | `./publisher_tcp 127.0.0.1 9001 pub1 1` | Publisher del partido 1 (match_1_vs_2: equipos 1 vs 2) |
| 5 | `./publisher_tcp 127.0.0.1 9001 pub2 2` | Publisher del partido 2 (match_3_vs_4: equipos 3 vs 4) |

---

## 3. Entender los Parámetros

### Para Subscribers
**Formato**: `./subscriber_tcp <IP> <PUERTO> <NOMBRE> <NUM_PARTIDOS>`

- `<IP>`: Dirección del broker (ej: 127.0.0.1)
- `<PUERTO>`: Puerto del broker (ej: 9001)
- `<NOMBRE>`: Nombre del suscriptor (ej: sub1, sub2)
- `<NUM_PARTIDOS>`: Número de partidos a suscribirse (1-20)

**Ejemplo**: `./subscriber_tcp 127.0.0.1 9001 sub1 2`
- Se suscribe a los tópicos: `match_1_vs_2` y `match_3_vs_4`

### Para Publishers
**Formato**: `./publisher_tcp <IP> <PUERTO> <NOMBRE> <NUM_PARTIDO>`

- `<IP>`: Dirección del broker
- `<PUERTO>`: Puerto del broker
- `<NOMBRE>`: Nombre del publicador (ej: pub1, pub2)
- `<NUM_PARTIDO>`: Número del partido a publicar (1-20)

**Ejemplo**: `./publisher_tcp 127.0.0.1 9001 pub1 1`
- Publica al tópico: `match_1_vs_2` con equipos 1 y 2

---

## 4. Mapeo de Partidos y Equipos

| Num. Partido | Tópico | Equipo 1 | Equipo 2 |
|---|---|---|---|
| 1 | `match_1_vs_2` | 1 | 2 |
| 2 | `match_3_vs_4` | 3 | 4 |
| 3 | `match_5_vs_6` | 5 | 6 |
| 4 | `match_7_vs_8` | 7 | 8 |
| N | `match_{2N-1}_vs_{2N}` | 2N-1 | 2N |

**Fórmula**: Para partido `i`:
- Tópico: `match_{2i-1}_vs_{2i}`
- Equipo 1: `2i - 1`
- Equipo 2: `2i`

---

## 5. Salida Esperada

### Terminal 1 (Broker):
```
[Broker] Escuchando en puerto 9001...
[Broker] Nueva conexión de publisher pub1 (127.0.0.1:xxxxx)
[Broker] Nueva conexión de subscriber sub1 (127.0.0.1:xxxxx)
[Broker] sub1 suscrito a: match_1_vs_2
[Broker] sub1 suscrito a: match_3_vs_4
```

### Terminal 2 (Subscriber sub1 - 2 partidos):
```
[Subscriber sub1] Iniciando suscripción a 2 partidos...
[Subscriber sub1] Suscrito a topic: match_1_vs_2
[Subscriber sub1] Suscrito a topic: match_3_vs_4
[Subscriber sub1] Mensaje de match_1_vs_2: Evento gol del equipo 1
[Subscriber sub1] Mensaje de match_3_vs_4: Evento gol del equipo 3
[Subscriber sub1] Mensaje de match_1_vs_2: Evento gol del equipo 2
[Subscriber sub1] Mensaje de match_3_vs_4: Evento gol del equipo 4
```

### Terminal 4 (Publisher pub1 - Partido 1):
```
[Publisher pub1] Conectado al broker
[Publisher pub1] Publicando en: match_1_vs_2
[Publisher pub1] Enviando: "Evento gol del equipo 1"
[Publisher pub1] Enviando: "Evento gol del equipo 2"
[Publisher pub1] Enviando: "Evento gol del equipo 1"
```

---

## 6. Características del Sistema

✅ **Múltiples partidos simultáneos**: Cada suscriptor puede recibir de varios partidos  
✅ **Generación dinámica de tópicos**: No necesita configuración predefinida  
✅ **Alternancia automática de equipos**: Los mensajes alternan entre equipos  
✅ **Threading concurrente**: El broker maneja múltiples publishers simultáneamente  
✅ **Sincronización con mutex**: Protege acceso a lista de suscriptores  
✅ **Intercalación en tiempo real**: Mensajes de diferentes partidos llegan mezclados naturalmente
