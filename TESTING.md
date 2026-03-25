# Testing Guide - TCP Pub-Sub System

## Quick Start (WSL2)

### Step 1: Clone Repository to WSL2

Open WSL2 terminal and navigate to your project:

```bash
# If your project is in Windows, access it via /mnt/c/
cd /mnt/c/Users/ariad/OneDrive\ -\ Universidad\ de\ los\ andes/2026/2026-1/Redes/Labs/Laboratorio3Redes

# Or copy to home directory for easier access
cp -r TCP ~/tcp-pubsub
cd ~/tcp-pubsub
```

### Step 2: Compile Programs

```bash
# Compile broker
gcc -Wall -Wextra -o broker_tcp broker_tcp.c

# Compile publisher
gcc -Wall -Wextra -o publisher_tcp publisher_tcp.c

# Compile subscriber
gcc -Wall -Wextra -o subscriber_tcp subscriber_tcp.c

# Verify executables exist
ls -lah broker_tcp publisher_tcp subscriber_tcp
```

**Expected Output:**
```
-rwxr-xr-x 1 user group  15K Mar 25 12:00 broker_tcp
-rwxr-xr-x 1 user group  12K Mar 25 12:00 publisher_tcp
-rwxr-xr-x 1 user group  13K Mar 25 12:00 subscriber_tcp
```

---

## Test Case 1: Basic Test (1 Publisher, 1 Subscriber)

### Terminal 1 - Start Broker:
```bash
./broker_tcp
```

**Expected Output:**
```
===== TCP PUB-SUB BROKER =====
Iniciando broker en puerto 9001...
[OK] Socket creado (descriptor: 3)
[OK] Socket vinculado a 127.0.0.1:9001
[OK] Broker en escucha...
Esperando conexiones de publicadores y suscriptores...
```

### Terminal 2 - Start Subscriber:
```bash
./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B
```

**Expected Output:**
```
===== TCP PUB-SUB SUBSCRIBER =====
Suscriptor ID: sub1
Temas a suscribirse: match_A_vs_B
Conectando a broker en 127.0.0.1:9001...

[OK] Socket creado
[OK] Conectado al broker 127.0.0.1:9001

Suscribiendo a temas...

[sub1] Suscrito al tema: match_A_vs_B

========================================
Esperando mensajes (presiona Ctrl+C para salir)...
========================================

```

### Terminal 1 - Broker Output (after subscriber connects):
```
[BROKER] Conexion aceptada de 127.0.0.1:54123
[BROKER] Cliente identificado como SUSCRIPTOR
[BROKER] Suscriptor registrado para tema 'match_A_vs_B'
```

### Terminal 3 - Start Publisher:
```bash
./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B
```

**Expected Output:**
```
===== TCP PUB-SUB PUBLISHER =====
Publicador ID: pub1
Tema: match_A_vs_B
Conectando a broker en 127.0.0.1:9001...

[OK] Socket creado
[OK] Conectado al broker 127.0.0.1:9001

Enviando 15 mensajes...

[pub1] Mensaje 1 enviado: Gol de Equipo A al minuto 5
[pub1] Mensaje 2 enviado: Saque de banda para Equipo A
[pub1] Mensaje 3 enviado: Tarjeta amarilla al numero 7 de Equipo B
[pub1] Mensaje 4 enviado: Cambio: jugador 10 entra por jugador 20 en Equipo A
[pub1] Mensaje 5 enviado: Gol de Equipo B al minuto 23
[pub1] Mensaje 6 enviado: Fuera de juego anulado
[pub1] Mensaje 7 enviado: Tarjeta roja al numero 15 de Equipo A
[pub1] Mensaje 8 enviado: Gol de Equipo A al minuto 45
[pub1] Mensaje 9 enviado: Fin del primer tiempo
[pub1] Mensaje 10 enviado: Inicia el segundo tiempo
[pub1] Mensaje 11 enviado: Gol de Equipo B al minuto 67
[pub1] Mensaje 12 enviado: Cambio: jugador 5 entra por jugador 3 en Equipo B
[pub1] Mensaje 13 enviado: Empate 2-2 en el marcador
[pub1] Mensaje 14 enviado: Ultimos minutos del partido
[pub1] Mensaje 15 enviado: Fin del partido 3-2 para Equipo A

[OK] Todos los 15 mensajes fueron enviados
Desconectando del broker...
[OK] Conexion cerrada
```

### Terminal 1 - Broker Output (after publisher sends messages):
```
[BROKER] Conexion aceptada de 127.0.0.1:54124
[BROKER] Mensaje recibido: PUBLISH|match_A_vs_B|Gol de Equipo A al minuto 5
[BROKER] Cliente identificado como PUBLICADOR
[BROKER] Reenviando mensaje del tema 'match_A_vs_B' a suscriptores
[BROKER] Mensaje enviado al suscriptor en socket 4
[BROKER] Mensaje recibido: PUBLISH|match_A_vs_B|Saque de banda para Equipo A
[BROKER] Reenviando mensaje del tema 'match_A_vs_B' a suscriptores
...
[BROKER] Publicador desconectado (socket 5)
```

