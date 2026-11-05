#!/bin/bash
# Script para configurar las interfaces de red del Enrutador (Isla 2 - Koopatroopas)
# Uso: sudo ./configurar_enrutador.sh [nombre_interfaz] (por defecto eth0)

set -e

IFACE="${1:-eth0}"
echo "=== Configurando Enrutador en interfaz: $IFACE ==="

# 1. Habilitar modo enrutador en el kernel de Linux
sysctl -w net.ipv4.ip_forward=1 >/dev/null

# 2. Subinterfaz VLAN 20 (Red local hacia Emisor y Repositorio)
ip link add link "$IFACE" name "${IFACE}.20" type vlan id 20 2>/dev/null || true
ip addr flush dev "${IFACE}.20" 2>/dev/null || true
ip addr add 172.16.20.1/24 dev "${IFACE}.20"
ip link set dev "${IFACE}.20" up
echo "[OK] VLAN 20 - LAN Local: 172.16.20.1/24"

# 3. Subinterfaz VLAN 112 (Hacia Isla 1 - Pirateados)
ip link add link "$IFACE" name "${IFACE}.112" type vlan id 112 2>/dev/null || true
ip addr flush dev "${IFACE}.112" 2>/dev/null || true
ip addr add 172.30.12.2/30 dev "${IFACE}.112"
ip link set dev "${IFACE}.112" up
echo "[OK] VLAN 112 - Transito Isla 1: 172.30.12.2/30"

# 4. Subinterfaz VLAN 124 (Hacia Isla 4 - 4bits)
ip link add link "$IFACE" name "${IFACE}.124" type vlan id 124 2>/dev/null || true
ip addr flush dev "${IFACE}.124" 2>/dev/null || true
ip addr add 172.30.24.1/30 dev "${IFACE}.124"
ip link set dev "${IFACE}.124" up
echo "[OK] VLAN 124 - Transito Isla 4: 172.30.24.1/30"

# 5. Subinterfaz VLAN 125 (Hacia Isla 5 - Pi de Frambuesa)
ip link add link "$IFACE" name "${IFACE}.125" type vlan id 125 2>/dev/null || true
ip addr flush dev "${IFACE}.125" 2>/dev/null || true
ip addr add 172.30.25.1/30 dev "${IFACE}.125"
ip link set dev "${IFACE}.125" up
echo "[OK] VLAN 125 - Transito Isla 5: 172.30.25.1/30"

ip link set dev "$IFACE" up
echo "=== Enrutador configurado con exito ==="
