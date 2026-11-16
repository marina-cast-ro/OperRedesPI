#!/bin/bash
# Script para compilar y levantar el Repositorio de ROAs facilmente
# Uso:
#   ./iniciar_repositorio.sh [puerto]                  (levanta repoServer en el puerto, por defecto 9000)
#   sudo ./iniciar_repositorio.sh --red [interfaz]     (configura IP de PC 2 en VLAN 20 y/o VLAN 99 y levanta repoServer)

PUERTO="9000"
MODO_RED=0
IFACE="eth0"

if [ "${1:-}" = "--red" ]; then
    MODO_RED=1
    IFACE="${2:-eth0}"
    PUERTO="${3:-9000}"
elif [ -n "${1:-}" ]; then
    PUERTO="$1"
fi

DIR_BASE="$(cd "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/.." && pwd)"
DIR_REPO="$DIR_BASE/Etapa4/repositorio"

if [ "$MODO_RED" -eq 1 ]; then
    echo "=============================================="
    echo "  CONFIGURACION DE RED PC 2 (REPOSITORIO ROA)"
    echo "  Interfaz fisica: $IFACE"
    echo "=============================================="
    echo "1) Modelo Local VLAN 20     (172.16.20.10/24 en $IFACE)"
    echo "2) Modelo Global VLAN 99    (172.16.99.20/24 en $IFACE - puerto Fa0/9 en access vlan 99)"
    echo "3) Modelo Dual VLAN 20 + 99 (172.16.20.10 en $IFACE + 172.16.99.20 en ${IFACE}.99)"
    echo "4) No tocar red, solo iniciar repoServer"
    read -p "Selecciona una opcion [1-4]: " OP_RED
    case "$OP_RED" in
        1)
            ip addr flush dev "$IFACE" 2>/dev/null || true
            ip addr add 172.16.20.10/24 dev "$IFACE"
            ip link set dev "$IFACE" up
            ip route replace default via 172.16.20.1 dev "$IFACE" 2>/dev/null || true
            echo "[OK] $IFACE configurada con 172.16.20.10/24 (GW 172.16.20.1)"
            ;;
        2)
            ip addr flush dev "$IFACE" 2>/dev/null || true
            ip addr add 172.16.99.20/24 dev "$IFACE"
            ip link set dev "$IFACE" up
            echo "[OK] $IFACE configurada con 172.16.99.20/24 (VLAN 99)"
            ;;
        3)
            ip addr flush dev "$IFACE" 2>/dev/null || true
            ip addr add 172.16.20.10/24 dev "$IFACE"
            ip link set dev "$IFACE" up
            ip route replace default via 172.16.20.1 dev "$IFACE" 2>/dev/null || true
            ip link add link "$IFACE" name "${IFACE}.99" type vlan id 99 2>/dev/null || true
            ip addr flush dev "${IFACE}.99" 2>/dev/null || true
            ip addr add 172.16.99.20/24 dev "${IFACE}.99"
            ip link set dev "${IFACE}.99" up
            echo "[OK] Modo Dual activo: 172.16.20.10/24 ($IFACE) y 172.16.99.20/24 (${IFACE}.99)"
            ;;
        *)
            echo "[*] Omitiendo configuracion de interfaz."
            ;;
    esac
fi

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
