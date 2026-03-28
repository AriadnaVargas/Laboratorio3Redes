# Sistema Pub-Sub con Múltiples Partidos Dinámicos

## Descripción del Cambio

El sistema ha sido refactorizado para soportar **suscripción dinámica a múltiples partidos**. En lugar de especificar nombres de temas individuales, los usuarios ahora especifican un número que determina cuántos partidos procesar.

### Cambios Clave

#### **Antes** (Sistema antiguo)
```bash
./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B
./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B
# Nombres de equipos: Barcelona, Real Madrid, etc.
```

#### **Ahora** (Sistema nuevo)
```bash
./subscriber_tcp 127.0.0.1 9001 sub1 3
# Se suscribe automáticamente a: match_1_vs_2, match_3_vs_4, match_5_vs_6

./publisher_tcp 127.0.0.1 9001 pub1 1  # match_1_vs_2 (equipos 1 y 2)
./publisher_tcp 127.0.0.1 9001 pub2 2  # match_3_vs_4 (equipos 3 y 4)
./publisher_tcp 127.0.0.1 9001 pub3 3  # match_5_vs_6 (equipos 5 y 6)
# Nombres de equipos: números simples
```

---

## Mapeo Dinámico de Partidos

### Fórmula Matemática

Para un partido con número `i`:
- **Topic**: `match_{2i-1}_vs_{2i}`
- **Team 1**: `2i - 1`
- **Team 2**: `2i`

### Ejemplos

| Num. | Topic | Equipo 1 | Equipo 2 |
|------|-------|----------|----------|
| 1 | match_1_vs_2 | 1 | 2 |
| 2 | match_3_vs_4 | 3 | 4 |
| 3 | match_5_vs_6 | 5 | 6 |
| 4 | match_7_vs_8 | 7 | 8 |
| i | match_{2i-1}_vs_{2i} | 2i-1 | 2i |

---

## Ejemplos de Uso

### Escenario 1: Subscriber a 2 Partidos

```bash
# Terminal 1: Broker
./broker_tcp

# Terminal 2: Subscriber se suscribe a 2 partidos
./subscriber_tcp 127.0.0.1 9001 sub1 2
# Se suscribe a: match_1_vs_2 y match_3_vs_4

# Terminal 3: Publisher para match_1_vs_2
./publisher_tcp 127.0.0.1 9001 pub1 1

# Terminal 4: Publisher para match_3_vs_4
./publisher_tcp 127.0.0.1 9001 pub2 2
```

**Salida esperada en Terminal 2 (sub1)**:
```
[sub1] [match_1_vs_2] Gol de 1 al minuto 5
[sub1] [match_3_vs_4] Gol de 3 al minuto 5
[sub1] [match_1_vs_2] Saque de banda para 2
[sub1] [match_3_vs_4] Saque de banda para 4
[sub1] [match_1_vs_2] Tarjeta amarilla al numero 7 de 1
[sub1] [match_3_vs_4] Tarjeta amarilla al numero 7 de 3
...
```

---

### Escenario 2: Múltiples Subscribers, Múltiples Partidos

```bash
# Terminal 1: Broker
./broker_tcp

# Terminal 2: Subscriber A se suscribe a 3 partidos
./subscriber_tcp 127.0.0.1 9001 sub_A 3

# Terminal 3: Subscriber B se suscribe a 2 partidos
./subscriber_tcp 127.0.0.1 9001 sub_B 2

# Terminal 4: Publisher para match_1_vs_2
./publisher_tcp 127.0.0.1 9001 pub1 1

# Terminal 5: Publisher para match_3_vs_4
./publisher_tcp 127.0.0.1 9001 pub2 2

# Terminal 6: Publisher para match_5_vs_6
./publisher_tcp 127.0.0.1 9001 pub3 3
```

**Resultado**:
- `sub_A` recibe mensajes de todos los 3 partidos (1, 2, 3)
- `sub_B` recibe mensajes de los primeros 2 partidos (1, 2)
- `sub_A` recibe 15*3 = 45 mensajes totales
- `sub_B` recibe 15*2 = 30 mensajes totales

---

### Escenario 3: Asimetría de Suscripciones

```bash
# Terminal 1: Broker
./broker_tcp

# Terminal 2: Subscriber 1 - 5 partidos
./subscriber_tcp 127.0.0.1 9001 sub1 5

# Terminal 3: Subscriber 2 - 3 partidos
./subscriber_tcp 127.0.0.1 9001 sub2 3

# Terminal 4-7: Publishers
./publisher_tcp 127.0.0.1 9001 pub1 1
./publisher_tcp 127.0.0.1 9001 pub2 2
./publisher_tcp 127.0.0.1 9001 pub3 3
./publisher_tcp 127.0.0.1 9001 pub4 4
./publisher_tcp 127.0.0.1 9001 pub5 5
```

**Resultado**:
- `sub1` recibe: match_1_vs_2, 3_vs_4, 5_vs_6, 7_vs_8, 9_vs_10 (todos los 5)
- `sub2` recibe: match_1_vs_2, 3_vs_4, 5_vs_6 (solo 3 de 5)
- Los partidos 4 y 5 no son vistos por sub2

---

## Limitaciones y Validaciones

### Rango Válido de Partidos

```c
#define MAX_PARTIDOS 20  // Máximo 20 partidos (equipos 1-40)
```

