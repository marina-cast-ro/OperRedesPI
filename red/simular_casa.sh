#!/bin/bash
# Levanta toda la topologia en 127.0.0.1 dentro de /tmp/demo_local_koopa para validar o demostrar en vivo:
#   - repoServer en 127.0.0.1:19000
#   - 4 enrutadores reales (Islas 1, 2, 4, 5) en TCP 15001, 15002, 15004, 15005
#   - 4 host_receptor locales en TCP 16001, 16002, 16004, 16005
#   - 4 ROAs validos publicados via host_emisor
#
# Uso:
#   ./red/simular_casa.sh --auto-test    # Ejecuta las 4 pruebas integrales, muestra logs y limpia
#   ./red/simular_casa.sh                # Modo interactivo en vivo hasta presionar Ctrl+C / Enter

set -u

DIR_BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEMO_DIR="/tmp/demo_local_koopa"
CERT_DIR="$DEMO_DIR/certs"
LOG_DIR="$DEMO_DIR/logs"

BIN_REPO="$DIR_BASE/Etapa4/repositorio/build/repoServer"
BIN_ROUTER="$DIR_BASE/Etapa4/vector/enrutador"
BIN_EMISOR="$DIR_BASE/Etapa4/host/host_emisor"
BIN_RECEPTOR="$DIR_BASE/Etapa4/host/host_receptor"
BIN_DATOS="$DIR_BASE/Etapa4/host/host_datos"
SCRIPT_ATAQUES="$DIR_BASE/red/probar_ataques.sh"

AUTO_TEST=0
[ "${1:-}" = "--auto-test" ] && AUTO_TEST=1

PIDS=()
PID_R5=""

limpiar() {
    for p in "${PIDS[@]}"; do
        if [ -n "$p" ] && kill -0 "$p" 2>/dev/null; then
            kill -TERM "$p" 2>/dev/null || true
        fi
    done
    local _t=0
    while [ "$_t" -lt 30 ]; do
        local vivo=0
        for p in "${PIDS[@]}"; do
            if [ -n "$p" ] && kill -0 "$p" 2>/dev/null; then
                vivo=1
                break
            fi
        done
        [ "$vivo" -eq 0 ] && break
        sleep 0.1
        _t=$((_t + 1))
    done
    for p in "${PIDS[@]}"; do
        if [ -n "$p" ] && kill -0 "$p" 2>/dev/null; then
            kill -KILL "$p" 2>/dev/null || true
        fi
        wait "$p" 2>/dev/null || true
    done
    if [ "$AUTO_TEST" -eq 1 ]; then
        rm -rf "$DEMO_DIR"
    fi
}
trap limpiar EXIT INT TERM

compilar_todo() {
    echo "[*] Verificando binarios..."
    [ -x "$BIN_REPO" ] || make -C "$DIR_BASE/Etapa4/repositorio"
    [ -x "$BIN_ROUTER" ] || make -C "$DIR_BASE/Etapa4/vector"
    if [ ! -x "$BIN_EMISOR" ] || [ ! -x "$BIN_RECEPTOR" ] || [ ! -x "$BIN_DATOS" ]; then
        make -C "$DIR_BASE/Etapa4/host"
    fi
}

verificar_puertos_libres() {
    python3 - <<'PYEOF'
import socket, sys
puertos = [19000, 15001, 15002, 15004, 15005, 16001, 16002, 16004, 16005]
ocupados = []
for p in puertos:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        if s.connect_ex(("127.0.0.1", p)) == 0:
            ocupados.append(p)
if ocupados:
    print("Puertos ocupados:", ocupados)
    sys.exit(1)
PYEOF
}

