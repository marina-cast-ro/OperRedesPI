#!/bin/bash
# Script para compilar y levantar el Repositorio de ROAs facilmente
# Uso: ./iniciar_repositorio.sh [puerto] (por defecto 9000)

PUERTO="${1:-9000}"
DIR_BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR_REPO="$DIR_BASE/Etapa4/repositorio"
TRUSTSTORE="$DIR_BASE/Etapa4/certificados/trusted-roots.pem"

echo "=============================================="
echo "    INICIANDO REPOSITORIO DE ROAS (PUERTO $PUERTO)"
echo "=============================================="

# 1. Compilar el servidor si no existe el binario
if [ ! -f "$DIR_REPO/build/repoServer" ]; then
    echo "[*] Compilando repoServer..."
    cd "$DIR_REPO"
    make
fi

# 2. Verificar o crear truststore minimo si hiciera falta
if [ ! -f "$TRUSTSTORE" ]; then
    echo "[!] No se encontro $TRUSTSTORE, buscando certificados en $DIR_BASE/Etapa4/certificados..."
    mkdir -p "$DIR_BASE/Etapa4/certificados"
    cat "$DIR_BASE/Etapa4/certificados"/*.crt 2>/dev/null > "$TRUSTSTORE" || touch "$TRUSTSTORE"
fi

# 3. Mostrar las IPs disponibles en esta maquina
echo "[*] IPs disponibles en esta maquina:"
hostname -I 2>/dev/null || ip -4 addr show | grep inet | awk '{print $2}'

echo "----------------------------------------------"
echo "Repositorio escuchando en puerto $PUERTO..."
echo "Presiona Ctrl+C para detener."
echo "----------------------------------------------"

# 4. Ejecutar el servidor
cd "$DIR_REPO"
exec ./build/repoServer "$PUERTO" "$TRUSTSTORE"
