#!/bin/bash
# Script de compilación para TCP Pub-Sub con threads

echo "===== COMPILANDO TCP PUB-SUB SISTEMA ====="
echo ""

# Compilar broker con soporte para pthreads
echo "[1/3] Compilando broker_tcp.c con pthread support..."
gcc -Wall -Wextra -pthread -o broker_tcp broker_tcp.c
if [ $? -eq 0 ]; then
    echo "✓ broker_tcp compilado exitosamente"
else
    echo "✗ Error al compilar broker_tcp"
    exit 1
fi
echo ""

# Compilar publisher
echo "[2/3] Compilando publisher_tcp.c..."
gcc -Wall -Wextra -o publisher_tcp publisher_tcp.c
if [ $? -eq 0 ]; then
    echo "✓ publisher_tcp compilado exitosamente"
else
    echo "✗ Error al compilar publisher_tcp"
    exit 1
fi
echo ""

# Compilar subscriber
echo "[3/3] Compilando subscriber_tcp.c..."
gcc -Wall -Wextra -o subscriber_tcp subscriber_tcp.c
if [ $? -eq 0 ]; then
    echo "✓ subscriber_tcp compilado exitosamente"
else
    echo "✗ Error al compilar subscriber_tcp"
    exit 1
fi
echo ""

echo "===== COMPILACIÓN COMPLETADA ====="
echo ""
echo "Archivos ejecutables creados:"
ls -lh broker_tcp publisher_tcp subscriber_tcp
echo ""
echo "Para ejecutar:"
echo "  Terminal 1: ./broker_tcp"
echo "  Terminal 2: ./subscriber_tcp 127.0.0.1 9001 sub1 match_A_vs_B"
echo "  Terminal 3: ./publisher_tcp 127.0.0.1 9001 pub1 match_A_vs_B"