generar_pki_demo() {
    mkdir -p "$CERT_DIR" "$LOG_DIR"
    echo "[*] Generando CA de demostracion y certificados para Islas 1, 2, 4 y 5 en $CERT_DIR..."

    openssl genrsa -out "$CERT_DIR/ca-demo.key" 2048 2>/dev/null
    openssl req -x509 -new -nodes -key "$CERT_DIR/ca-demo.key" -sha256 -days 365 \
        -subj "/CN=CA-Demo-RPKI" \
        -addext "basicConstraints=critical,CA:TRUE,pathlen:0" \
        -addext "keyUsage=critical,keyCertSign,cRLSign" \
        -out "$CERT_DIR/ca-demo.crt" 2>/dev/null

    cp "$CERT_DIR/ca-demo.crt" "$CERT_DIR/trusted-roots.pem"
    printf 'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\n' > "$CERT_DIR/nodo-ext.cnf"

    local pair
    for pair in "1:101" "2:201" "4:401" "5:501"; do
        local isla="${pair%%:*}"
        local num="${pair##*:}"
        openssl genrsa -out "$CERT_DIR/nodo-AS${isla}-${num}.key" 2048 2>/dev/null
        openssl req -new -key "$CERT_DIR/nodo-AS${isla}-${num}.key" \
            -subj "/CN=AS${isla}-${num}" \
            -out "$CERT_DIR/nodo-AS${isla}-${num}.csr" 2>/dev/null
        openssl x509 -req -in "$CERT_DIR/nodo-AS${isla}-${num}.csr" \
            -CA "$CERT_DIR/ca-demo.crt" -CAkey "$CERT_DIR/ca-demo.key" -CAcreateserial \
            -out "$CERT_DIR/nodo-AS${isla}-${num}.crt" -days 30 -sha256 \
            -extfile "$CERT_DIR/nodo-ext.cnf" 2>/dev/null
    done
}

escribir_configs() {
    cat > "$DEMO_DIR/config_isla_1.json" <<EOF
{
  "mi_id": 1,
  "nombre": "Pirateados",
  "puerto_tcp": 15001,
  "servidor_local": "127.0.0.1",
  "puerto_local": 16001,
  "intervalo_announce_seg": 2,
  "tolerancia_silencio_seg": 15,
  "repositorio": {
    "ip": "127.0.0.1",
    "puerto": 19000,
    "id": "ENR-1",
    "truststore": "$CERT_DIR/trusted-roots.pem",
    "periodo_s": 2,
    "espera_inicial_max_s": 0
  },
  "vecinos": {
    "2": { "equipo": "Koopatroopas", "ip": "127.0.0.1", "puerto": 15002, "costo": 2 }
  }
}
EOF

    cat > "$DEMO_DIR/config_isla_2.json" <<EOF
{
  "mi_id": 2,
  "nombre": "Koopatroopas",
  "puerto_tcp": 15002,
  "servidor_local": "127.0.0.1",
  "puerto_local": 16002,
  "intervalo_announce_seg": 2,
  "tolerancia_silencio_seg": 15,
  "repositorio": {
    "ip": "127.0.0.1",
    "puerto": 19000,
    "id": "ENR-2",
    "truststore": "$CERT_DIR/trusted-roots.pem",
    "periodo_s": 2,
    "espera_inicial_max_s": 0
  },
  "vecinos": {
    "1": { "equipo": "Pirateados",      "ip": "127.0.0.1", "puerto": 15001, "costo": 2 },
    "4": { "equipo": "4bits",           "ip": "127.0.0.1", "puerto": 15004, "costo": 5 },
    "5": { "equipo": "Pi de Frambuesa", "ip": "127.0.0.1", "puerto": 15005, "costo": 2 }
  }
}
EOF

    cat > "$DEMO_DIR/config_isla_4.json" <<EOF
{
  "mi_id": 4,
  "nombre": "4bits",
  "puerto_tcp": 15004,
  "servidor_local": "127.0.0.1",
  "puerto_local": 16004,
  "intervalo_announce_seg": 2,
  "tolerancia_silencio_seg": 15,
  "repositorio": {
    "ip": "127.0.0.1",
    "puerto": 19000,
    "id": "ENR-4",
    "truststore": "$CERT_DIR/trusted-roots.pem",
    "periodo_s": 2,
    "espera_inicial_max_s": 0
  },
  "vecinos": {
    "2": { "equipo": "Koopatroopas",    "ip": "127.0.0.1", "puerto": 15002, "costo": 5 },
    "5": { "equipo": "Pi de Frambuesa", "ip": "127.0.0.1", "puerto": 15005, "costo": 1 }
  }
}
EOF

    cat > "$DEMO_DIR/config_isla_5.json" <<EOF
{
  "mi_id": 5,
  "nombre": "Pi de Frambuesa",
  "puerto_tcp": 15005,
  "servidor_local": "127.0.0.1",
  "puerto_local": 16005,
  "intervalo_announce_seg": 2,
  "tolerancia_silencio_seg": 15,
  "repositorio": {
    "ip": "127.0.0.1",
    "puerto": 19000,
    "id": "ENR-5",
    "truststore": "$CERT_DIR/trusted-roots.pem",
    "periodo_s": 2,
    "espera_inicial_max_s": 0
  },
  "vecinos": {
    "2": { "equipo": "Koopatroopas", "ip": "127.0.0.1", "puerto": 15002, "costo": 2 },
    "4": { "equipo": "4bits",        "ip": "127.0.0.1", "puerto": 15004, "costo": 1 }
  }
}
EOF
}

