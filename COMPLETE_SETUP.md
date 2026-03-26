# TCP Pub-Sub System - Complete Testing Setup

## 📋 What You Have

**Implementation Files (TCP/):**
- `broker_tcp.c` (12 KB) - Central server
- `publisher_tcp.c` (5.4 KB) - Message sender
- `subscriber_tcp.c` (6.7 KB) - Message receiver

**Documentation Files:**
- `README.md` - Full socket API documentation (9 libraries)
- `TESTING.md` - Detailed test cases (4 scenarios)
- `QUICKSTART.md` - 5-minute setup guide
- `TESTING_VISUAL_GUIDE.txt` - Quick reference
- `PROJECT_SUMMARY.txt` - Project overview

---

## 🚀 Quick Test (5 Minutes)

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

### 4. Open 5 Terminals & Run

| Terminal | Command |
|----------|---------|
| 1 | `./broker_tcp` |
| 2 | `./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B` |
| 3 | `./subscriber_tcp 127.0.0.1 9001 sub2 match_C_vs_D` |
| 4 | `./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B` |
| 5 | `./publisher_tcp 127.0.0.1 9001 pub2 match_C_vs_D` |
