#!/bin/bash
# Script para operar el Nodo Host (Emisor y Receptor) facilmente
# Uso: ./iniciar_emisor_receptor.sh

DIR_BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR_HOST="$DIR_BASE/Etapa4/host"
DIR_ROUTER="$DIR_BASE/Etapa4/vector"
OUTPUT_FILE="$DIR_ROUTER/output.txt"

# 1. Asegurar certificados y llaves disponibles
mkdir -p "$DIR_BASE/Etapa4/certificados"

# Detectar rutas
if [ -f "$DIR_BASE/Etapa4/certificados/nodo.key" ]; then
    DEFAULT_KEY="$DIR_BASE/Etapa4/certificados/nodo.key"
else
    DEFAULT_KEY=$(find "$DIR_BASE/Etapa4" -name "nodo*.key" 2>/dev/null | head -n 1)
fi

if [ -f "$DIR_BASE/Etapa4/certificados/nodo.crt" ]; then
    DEFAULT_CRT="$DIR_BASE/Etapa4/certificados/nodo.crt"
else
    DEFAULT_CRT=$(find "$DIR_BASE/Etapa4" -name "nodo*.crt" 2>/dev/null | head -n 1)
fi

# 2. Compilar binarios de host si falta alguno
if [ ! -f "$DIR_HOST/host_emisor" ] || [ ! -f "$DIR_HOST/host_receptor" ] || [ ! -f "$DIR_HOST/host_datos" ]; then
    echo "[*] Compilando binarios de host en C..."
    (cd "$DIR_HOST" && make)
fi

echo "=============================================="
echo "      NODO HOST (EMISOR / RECEPTOR)"
echo "=============================================="
echo "  Llave detectada: $DEFAULT_KEY"
echo "  Cert detectado:  $DEFAULT_CRT"
echo "=============================================="
echo "1) Publicar ROA al Repositorio (Emisor)"
echo "2) Enviar mensaje DATA a otra isla (Emisor de datos)"
echo "3) Iniciar Receptor TCP en puerto 5005 (guarda y muestra mensajes)"
echo "4) Publicar ROA y levantar Receptor TCP de una vez"
echo "5) Salir"
echo "=============================================="
read -p "Selecciona una opcion [1-5]: " OPCION

case $OPCION in
    1)
        read -p "IP del Repositorio [172.16.20.10]: " IP_REPO
        IP_REPO="${IP_REPO:-172.16.20.10}"
        read -p "Puerto del Repositorio [9000]: " PUERTO_REPO
        PUERTO_REPO="${PUERTO_REPO:-9000}"
        read -p "Numero de Emisor [201]: " NUM_EMISOR
        NUM_EMISOR="${NUM_EMISOR:-201}"
        read -p "Numero de Isla / AS [2]: " ISLA
        ISLA="${ISLA:-2}"
        read -p "IP que anuncia tu isla [172.16.20.10]: " IP_ANUNCIO
        IP_ANUNCIO="${IP_ANUNCIO:-172.16.20.10}"
        read -p "Republicar cada 60s y renovar si faltan <5min (--loop)? [s/N]: " USAR_LOOP

        read -p "Ruta de llave privada [$DEFAULT_KEY]: " KEY
        KEY="${KEY:-$DEFAULT_KEY}"
        read -p "Ruta de certificado [$DEFAULT_CRT]: " CRT
        CRT="${CRT:-$DEFAULT_CRT}"

        if [ ! -f "$KEY" ] || [ ! -f "$CRT" ]; then
            echo "[ERROR] No se encontro llave ($KEY) o cert ($CRT)."
            exit 1
        fi

        LOOP_FLAG=""
        if [[ "$USAR_LOOP" =~ ^[sS]$ ]]; then
            LOOP_FLAG="--loop"
        fi

        echo "[*] Ejecutando host_emisor hacia $IP_REPO:$PUERTO_REPO..."
        "$DIR_HOST/host_emisor" "$IP_REPO" "$PUERTO_REPO" "$NUM_EMISOR" "$ISLA" "$IP_ANUNCIO" "$KEY" "$CRT" 86400 $LOOP_FLAG
        ;;
    2)
        read -p "IP de tu Router local [172.16.20.1]: " IP_ROUTER
        IP_ROUTER="${IP_ROUTER:-172.16.20.1}"
        read -p "Puerto del Router [5005]: " PUERTO_ROUTER
        PUERTO_ROUTER="${PUERTO_ROUTER:-5005}"
        read -p "Tu isla origen [2]: " ORIGEN
        ORIGEN="${ORIGEN:-2}"
        read -p "Isla destino (ej. 1 para Pirateados, 4 para 4bits, 5 para Pi): " DESTINO
        read -p "Mensaje de texto a enviar: " MENSAJE

        echo "[*] Enviando paquete DATA a $IP_ROUTER:$PUERTO_ROUTER..."
        "$DIR_HOST/host_datos" "$IP_ROUTER" "$PUERTO_ROUTER" "$ORIGEN" "$DESTINO" "$MENSAJE" 8
        ;;
    3)
        read -p "Archivo donde guardar mensajes [$OUTPUT_FILE]: " ARCHIVO_REC
        ARCHIVO_REC="${ARCHIVO_REC:-$OUTPUT_FILE}"
        echo "[*] Iniciando host_receptor en TCP puerto 5005 (guardando en '$ARCHIVO_REC')..."
        "$DIR_HOST/host_receptor" 5005 "$ARCHIVO_REC"
        ;;
    4)
        read -p "IP del Repositorio [172.16.20.10]: " IP_REPO
        IP_REPO="${IP_REPO:-172.16.20.10}"
        KEY="$DEFAULT_KEY"
        CRT="$DEFAULT_CRT"

        if [ ! -f "$KEY" ] || [ ! -f "$CRT" ]; then
            echo "[ERROR] No se encontro llave ($KEY) o cert ($CRT)."
            exit 1
        fi

        echo "[*] 1/2 Publicando ROA en bucle (--loop en segundo plano)..."
        "$DIR_HOST/host_emisor" "$IP_REPO" 9000 201 2 172.16.20.10 "$KEY" "$CRT" 86400 --loop &
        PID_EMISOR=$!
        trap 'kill "$PID_EMISOR" 2>/dev/null || true' EXIT

        echo ""
        echo "[*] 2/2 Iniciando host_receptor en TCP puerto 5005 (guardando en '$OUTPUT_FILE')..."
        "$DIR_HOST/host_receptor" 5005 "$OUTPUT_FILE"
        ;;
    *)
        echo "Saliendo."
        exit 0
        ;;
esac