esperar_puerto() {
    local puerto=$1
    python3 - "$puerto" <<'PYEOF'
import socket, sys, time
port = int(sys.argv[1])
for _ in range(50):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        if s.connect_ex(("127.0.0.1", port)) == 0:
            sys.exit(0)
    time.sleep(0.1)
sys.exit(1)
PYEOF
}

iniciar_servicios() {
    echo "[*] Levantando repoServer en 127.0.0.1:19000..."
    "$BIN_REPO" "$CERT_DIR/trusted-roots.pem" 19000 > "$LOG_DIR/repo.log" 2>&1 &
    PIDS+=("$!")
    esperar_puerto 19000 || { echo "[ERROR] repoServer no inicio en 19000"; exit 1; }

    echo "[*] Publicando ROAs validos con host_emisor para Islas 1, 2, 4 y 5..."
    local pair
    for pair in "1:101" "2:201" "4:401" "5:501"; do
        local isla="${pair%%:*}"
        local num="${pair##*:}"
        "$BIN_EMISOR" 127.0.0.1 19000 "$num" "$isla" "172.16.${isla}0.10" \
            "$CERT_DIR/nodo-AS${isla}-${num}.key" "$CERT_DIR/nodo-AS${isla}-${num}.crt" 3600 \
            > "$LOG_DIR/emisor_${isla}.log" 2>&1
    done

    echo "[*] Levantando 4 receptores locales (puertos 16001, 16002, 16004, 16005)..."
    local i
    for i in 1 2 4 5; do
        "$BIN_RECEPTOR" "1600${i}" "$LOG_DIR/receptor_${i}_mensajes.txt" > "$LOG_DIR/receptor_${i}.log" 2>&1 &
        PIDS+=("$!")
        esperar_puerto "1600${i}" || { echo "[ERROR] host_receptor $i no inicio"; exit 1; }
    done

    echo "[*] Levantando 4 enrutadores (puertos 15001, 15002, 15004, 15005)..."
    for i in 1 2 4 5; do
        mkdir -p "$DEMO_DIR/run_isla_${i}"
        (cd "$DEMO_DIR/run_isla_${i}" && exec "$BIN_ROUTER" --red "$DEMO_DIR/config_isla_${i}.json") \
            > "$LOG_DIR/enrutador_${i}.log" 2>&1 &
        local rpid=$!
        PIDS+=("$rpid")
        [ "$i" -eq 5 ] && PID_R5="$rpid"
        esperar_puerto "1500${i}" || { echo "[ERROR] enrutador $i no inicio"; exit 1; }
    done
}

