#!/bin/bash
# Script para compilar y levantar el Repositorio de ROAs facilmente
# Uso: ./iniciar_repositorio.sh [puerto] (por defecto 9000)

PUERTO="${1:-9000}"
DIR_BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR_REPO="$DIR_BASE/Etapa4/repositorio"

# Buscar truststore en las ubicaciones del proyecto
if [ -f "$DIR_BASE/Etapa4/certificados/trusted-roots.pem" ]; then
    TRUSTSTORE="$DIR_BASE/Etapa4/certificados/trusted-roots.pem"
elif [ -f "$DIR_BASE/Etapa4/validacionROAs/prueba/truststore/trusted-roots.pem" ]; then
    TRUSTSTORE="$DIR_BASE/Etapa4/validacionROAs/prueba/truststore/trusted-roots.pem"
else
    TRUSTSTORE="$DIR_BASE/Etapa4/certificados/trusted-roots.pem"
fi

echo "=============================================="
echo "    INICIANDO REPOSITORIO DE ROAS (PUERTO $PUERTO)"
echo "=============================================="
echo "[*] Truststore: $TRUSTSTORE"

# 1. Compilar el servidor si no existe el binario
if [ ! -f "$DIR_REPO/build/repoServer" ]; then
    echo "[*] Compilando repoServer..."
    cd "$DIR_REPO" && make
fi

# 2. Verificar que el truststore exista
if [ ! -f "$TRUSTSTORE" ]; then
    echo "[!] No se encontro $TRUSTSTORE, copiando de validacionROAs..."
    mkdir -p "$DIR_BASE/Etapa4/certificados"
    if [ -f "$DIR_BASE/Etapa4/validacionROAs/prueba/truststore/trusted-roots.pem" ]; then
        cp "$DIR_BASE/Etapa4/validacionROAs/prueba/truststore/trusted-roots.pem" "$TRUSTSTORE"
    else
        touch "$TRUSTSTORE"
    fi
fi

# 3. Mostrar las IPs disponibles en esta maquina
echo "[*] IPs disponibles en esta maquina:"
hostname -I 2>/dev/null || ip -4 addr show | grep inet | awk '{print $2}'

echo "----------------------------------------------"
echo "Repositorio escuchando en puerto $PUERTO..."
echo "Presiona Ctrl+C para detener."
echo "----------------------------------------------"

# 4. Ejecutar el servidor (el orden de argumentos en repoServer es: [trusted-roots.pem] [puerto])
cd "$DIR_REPO"
exec ./build/repoServer "$TRUSTSTORE" "$PUERTO"
