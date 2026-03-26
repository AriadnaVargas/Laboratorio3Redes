# Guía de Ejecución – Sistema TCP Pub-Sub

Esta guía te explica paso a paso cómo compilar y ejecutar el sistema **Publicador-Suscriptor (Pub-Sub)** usando TCP y UDP en WSL.

---

## 1. Archivos del proyecto

###  Implementación (carpeta TCP/)
- `broker_tcp.c` → Servidor central (broker)
- `publisher_tcp.c` → Publicador (envía mensajes)
- `subscriber_tcp.c` → Suscriptor (recibe mensajes)

###  Documentación
- `README.md` → Documentación completa de sockets

---

##  2. Abrir entorno de trabajo

### 1. Abre tu terminal de WSL:

```bash
wsl
```
### 2. Ir a la carpeta del proyecto
```bash
cd ../Laboratorio3Redes/TCP
```

### 3. Compiple
```bash
gcc -Wall -Wextra -o broker_tcp broker_tcp.c
gcc -Wall -Wextra -o publisher_tcp publisher_tcp.c
gcc -Wall -Wextra -o subscriber_tcp subscriber_tcp.c
```

### 4. Debes abrir 5 terminales diferentes en WSL y correr:

| Terminal | Command |
|----------|---------|
| 1 | `./broker_tcp` |
| 2 | `./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B` |
| 3 | `./subscriber_tcp 127.0.0.1 9001 sub2 match_C_vs_D` |
| 4 | `./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B` |
| 5 | `./publisher_tcp 127.0.0.1 9001 pub2 match_C_vs_D` |