### Terminal 2 - Subscriber Receiving Messages:
```
[sub1] [match_A_vs_B] Gol de Equipo A al minuto 5
[sub1] [match_A_vs_B] Saque de banda para Equipo A
[sub1] [match_A_vs_B] Tarjeta amarilla al numero 7 de Equipo B
[sub1] [match_A_vs_B] Cambio: jugador 10 entra por jugador 20 en Equipo A
[sub1] [match_A_vs_B] Gol de Equipo B al minuto 23
[sub1] [match_A_vs_B] Fuera de juego anulado
[sub1] [match_A_vs_B] Tarjeta roja al numero 15 de Equipo A
[sub1] [match_A_vs_B] Gol de Equipo A al minuto 45
[sub1] [match_A_vs_B] Fin del primer tiempo
[sub1] [match_A_vs_B] Inicia el segundo tiempo
[sub1] [match_A_vs_B] Gol de Equipo B al minuto 67
[sub1] [match_A_vs_B] Cambio: jugador 5 entra por jugador 3 en Equipo B
[sub1] [match_A_vs_B] Empate 2-2 en el marcador
[sub1] [match_A_vs_B] Ultimos minutos del partido
[sub1] [match_A_vs_B] Fin del partido 3-2 para Equipo A
```

### Verification Checklist ✅
- [ ] Subscriber receives all 15 messages
- [ ] Messages arrive in correct order
- [ ] Messages are not duplicated
- [ ] Subscriber can press Ctrl+C to exit
- [ ] No error messages appear

---

## Test Case 2: Multiple Subscribers to Same Topic

### Terminal 1 - Broker:
```bash
./broker_tcp
```

### Terminal 2 - Subscriber 1:
```bash
./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B
```

### Terminal 3 - Subscriber 2:
```bash
./subscriber_tcp 127.0.0.1 9001 sub2 match_A_vs_B
```

### Terminal 4 - Publisher:
```bash
./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B
```

### Expected Result:
```
✅ sub1 receives all 15 messages
✅ sub2 receives all 15 messages (same as sub1)
✅ Both receive messages simultaneously
✅ No message is missed by either subscriber
```

### Broker Output:
```
[BROKER] Reenviando mensaje del tema 'match_A_vs_B' a suscriptores
[BROKER] Mensaje enviado al suscriptor en socket 4
[BROKER] Mensaje enviado al suscriptor en socket 5
```

---

## Test Case 3: Multiple Subscribers to Different Topics

### Terminal 1 - Broker:
```bash
./broker_tcp
```

### Terminal 2 - Subscriber 1 (match_A_vs_B):
```bash
./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B
```

### Terminal 3 - Subscriber 2 (match_C_vs_D):
```bash
./subscriber_tcp 127.0.0.1 9001 sub2 match_C_vs_D
```

### Terminal 4 - Publisher 1 (match_A_vs_B):
```bash
./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B
```

### Terminal 5 - Publisher 2 (match_C_vs_D):
```bash
./publisher_tcp 127.0.0.1 9001 pub2 match_C_vs_D
```

### Expected Result:
```
✅ sub1 receives 15 messages from pub1 (match_A_vs_B)
✅ sub2 receives 15 messages from pub2 (match_C_vs_D)
✅ sub1 does NOT receive pub2's messages
✅ sub2 does NOT receive pub1's messages
✅ Messages are correctly filtered by topic
```

### Verification:
```bash
# In sub1 terminal, should see:
[sub1] [match_A_vs_B] ...
[sub1] [match_A_vs_B] ...
# Should NOT see match_C_vs_D messages

# In sub2 terminal, should see:
[sub2] [match_C_vs_D] ...
[sub2] [match_C_vs_D] ...
# Should NOT see match_A_vs_B messages
```

---

## Test Case 4: Multiple Subscribers to Multiple Topics

### Terminal 1 - Broker:
```bash
./broker_tcp
```

### Terminal 2 - Subscriber (both topics):
```bash
./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B match_C_vs_D
```

### Terminal 3 - Publisher 1 (match_A_vs_B):
```bash
./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B
```

### Terminal 4 - Publisher 2 (match_C_vs_D):
```bash
./publisher_tcp 127.0.0.1 9001 pub2 match_C_vs_D
```

### Expected Result:
```
✅ sub1 receives 15 messages from pub1 (match_A_vs_B)
✅ sub1 receives 15 messages from pub2 (match_C_vs_D)
✅ Total: 30 messages received
✅ Messages grouped by topic
```

