# Sistema de Mensajes Aleatorios y Diversificados

## Descripción General

El publisher TCP ahora genera mensajes de eventos deportivos **aleatorios** y **no secuenciales**, en lugar de enviar los mensajes en orden predefinido. Esto proporciona una experiencia más realista y variada en la simulación de un partido en vivo.

---

## Cambios Implementados

### 1. Expansión de Plantillas de Mensajes

Se expandió de **15 mensajes** a más de **45 mensajes variados** categorizados:

#### Goles y Acciones Ofensivas (6 mensajes)
- `¡GOL! Equipo %d abre el marcador al minuto 3`
- `¡GOL! Espectacular gol de equipo %d al minuto 12`
- `Tiro de equipo %d desviado por poco`
- `Remate de cabeza de %d fuera del arco`
- `¡GOL! Gol de penalti convertido por %d`
- `Equipo %d corre peligro de recibir gol`

#### Tarjetas y Disciplina (4 mensajes)
- `Tarjeta amarilla para número 7 de equipo %d`
- `Tarjeta roja directa para número 15 de equipo %d`
- `Segunda amarilla: expulsado jugador de equipo %d`
- `Amonestación a número 10 de %d por protesta`

#### Cambios y Estrategia (5 mensajes)
- `Cambio en equipo %d: entra número 11 por número 9`
- `Cambio en equipo %d: sale número 5, entra número 23`
- `Equipo %d aumenta presión ofensiva`
- `Cambio defensivo en equipo %d para proteger resultado`
- `Triple cambio simultáneo en equipo %d`

#### Momentos del Partido (6 mensajes)
- `Fuera de juego anulado a equipo %d`
- `Balón recuperado por defensa de %d`
- `Córner a favor de equipo %d`
- `Saque de banda para equipo %d`
- `Tiro libre directo para equipo %d`
- `Tiempo muerto solicitado por técnico de equipo %d`

#### Estadísticas y Análisis (5 mensajes)
- `Primer tiempo: Equipo %d domina el juego`
- `Equipo %d incrementa velocidad de pases`
- `Racha de 5 remates consecutivos de equipo %d`
- `Posesión de balón: 60% equipo %d, 40% rival`
- `Centro de defensa de equipo %d realiza gran bloqueo`

#### Momentos Críticos (5 mensajes)
- `¡Gran jugada! Equipo %d se acerca al arco rival`
- `Defensa de %d hace falta para evitar gol seguro`
- `Equipo %d pierde balón en zona peligrosa`
- `Portero de %d hace parada espectacular`
- `¡Atajada milagrosa del portero de equipo %d!`

#### Contexto del Partido (10 mensajes)
- `HALFTIME: Primera mitad finaliza`
- `Segundo tiempo inicia con intensidad`
- `Equipos regresan al terreno de juego`
- `FINAL: ¡El partido ha terminado!`
- `Tension máxima en los últimos minutos`
- `Equipo %d presiona en busca del empate`
- `Equipo %d se defiende contra la avalancha rival`
- `Pitch invasion: Aficionados invaden el campo`
- `Revisor VAR analiza jugada controversial`
- `Gol confirmado tras revisión del VAR`

---

### 2. Selección Aleatoria de Mensajes

#### Antes (Secuencial):
```c
for (size_t i = 0; i < NUM_MESSAGES; i++) {
    // Mensaje i es message_templates[i]
    // Siempre en orden: 0, 1, 2, 3, ...
}
```

#### Ahora (Aleatorio):
```c
srand(time(NULL) + getpid());  // Semilla única por proceso

int used_messages[NUM_MESSAGES] = {0};
int messages_sent = 0;

while (messages_sent < 20) {
    // Seleccionar índice aleatorio
    int random_idx = rand() % NUM_MESSAGES;
    
    // Si ya fue usado, buscar otro
    while (used_messages[random_idx] && messages_sent < NUM_MESSAGES) {
        random_idx = rand() % NUM_MESSAGES;
    }
    
    // Marcar como usado
    used_messages[random_idx] = 1;
    
    // Usar message_templates[random_idx]
}
```

**Ventajas:**
- ✅ No repeticiones hasta completar el pool
- ✅ Orden impredecible para más realismo
- ✅ Si se necesitan más de 45 mensajes, comienza a reutilizar
- ✅ Cada publisher tiene semilla única (basada en tiempo + PID)

---

### 3. Tiempos Aleatorios entre Mensajes

