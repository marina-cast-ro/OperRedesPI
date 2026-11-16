#!/bin/bash
# Script de pruebas de seguridad RPKI (4 ataques) para demo en laboratorio o modo local (127.0.0.1).
# Uso:
#   ./red/probar_ataques.sh                      # Menu interactivo (valores de laboratorio)
#   ./red/probar_ataques.sh --local              # Menu interactivo en modo 127.0.0.1
#   ./red/probar_ataques.sh --local [1|2|3|4|all]

set -u

DIR_BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR_HOST="$DIR_BASE/Etapa4/host"
DIR_CERTS="$DIR_BASE/Etapa4/certificados"

MODO="lab"
IP_REPO="${IP_REPO:-172.16.20.10}"
PUERTO_REPO="${PUERTO_REPO:-9000}"
IP_ROUTER="${IP_ROUTER:-172.16.20.1}"
PUERTO_ROUTER="${PUERTO_ROUTER:-5005}"
KEY_KOOPA="${KEY_KOOPA:-}"
CRT_KOOPA="${CRT_KOOPA:-}"
ACCION=""

mostrar_ayuda() {
    cat <<EOF
Uso: $0 [opciones] [1|2|3|4|a|b|c|d|all]

Opciones:
  --local               Usa puertos locales en 127.0.0.1 (repo 19000, router 15002)
  --lab                 Usa valores de laboratorio (repo 172.16.20.10:9000, router 172.16.20.1:5005)
  --repo IP:PUERTO      Sobrescribe IP y puerto del Repositorio ROA
  --router IP:PUERTO    Sobrescribe IP y puerto del Enrutador objetivo
  --key RUTA            Llave privada de Koopatroopas (AS2-201)
  --cert RUTA           Certificado de Koopatroopas (AS2-201)
  -h, --help            Muestra esta ayuda

Ataques:
  1 / a  Prefix Hijacking en plano de enrutamiento (ANNOUNCE|5|5:1)
  2 / b  Suplantacion de origen en PUT (AS2-201 intenta publicar isla 5 / 172.16.50.10)
  3 / c  Certificado de CA falsa (CA autofirmada pirata con CN=AS5-999 -> ERR_PERMISO)
  4 / d  Borrado no autorizado (DELETE sobre ROA 172.16.50.10 autenticado como AS2-201)
  all    Ejecuta los 4 ataques en secuencia
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --local)
            MODO="local"
            IP_REPO="127.0.0.1"
            PUERTO_REPO="19000"
            IP_ROUTER="127.0.0.1"
            PUERTO_ROUTER="15002"
            shift
            ;;
        --lab)
            MODO="lab"
            IP_REPO="172.16.20.10"
            PUERTO_REPO="9000"
            IP_ROUTER="172.16.20.1"
            PUERTO_ROUTER="5005"
            shift
            ;;
        --repo)
            [ $# -lt 2 ] && { echo "[ERROR] Falta argumento para --repo"; exit 1; }
            IP_REPO="${2%%:*}"
            [[ "$2" == *:* ]] && PUERTO_REPO="${2##*:}"
            shift 2
            ;;
        --router)
            [ $# -lt 2 ] && { echo "[ERROR] Falta argumento para --router"; exit 1; }
            IP_ROUTER="${2%%:*}"
            [[ "$2" == *:* ]] && PUERTO_ROUTER="${2##*:}"
            shift 2
            ;;
        --key)
            [ $# -lt 2 ] && { echo "[ERROR] Falta argumento para --key"; exit 1; }
            KEY_KOOPA="$2"
            shift 2
            ;;
        --cert)
            [ $# -lt 2 ] && { echo "[ERROR] Falta argumento para --cert"; exit 1; }
            CRT_KOOPA="$2"
            shift 2
            ;;
        -h|--help)
            mostrar_ayuda
            exit 0
            ;;
        1|a|A|2|b|B|3|c|C|4|d|D|all)
            ACCION="$1"
            shift
            ;;
        *)
            echo "[ERROR] Argumento no reconocido: $1"
            mostrar_ayuda
            exit 1
            ;;
    esac
done