**Validación**:
```bash
# Válido
./subscriber_tcp 127.0.0.1 9001 sub1 1
./subscriber_tcp 127.0.0.1 9001 sub1 20

# INVALID
./subscriber_tcp 127.0.0.1 9001 sub1 0    # Error: debe ser >= 1
./subscriber_tcp 127.0.0.1 9001 sub1 21   # Error: excede MAX_PARTIDOS
./subscriber_tcp 127.0.0.1 9001 sub1 -5   # Error: número negativo
```

---

## Formato de Mensajes

### Mensaje Publicado (del Broker)

```
[match_1_vs_2] Gol de 1 al minuto 5
[match_3_vs_4] Cambio: jugador 10 entra por jugador 20 en 4
[match_5_vs_6] Tarjeta roja al numero 15 de 5
```

**Estructura**:
- `[match_X_vs_Y]` - Identificador del partido
- Mensaje personalizado con número de equipo

### Protocolo Interno (no visible al usuario)

```
PUBLISH|match_1_vs_2|Gol de 1 al minuto 5
SUBSCRIBE|match_1_vs_2
```

---

## Alternancia de Mensajes

Los mensajes se alternan automáticamente entre los equipos:

```
Mensaje 1: "Gol de 1 al minuto 5"          (team1 = 2*1-1 = 1)
Mensaje 2: "Saque de banda para 2"         (team2 = 2*1 = 2)
Mensaje 3: "Tarjeta amarilla al numero 7 de 1"  (team1)
Mensaje 4: "Cambio: ... en 2"              (team2)
...
```

**Cálculo**:
```c
int team = (i % 2 == 0) ? team1 : team2;
```

---

## Arquitectura del Sistema

### Estructura de Datos

#### Subscriber
```c
char topics[MAX_PARTIDOS][100];  // Array de topics generados
int num_partidos;                 // Cantidad de partidos
```

**Generación de topics en subscriber**:
```c
for (int i = 0; i < num_partidos; i++) {
    int team1 = 2 * (i + 1) - 1;  /* 1, 3, 5, ... */
    int team2 = 2 * (i + 1);      /* 2, 4, 6, ... */
    snprintf(topics[i], 100, "match_%d_vs_%d", team1, team2);
}
```

#### Publisher
```c
int num_partido;           // Identificador de partido (1-20)
int team1 = 2*num_partido - 1;
int team2 = 2*num_partido;
char topic[100];           // "match_X_vs_Y" generado dinámicamente
```

### Flujo de Mensajes

```
Publisher 1 (num_partido=1)
  ↓
  topic: "match_1_vs_2"
  Mensaje: "PUBLISH|match_1_vs_2|Gol de 1 al minuto 5"
  ↓
Broker (TCP Multi-threaded)
  ↓
  Routing por topic
  ↓
  ├→ Subscriber A (suscrito a 3 partidos, incluye match_1_vs_2) ✓
  └→ Subscriber B (suscrito a 2 partidos, incluye match_1_vs_2) ✓
```

---

## Compilación

```bash
cd TCP

# Compilar broker con pthread
gcc -Wall -Wextra -pthread -o broker_tcp broker_tcp.c

# Compilar publisher
gcc -Wall -Wextra -o publisher_tcp publisher_tcp.c

# Compilar subscriber
gcc -Wall -Wextra -o subscriber_tcp subscriber_tcp.c
```

---

## Testing

### Test 1: Validación de Entrada

```bash
# Estos deben fallar:
./subscriber_tcp 127.0.0.1 9001 sub1 0
# [ERROR] Número de partidos debe estar entre 1 y 20

./subscriber_tcp 127.0.0.1 9001 sub1 21
# [ERROR] Número de partidos debe estar entre 1 y 20

./subscriber_tcp 127.0.0.1 9001 sub1 abc
# [ERROR] Número de partidos debe estar entre 1 y 20 (atoi retorna 0)
```

### Test 2: Flujo Correcto

```bash
# Terminal 1
./broker_tcp

# Terminal 2
./subscriber_tcp 127.0.0.1 9001 sub1 2

# Terminal 3
./publisher_tcp 127.0.0.1 9001 pub1 1

# Salida esperada (en Terminal 2):
[sub1] [match_1_vs_2] Gol de 1 al minuto 5
[sub1] [match_1_vs_2] Saque de banda para 2
[sub1] [match_1_vs_2] Tarjeta amarilla al numero 7 de 1
...
```

---

## Resumen de Cambios

| Aspecto | Antes | Después |
|---------|-------|---------|
| **Entrada sub** | `sub1 match_A_vs_B` | `sub1 3` |
| **Entrada pub** | `pub1 match_A_vs_B` | `pub1 1` |
| **Topics** | Nombre de liga (ej: match_A_vs_B) | Dinámicos (match_1_vs_2) |
| **Equipos** | Nombres (Barcelona, Real Madrid) | Números (1, 2, 3, 4...) |
| **Suscripciones** | Una por ejecución | N dinámicos |
| **Mensajes** | De 1 partido | De N partidos intercalados |

---

## Ventajas del Nuevo Sistema

✅ **Escalabilidad**: Fácil de agregar/quitar partidos  
✅ **Simplicidad**: Uso de números en lugar de nombres  
✅ **Dinamismo**: Topics generados en tiempo de ejecución  
✅ **Flexibilidad**: Subscribers pueden elegir cuántos partidos monitorear  
✅ **Concurrencia**: Multi-threading permite múltiples partidos simultáneos  
✅ **Intercalado**: Mensajes de partidos diferentes se mezclan naturalmente  

---

## Próximos Pasos

- [ ] Implementar interface gráfica para monitoreo
- [ ] Agregar persistencia de mensajes
- [ ] Implementar UDP para comparación
- [ ] Agregar métricas de performance
