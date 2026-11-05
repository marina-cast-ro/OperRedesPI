#!/bin/bash
# Script para operar el Nodo Host (Emisor y Receptor) facilmente
# Uso: ./iniciar_emisor_receptor.sh

DIR_BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR_HOST="$DIR_BASE/Etapa4/host"
DIR_ROUTER="$DIR_BASE/Etapa4/vector"
OUTPUT_FILE="$DIR_ROUTER/output.txt"

# 1. Compilar host_emisor si no esta compilado
if [ ! -f "$DIR_HOST/host_emisor" ]; then
    echo "[*] Compilando host_emisor en C..."
    cd "$DIR_HOST" && make
fi

echo "=============================================="
echo "      NODO HOST (EMISOR / RECEPTOR)"
echo "=============================================="
echo "1) Publicar ROA al Repositorio (Emisor)"
echo "2) Enviar mensaje DATA a otra isla (Emisor de datos)"
echo "3) Ver mensajes recibidos en vivo (Receptor - output.txt)"
echo "4) Publicar ROA y monitorear Receptor de una vez"
echo "5) Salir"
echo "=============================================="
read -p "Selecciona una opcion [1-5]: " OPCION

case $OPCION in
    1)
        read -p "IP del Repositorio [172.16.20.10]: " IP_REPO
        IP_REPO="${IP_REPO:-172.16.20.10}"
        read -p "Puerto del Repositorio [9000]: " PUERTO_REPO
        PUERTO_REPO="${PUERTO_REPO:-9000}"
        read -p "Numero de Emisor [17]: " NUM_EMISOR
        NUM_EMISOR="${NUM_EMISOR:-17}"
        read -p "Numero de Isla [1]: " ISLA
        ISLA="${ISLA:-1}"
        read -p "IP que anuncia tu isla [10.0.1.1]: " IP_ANUNCIO
        IP_ANUNCIO="${IP_ANUNCIO:-10.0.1.1}"

        KEY="$DIR_BASE/Etapa4/certificados/nodo.key"
        CRT="$DIR_BASE/Etapa4/certificados/nodo.crt"

        echo "[*] Ejecutando host_emisor..."
        "$DIR_HOST/host_emisor" "$IP_REPO" "$PUERTO_REPO" "$NUM_EMISOR" "$ISLA" "$IP_ANUNCIO" "$KEY" "$CRT" 86400
        ;;
    2)
        read -p "IP de tu Router local [127.0.0.1]: " IP_ROUTER
        IP_ROUTER="${IP_ROUTER:-127.0.0.1}"
        read -p "Puerto del Router [5005]: " PUERTO_ROUTER
        PUERTO_ROUTER="${PUERTO_ROUTER:-5005}"
        read -p "Tu isla origen [1]: " ORIGEN
        ORIGEN="${ORIGEN:-1}"
        read -p "Isla destino (ej. 2 para SHELL, 4 para Pirateados): " DESTINO
        read -p "Mensaje de texto a enviar: " MENSAJE

        PAQUETE="DATA|${ORIGEN}|${DESTINO}|8|${MENSAJE}"
        echo "[*] Enviando paquete a tu router: $PAQUETE"
        echo "$PAQUETE" | nc "$IP_ROUTER" "$PUERTO_ROUTER"
        echo "[OK] Paquete enviado al router para su enrutamiento."
        ;;
    3)
        echo "[*] Monitoreando mensajes recibidos en '$OUTPUT_FILE'..."
        touch "$OUTPUT_FILE"
        echo "(Presiona Ctrl+C para salir del visor)"
        echo "----------------------------------------------"
        tail -f "$OUTPUT_FILE"
        ;;
    4)
        read -p "IP del Repositorio [172.16.20.10]: " IP_REPO
        IP_REPO="${IP_REPO:-172.16.20.10}"
        KEY="$DIR_BASE/Etapa4/certificados/nodo.key"
        CRT="$DIR_BASE/Etapa4/certificados/nodo.crt"
        echo "[*] 1/2 Publicando ROA..."
        "$DIR_HOST/host_emisor" "$IP_REPO" 9000 17 1 10.0.1.1 "$KEY" "$CRT" 86400

        echo ""
        echo "[*] 2/2 Monitoreando receptor en '$OUTPUT_FILE'..."
        touch "$OUTPUT_FILE"
        tail -f "$OUTPUT_FILE"
        ;;
    *)
        echo "Saliendo."
        exit 0
        ;;
esac