resolver_credenciales_koopa() {
    if [ -n "$KEY_KOOPA" ] && [ -n "$CRT_KOOPA" ] && [ -f "$KEY_KOOPA" ] && [ -f "$CRT_KOOPA" ]; then
        return 0
    fi

    if [ -f "/tmp/demo_local_koopa/certs/nodo-AS2-201.key" ] && [ -f "/tmp/demo_local_koopa/certs/nodo-AS2-201.crt" ]; then
        KEY_KOOPA="/tmp/demo_local_koopa/certs/nodo-AS2-201.key"
        CRT_KOOPA="/tmp/demo_local_koopa/certs/nodo-AS2-201.crt"
        return 0
    fi

    if [ -f "$DIR_CERTS/nodo.key" ] && [ -f "$DIR_CERTS/nodo.crt" ]; then
        KEY_KOOPA="$DIR_CERTS/nodo.key"
        CRT_KOOPA="$DIR_CERTS/nodo.crt"
        return 0
    fi

    if [ -f "$DIR_CERTS/nodo-AS2-201.key" ] && [ -f "$DIR_CERTS/nodo-AS2-201.crt" ]; then
        KEY_KOOPA="$DIR_CERTS/nodo-AS2-201.key"
        CRT_KOOPA="$DIR_CERTS/nodo-AS2-201.crt"
        return 0
    fi

    return 1
}

compilar_host_si_falta() {
    if [ ! -x "$DIR_HOST/host_emisor" ]; then
        echo "[*] Compilando host_emisor en $DIR_HOST..."
        make -C "$DIR_HOST" host_emisor
    fi
}

enviar_tcp_linea() {
    local host=$1 puerto=$2 linea=$3 leer_resp=${4:-1}
    python3 - "$host" "$puerto" "$linea" "$leer_resp" <<'PYEOF'
import socket, sys
host, port, msg, want_reply = sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4])
with socket.create_connection((host, port), timeout=5) as s:
    s.sendall((msg + "\n").encode("utf-8"))
    if want_reply:
        data = b""
        while not data.endswith(b"\n"):
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
        print(data.decode("utf-8", errors="replace").strip())
PYEOF
}

ataque_a_hijacking() {
    local ip_victima_anuncio=${1:-"ANNOUNCE|5|5:1"}
    echo "------------------------------------------------------------"
    echo "[ATAQUE A] Prefix Hijacking en plano de enrutamiento"
    echo "  Objetivo : $IP_ROUTER:$PUERTO_ROUTER"
    echo "  Payload  : $ip_victima_anuncio"
    echo "------------------------------------------------------------"
    if enviar_tcp_linea "$IP_ROUTER" "$PUERTO_ROUTER" "$ip_victima_anuncio" 0; then
        echo "[OK] Anuncio falso enviado a $IP_ROUTER:$PUERTO_ROUTER."
        echo "[INFO] Verifica en la bitacora del enrutador el mensaje: [ROA] Ruta descartada (INVALID)"
        return 0
    else
        echo "[ERROR] No se pudo conectar con el enrutador en $IP_ROUTER:$PUERTO_ROUTER."
        return 1
    fi
}

ataque_b_suplantacion_put() {
    echo "------------------------------------------------------------"
    echo "[ATAQUE B] Suplantacion de origen en PUT"
    echo "  Repo     : $IP_REPO:$PUERTO_REPO"
    echo "  Accion   : AS2-201 intenta publicar ROA para isla 5 (172.16.50.10)"
    echo "------------------------------------------------------------"
    compilar_host_si_falta
    if ! resolver_credenciales_koopa; then
        echo "[ERROR] No se encontraron llave y certificado de Koopatroopas (AS2-201)."
        echo "        Especifica --key y --cert, o levanta primero ./red/simular_casa.sh."
        return 1
    fi
    local salida rc=0
    salida=$("$DIR_HOST/host_emisor" "$IP_REPO" "$PUERTO_REPO" 201 5 172.16.50.10 "$KEY_KOOPA" "$CRT_KOOPA" 3600 2>&1) || rc=$?
    printf '%s\n' "$salida"
    if printf '%s\n' "$salida" | grep -q "ERR_PERMISO"; then
        echo "[VALIDADO] Repositorio rechazo la suplantacion con ERR_PERMISO."
        return 0
    fi
    echo "[ADVERTENCIA] No se observo ERR_PERMISO (rc=$rc)."
    return 1
}