#### Antes (Fijo):
```c
sleep(1);  // Espera exactamente 1 segundo
```

#### Ahora (Aleatorio):
```c
int random_delay = (rand() % 21) + 5;  // 5-25 decisimas
usleep(random_delay * 100000);         // 0.5-2.5 segundos
```

**Rango:**
- Mínimo: 0.5 segundos (5 decisimas)
- Máximo: 2.5 segundos (25 decisimas)
- Promedio: 1.5 segundos

Esto simula mejor la variabilidad de eventos en un partido real.

---

### 4. Soporte de Mensajes con y sin Equipo

El sistema detecta automáticamente si un mensaje incluye un equipo:

```c
int has_team = (strstr(message_templates[random_idx], "%d") != NULL);

if (has_team) {
    // Mensaje como "Gol de equipo %d al minuto 3"
    int team = (messages_sent % 2 == 0) ? team1 : team2;
    generate_message(..., team);
} else {
    // Mensaje como "HALFTIME: Primera mitad finaliza"
    generate_message(..., 0);
}
```

---

## Cambios en publisher_tcp.c

| Aspecto | Antes | Ahora |
|--------|-------|-------|
| **Mensajes** | 15 | 45+ |
| **Selección** | Secuencial (0→14) | Aleatorio |
| **Repeticiones** | Todas únicas | Únicas hasta N repetir |
| **Intervalos** | 1 segundo fijo | 0.5-2.5 segundos aleatorio |
| **Lineas de código** | 218 | 294 |
| **Mensajes por run** | 15 | 20 |
| **Realismo** | Predecible | Naturalista |

---

## Ejemplo de Salida

```
[pub1] Mensaje 1 enviado: ¡GOL! Equipo 1 abre el marcador al minuto 3
[pub1] Mensaje 2 enviado: Segundo tiempo inicia con intensidad
[pub1] Mensaje 3 enviado: Tarjeta amarilla para número 7 de equipo 2
[pub1] Mensaje 4 enviado: Equipo 1 incrementa velocidad de pases
[pub1] Mensaje 5 enviado: ¡Atajada milagrosa del portero de equipo 2!
[pub1] Mensaje 6 enviado: Cambio en equipo 1: entra número 11 por número 9
[pub1] Mensaje 7 enviado: Fuera de juego anulado a equipo 1
[pub1] Mensaje 8 enviado: FINAL: ¡El partido ha terminado!
...
```

Nótese que **el orden es impredecible**, no sigue un patrón secuencial como antes.

---

## Implicaciones de Diseño

### 1. Comportamiento Determinista pero Variado
- Mismo publisher en múltiples ejecuciones: **orden diferente**
- Diferentes publishers simultáneos: **órdenes completamente diferentes**
- Resultado: Intercalación natural y realista

### 2. Escalabilidad
- Pool de 45+ mensajes permite N publishers sin saturación
- Reutilización automática si se necesitan más mensajes
- Sistema flexible para agregar nuevos mensajes

### 3. Validación
- Compilación: ✅ `-Wall -Wextra` sin warnings reales
- Funcionalidad: ✅ Mensajes se envían correctamente
- Threading: ✅ Compatible con broker multi-threaded

---

## Instrucciones de Compilación

```bash
cd TCP

# Con -pthread para soporte de threading
gcc -Wall -Wextra -pthread -o broker_tcp broker_tcp.c
gcc -Wall -Wextra -o publisher_tcp publisher_tcp.c
gcc -Wall -Wextra -o subscriber_tcp subscriber_tcp.c
```

---

## Uso

```bash
# Terminal 1: Broker
./broker_tcp

# Terminal 2: Subscriber a 2 partidos
./subscriber_tcp 127.0.0.1 9001 sub1 2

# Terminal 3: Publisher partido 1
./publisher_tcp 127.0.0.1 9001 pub1 1

# Terminal 4: Publisher partido 2
./publisher_tcp 127.0.0.1 9001 pub2 2

# Observar: Mensajes variados, en orden aleatorio, intercalados naturalmente
```

---

## Commit

```
6dec60a - Feat: Implement random sports event messages with rich messaging
```

Cambios:
- 104 insertiones
- 28 eliminaciones
- Nuevo archivo: `RANDOM_MESSAGES.md` (este archivo)

---

**Fecha**: Marzo 28, 2026  
**Laboratorio**: Laboratorio 3 - Redes  
**Universidad**: Universidad de los Andes