correr_auto_test() {
    local ok_1=0 ok_2=0 ok_3=0 ok_4=0

    echo ""
    echo "============================================================"
    echo "1) CONVERGENCIA DE RUTA MAS CORTA MULTI-SALTO Y ROAs"
    echo "============================================================"
    for _ in $(seq 1 40); do
        if grep -q "4 de 4 ROA validos" "$LOG_DIR/enrutador_2.log" && \
           grep -q "isla 4 (4bits): costo 3, por isla 5 (Pi de Frambuesa)" "$LOG_DIR/enrutador_2.log"; then
            ok_1=1
            break
        fi
        sleep 0.25
    done
    grep -E "Tabla de isla 2|-> isla |4 de 4 ROA validos" "$LOG_DIR/enrutador_2.log" | tail -n 12
    [ "$ok_1" -eq 1 ] && echo "[PASS 1] Isla 2 sincronizo 4 ROAs y eligio ruta multi-salto 2->5->4 (costo 3 < 5)." \
                      || { echo "[FAIL 1] No convergio la ruta multi-salto esperada."; exit 1; }

    echo ""
    echo "============================================================"
    echo "2) ENTREGA EXTREMO A EXTREMO DE DATA (Isla 2 -> Isla 4)"
    echo "============================================================"
    "$BIN_DATOS" 127.0.0.1 15002 2 4 "Hola 4bits desde Koopatroopas" 8
    for _ in $(seq 1 20); do
        if [ -f "$LOG_DIR/receptor_4_mensajes.txt" ] && \
           grep -q 'isla_origen=2 datos="Hola 4bits desde Koopatroopas"' "$LOG_DIR/receptor_4_mensajes.txt"; then
            ok_2=1
            break
        fi
        sleep 0.2
    done
    echo "--- Reenvio en Isla 2 y 5 ---"
    grep "\[REENVIO" "$LOG_DIR/enrutador_2.log" "$LOG_DIR/enrutador_5.log" || true
    echo "--- Registro en Isla 4 (output.txt y host_receptor) ---"
    [ -f "$DEMO_DIR/run_isla_4/output.txt" ] && cat "$DEMO_DIR/run_isla_4/output.txt"
    [ -f "$LOG_DIR/receptor_4_mensajes.txt" ] && cat "$LOG_DIR/receptor_4_mensajes.txt"
    [ "$ok_2" -eq 1 ] && echo "[PASS 2] DATA entregado integramente con isla_origen=2." \
                      || { echo "[FAIL 2] DATA no llego al receptor de Isla 4."; exit 1; }

    echo ""
    echo "============================================================"
    echo "3) BLOQUEO DE ATAQUES RPKI (red/probar_ataques.sh --local all)"
    echo "============================================================"
    "$SCRIPT_ATAQUES" --local all
    for _ in $(seq 1 20); do
        if grep -q "\[ROA\] Ruta descartada (INVALID)" "$LOG_DIR/enrutador_2.log"; then
            ok_3=1
            break
        fi
        sleep 0.1
    done
    echo "--- Log RPKI en enrutador Isla 2 ---"
    grep "\[ROA\] Ruta descartada" "$LOG_DIR/enrutador_2.log" || true
    [ "$ok_3" -eq 1 ] && echo "[PASS 3] Prefix Hijacking marcado INVALID y descartado en Isla 2." \
                      || { echo "[FAIL 3] No se registro descarte INVALID en Isla 2."; exit 1; }

    echo ""
    echo "============================================================"
    echo "4) CAIDA CON SIGINT EN ISLA 5, MMU STATS Y RECONVERGENCIA"
    echo "============================================================"
    kill -INT "$PID_R5"
    wait "$PID_R5" 2>/dev/null || true
    for _ in $(seq 1 30); do
        if grep -q "isla 4 (4bits): costo 5, por isla 4 (4bits)" "$LOG_DIR/enrutador_2.log" && \
           grep -q "isla 5 (Pi de Frambuesa): INALCANZABLE" "$LOG_DIR/enrutador_2.log"; then
            ok_4=1
            break
        fi
        sleep 0.2
    done
    echo "--- Salida al cerrar Isla 5 (SIGINT + MMU) ---"
    tail -n 6 "$LOG_DIR/enrutador_5.log"
    echo "--- Reconvergencia en Isla 2 ---"
    tail -n 6 "$LOG_DIR/enrutador_2.log"
    echo "--- Resumen repoServer ---"
    tail -n 8 "$LOG_DIR/repo.log"
    [ "$ok_4" -eq 1 ] && echo "[PASS 4] Isla 5 envio muerte (-1), imprimio MMU e Isla 2 reconvergio a 2->4 (costo 5)." \
                      || { echo "[FAIL 4] No se completo la reconvergencia esperada."; exit 1; }

    echo ""
    echo "TODAS LAS PRUEBAS INTEGRALES EN 127.0.0.1 PASARON EXITOSAMENTE."
}

rm -rf "$DEMO_DIR"
compilar_todo
verificar_puertos_libres || { echo "[ERROR] Libera los puertos ocupados antes de ejecutar."; exit 1; }
generar_pki_demo
escribir_configs
iniciar_servicios

if [ "$AUTO_TEST" -eq 1 ]; then
    correr_auto_test
else
    echo ""
    echo "============================================================"
    echo "  TOPOLOGIA ACTIVA EN VIVO EN 127.0.0.1 ($DEMO_DIR)"
    echo "============================================================"
    echo "  Repositorio ROA : 127.0.0.1:19000"
    echo "  Enrutadores     : 1->15001, 2->15002, 4->15004, 5->15005"
    echo "  Receptores LAN  : 1->16001, 2->16002, 4->16004, 5->16005"
    echo "  Logs            : $LOG_DIR/"
    echo "  Ejemplos:"
    echo "    $BIN_DATOS 127.0.0.1 15002 2 4 \"Hola 4bits\" 8"
    echo "    $SCRIPT_ATAQUES --local all"
    echo "============================================================"
    read -r -p "Presiona ENTER para detener todos los procesos y salir... "
fi