ataque_c_ca_falsa() {
    echo "------------------------------------------------------------"
    echo "[ATAQUE C] Certificado emitido por CA falsa (no confiable)"
    echo "  Repo     : $IP_REPO:$PUERTO_REPO"
    echo "  Accion   : Generar CA pirata en /tmp y enviar HELLO|OWNER|999|<cert_AS5-999>"
    echo "------------------------------------------------------------"
    local tmp_dir
    tmp_dir=$(mktemp -d /tmp/ataque_ca_falsa_XXXXXX)

    openssl genrsa -out "$tmp_dir/pirata-ca.key" 2048 2>/dev/null
    openssl req -x509 -new -nodes -key "$tmp_dir/pirata-ca.key" -sha256 -days 30 \
        -subj "/CN=CA-Pirata" \
        -addext "basicConstraints=critical,CA:TRUE,pathlen:0" \
        -addext "keyUsage=critical,keyCertSign,cRLSign" \
        -out "$tmp_dir/pirata-ca.crt" 2>/dev/null

    openssl genrsa -out "$tmp_dir/falso.key" 2048 2>/dev/null
    openssl req -new -key "$tmp_dir/falso.key" -subj "/CN=AS5-999" \
        -out "$tmp_dir/falso.csr" 2>/dev/null
    printf 'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\n' > "$tmp_dir/nodo.ext"
    openssl x509 -req -in "$tmp_dir/falso.csr" -CA "$tmp_dir/pirata-ca.crt" \
        -CAkey "$tmp_dir/pirata-ca.key" -CAcreateserial -out "$tmp_dir/falso.crt" \
        -days 30 -sha256 -extfile "$tmp_dir/nodo.ext" 2>/dev/null

    local cert_b64 resp=""
    cert_b64=$(openssl x509 -in "$tmp_dir/falso.crt" -outform DER | base64 -w0)
    resp=$(enviar_tcp_linea "$IP_REPO" "$PUERTO_REPO" "HELLO|OWNER|999|$cert_b64" 1) || {
        rm -rf "$tmp_dir"
        echo "[ERROR] No se pudo conectar con el repositorio en $IP_REPO:$PUERTO_REPO."
        return 1
    }
    rm -rf "$tmp_dir"
    echo "Respuesta del repositorio: $resp"
    if [ "$resp" = "ERR_PERMISO" ]; then
        echo "[VALIDADO] Repositorio rechazo el certificado de CA pirata con ERR_PERMISO."
        return 0
    fi
    echo "[ADVERTENCIA] Respuesta inesperada: $resp"
    return 1
}

ataque_d_delete_no_autorizado() {
    local ip_objetivo=${1:-"172.16.50.10"}
    echo "------------------------------------------------------------"
    echo "[ATAQUE D] Borrado no autorizado (DELETE sobre ROA ajeno)"
    echo "  Repo     : $IP_REPO:$PUERTO_REPO"
    echo "  Accion   : Autenticar como AS2-201 y enviar DELETE|$ip_objetivo|<firma>"
    echo "------------------------------------------------------------"
    if ! resolver_credenciales_koopa; then
        echo "[ERROR] No se encontraron llave y certificado de Koopatroopas (AS2-201)."
        echo "        Especifica --key y --cert, o levanta primero ./red/simular_casa.sh."
        return 1
    fi

    local out rc=0
    out=$(python3 - "$IP_REPO" "$PUERTO_REPO" "$ip_objetivo" "$KEY_KOOPA" "$CRT_KOOPA" <<'PYEOF'
import base64, socket, subprocess, sys

host, port, ip_obj, key_path, crt_path = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4], sys.argv[5]
der = subprocess.check_output(["openssl", "x509", "-in", crt_path, "-outform", "DER"])
cert_b64 = base64.b64encode(der).decode("ascii")

with socket.create_connection((host, port), timeout=10) as s:
    f = s.makefile("r", encoding="utf-8", errors="replace")
    s.sendall(f"HELLO|OWNER|201|{cert_b64}\n".encode("utf-8"))
    hello = f.readline().strip()
    print(f"HELLO -> {hello}")
    parts = hello.replace(" ", "|").split("|")
    if len(parts) < 3 or parts[0] != "HELLO_OK":
        sys.exit(2)
    nonce = parts[-1]
    to_sign = f"DELETE|{ip_obj}|{nonce}".encode("utf-8")
    sig = subprocess.check_output(["openssl", "dgst", "-sha256", "-sign", key_path], input=to_sign)
    sig_b64 = base64.b64encode(sig).decode("ascii")
    s.sendall(f"DELETE|{ip_obj}|{sig_b64}\n".encode("utf-8"))
    resp = f.readline().strip()
    print(f"DELETE -> {resp}")
    if resp != "ERR_PERMISO":
        sys.exit(3)
PYEOF
    ) || rc=$?
    printf '%s\n' "$out"
    if [ "$rc" -eq 0 ]; then
        echo "[VALIDADO] Repositorio rechazo el DELETE no autorizado con ERR_PERMISO."
        return 0
    fi
    echo "[ADVERTENCIA] El DELETE no devolvio ERR_PERMISO (rc=$rc; asegurate de que exista un ROA previo para $ip_objetivo)."
    return 1
}