---

## Automated Test Script

Create a file `test_pubsub.sh`:

```bash
#!/bin/bash

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}=== TCP Pub-Sub System Test ===${NC}\n"

# Check if executables exist
if [ ! -f broker_tcp ] || [ ! -f publisher_tcp ] || [ ! -f subscriber_tcp ]; then
    echo -e "${RED}[ERROR] Executables not found. Please compile first.${NC}"
    echo "Run: gcc -o broker_tcp broker_tcp.c && gcc -o publisher_tcp publisher_tcp.c && gcc -o subscriber_tcp subscriber_tcp.c"
    exit 1
fi

echo -e "${GREEN}[OK] All executables found${NC}\n"

# Start broker in background
echo -e "${YELLOW}Starting broker...${NC}"
./broker_tcp &
BROKER_PID=$!
sleep 2

# Start subscriber
echo -e "${YELLOW}Starting subscriber...${NC}"
timeout 30 ./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B > subscriber_output.txt 2>&1 &
SUB_PID=$!
sleep 2

# Start publisher
echo -e "${YELLOW}Starting publisher...${NC}"
timeout 30 ./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B > publisher_output.txt 2>&1 &
PUB_PID=$!

# Wait for publisher to finish
wait $PUB_PID

# Wait a bit more for subscriber to receive
sleep 2

# Kill subscriber and broker
kill $SUB_PID 2>/dev/null
kill $BROKER_PID 2>/dev/null

# Count received messages
MESSAGES_SENT=$(grep -c "Mensaje" publisher_output.txt || echo 0)
MESSAGES_RECEIVED=$(grep -c "match_A_vs_B" subscriber_output.txt || echo 0)

echo -e "\n${YELLOW}=== Test Results ===${NC}"
echo "Messages sent: $MESSAGES_SENT"
echo "Messages received: $MESSAGES_RECEIVED"

if [ "$MESSAGES_SENT" -eq "$MESSAGES_RECEIVED" ]; then
    echo -e "${GREEN}✅ TEST PASSED: All messages delivered correctly${NC}"
    exit 0
else
    echo -e "${RED}❌ TEST FAILED: Message count mismatch${NC}"
    echo "Publisher output:"
    cat publisher_output.txt
    echo -e "\nSubscriber output:"
    cat subscriber_output.txt
    exit 1
fi
```

Usage:
```bash
chmod +x test_pubsub.sh
./test_pubsub.sh
```

---

## Troubleshooting

### Error: "Address already in use"
```
[ERROR] No se pudo vincular el socket al puerto 9001
```
**Solution**: Wait a few seconds (TIME_WAIT state) or use a different port:
```bash
# Check what's using port 9001
lsof -i :9001

# Kill the process (if needed)
kill -9 <PID>
```

### Error: "Connection refused"
```
[ERROR] No se pudo conectar al broker: Connection refused
```
**Solution**: Make sure broker is running in another terminal first.

### Error: "Invalid IP address"
```
[ERROR] Direccion IP invalida: 192.168.x.x
```
**Solution**: Use 127.0.0.1 for localhost or use correct IP address.

### Subscriber not receiving messages
**Possible causes**:
1. Subscriber not yet connected when publisher sends
2. Topic name mismatch (check spelling)
3. Broker crashed (check Terminal 1 output)

**Debug**: Add more verbose output or use `strace` to monitor syscalls:
```bash
strace -e trace=send,recv ./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B
```

---

## Performance Testing

### Test Message Throughput:

Modify publisher to send 100 messages instead of 15:

```bash
# Using bash to time the execution
time ./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B
```

### Test with Multiple Publishers:

```bash
# Terminal 4
./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B &

# Terminal 5
./publisher_tcp 127.0.0.1 9001 pub2 match_A_vs_B &

# Wait for both to complete
wait
```

---

## Network Capture with Wireshark

To analyze TCP packets:

1. Start Wireshark
2. Filter by: `tcp.port == 9001`
3. Observe 3-way handshake, data transmission, and graceful close
4. Compare with UDP behavior later

---

## Checklist for Lab Submission

- [ ] Compile without errors or warnings
- [ ] Test Case 1 passes (1 publisher, 1 subscriber)
- [ ] Test Case 2 passes (multiple subscribers, same topic)
- [ ] Test Case 3 passes (multiple topics, different subscribers)
- [ ] Test Case 4 passes (subscriber on multiple topics)
- [ ] All 15+ messages delivered correctly
- [ ] Messages arrive in correct order
- [ ] No messages duplicated
- [ ] Graceful shutdown with Ctrl+C
- [ ] Error handling for network failures
- [ ] Documentation complete in README.md

---

**Good luck with testing! 🚀**
