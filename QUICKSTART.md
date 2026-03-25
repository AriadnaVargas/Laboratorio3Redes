# Quick Start - TCP Pub-Sub Testing

## 5-Minute Setup

### 1. Open WSL2 Terminal
```bash
wsl
```

### 2. Navigate to Project
```bash
cd /mnt/c/Users/ariad/OneDrive\ -\ Universidad\ de\ los\ andes/2026/2026-1/Redes/Labs/Laboratorio3Redes/TCP
```

### 3. Compile
```bash
gcc -Wall -Wextra -o broker_tcp broker_tcp.c
gcc -Wall -Wextra -o publisher_tcp publisher_tcp.c
gcc -Wall -Wextra -o subscriber_tcp subscriber_tcp.c
```

### 4. Open 5 Separate WSL2 Windows

**Window 1 - Broker:**
```bash
cd /path/to/TCP
./broker_tcp
```

**Window 2 - Subscriber 1:**
```bash
cd /path/to/TCP
./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B
```

**Window 3 - Subscriber 2:**
```bash
cd /path/to/TCP
./subscriber_tcp 127.0.0.1 9001 sub2 match_C_vs_D
```

**Window 4 - Publisher 1:**
```bash
cd /path/to/TCP
./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B
```

**Window 5 - Publisher 2:**
```bash
cd /path/to/TCP
./publisher_tcp 127.0.0.1 9001 pub2 match_C_vs_D
```

## Expected Results

✅ **Subscriber 1** receives 15 messages from **Publisher 1** only  
✅ **Subscriber 2** receives 15 messages from **Publisher 2** only  
✅ Messages arrive **in order**  
✅ **No duplicates**  
✅ **No cross-topic messages**

---

## Common Issues

| Problem | Solution |
|---------|----------|
| Address already in use | Wait 30 seconds or use different port |
| Connection refused | Start broker first |
| No messages received | Check topic names match exactly |
| Compile errors | Make sure you're in WSL2 (not Windows CMD) |

---

See `TESTING.md` for detailed test cases and troubleshooting.
See `README.md` for socket API documentation.