ejecutar_opcion() {
    case "$1" in
        1|a|A) ataque_a_hijacking ;;
        2|b|B) ataque_b_suplantacion_put ;;
        3|c|C) ataque_c_ca_falsa ;;
        4|d|D) ataque_d_delete_no_autorizado ;;
        all)
            local fallos=0
            ataque_a_hijacking || fallos=$((fallos + 1))
            ataque_b_suplantacion_put || fallos=$((fallos + 1))
            ataque_c_ca_falsa || fallos=$((fallos + 1))
            ataque_d_delete_no_autorizado || fallos=$((fallos + 1))
            echo "============================================================"
            echo "Resumen ataques: $((4 - fallos))/4 ejecutados con rechazo esperado."
            [ "$fallos" -eq 0 ]
            ;;
        *)
            echo "[ERROR] Opcion invalida: $1"
            return 1
            ;;
    esac
}

if [ -n "$ACCION" ]; then
    ejecutar_opcion "$ACCION"
    exit $?
fi

echo "=============================================="
echo "   PRUEBAS DE SEGURIDAD RPKI ($MODO)"
echo "=============================================="
read -r -p "Modo local 127.0.0.1 (s) o laboratorio (n)? [$([ "$MODO" = "local" ] && echo S || echo s/N)]: " RESP_MODO
if [[ "${RESP_MODO:-}" =~ ^[sS]$ ]]; then
    MODO="local"
    IP_REPO="127.0.0.1"
    PUERTO_REPO="19000"
    IP_ROUTER="127.0.0.1"
    PUERTO_ROUTER="15002"
fi

read -r -p "IP del Repositorio [$IP_REPO]: " IN_REPO_IP
IP_REPO="${IN_REPO_IP:-$IP_REPO}"
read -r -p "Puerto del Repositorio [$PUERTO_REPO]: " IN_REPO_PORT
PUERTO_REPO="${IN_REPO_PORT:-$PUERTO_REPO}"
read -r -p "IP del Enrutador objetivo [$IP_ROUTER]: " IN_RTR_IP
IP_ROUTER="${IN_RTR_IP:-$IP_ROUTER}"
read -r -p "Puerto del Enrutador objetivo [$PUERTO_ROUTER]: " IN_RTR_PORT
PUERTO_ROUTER="${IN_RTR_PORT:-$PUERTO_ROUTER}"

while true; do
    echo ""
    echo "=============================================="
    echo "  Repo: $IP_REPO:$PUERTO_REPO | Router: $IP_ROUTER:$PUERTO_ROUTER"
    echo "=============================================="
    echo "a / 1) Prefix Hijacking en enrutamiento (ANNOUNCE|5|5:1)"
    echo "b / 2) Suplantacion de origen en PUT (AS2-201 -> isla 5)"
    echo "c / 3) Certificado de CA falsa (CN=AS5-999 -> ERR_PERMISO)"
    echo "d / 4) Borrado no autorizado DELETE sobre isla 5"
    echo "5)     Ejecutar los 4 ataques seguidos (all)"
    echo "6)     Salir"
    echo "=============================================="
    read -r -p "Selecciona una opcion: " OP
    case "${OP:-}" in
        5) ejecutar_opcion all ;;
        6|q|Q|salir) exit 0 ;;
        *) ejecutar_opcion "${OP:-}" ;;
    esac
done
