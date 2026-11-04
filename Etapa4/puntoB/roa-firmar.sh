#!/bin/bash
# Firma un ROA con la llave del nodo emisor (punto B). Imprime la línea ROA completa.
# Uso: ./roa-firmar.sh <IP> <ISLA> <NUM_EMISOR> <nodo.key> <nodo.crt> [segundos de validez (def. 86400)]
set -e
[ $# -ge 5 ] || { echo "Uso: $0 <IP> <ISLA> <NUM_EMISOR> <nodo.key> <nodo.crt> [validez]"; exit 1; }
IP=$1; ISLA=$2; NUM=$3; KEY=$4; CRT=$5; VALIDEZ=${6:-86400}

[[ "$IP" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]] || { echo "IP inválida" >&2; exit 1; }
[[ "$ISLA" =~ ^[1-9][0-9]*$ && "$NUM" =~ ^[0-9]+$ ]] || { echo "ISLA/NUM_EMISOR inválidos" >&2; exit 1; }

# El CN del certificado tiene que ser el de este ROA
CN=$(openssl x509 -in "$CRT" -noout -subject -nameopt multiline | awk -F' = ' '/commonName/ {print $2}')
[ "$CN" = "AS$ISLA-$NUM" ] || { echo "El certificado es $CN, no AS$ISLA-$NUM" >&2; exit 1; }

EMISION=$(date -u +%s)
VENCE=$((EMISION + VALIDEZ))
# VENCE no puede pasar del notAfter del certificado
NOTAFTER=$(date -u -d "$(openssl x509 -in "$CRT" -noout -enddate | cut -d= -f2)" +%s)
[ "$VENCE" -le "$NOTAFTER" ] || VENCE=$NOTAFTER
[ "$VENCE" -gt "$EMISION" ] || { echo "El certificado ya venció" >&2; exit 1; }

TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
printf '%s' "ROA|$IP|$ISLA|$NUM|$EMISION|$VENCE" > "$TMP/msg.txt"      # sin \n final
openssl dgst -sha256 -sign "$KEY" -out "$TMP/sig.bin" "$TMP/msg.txt"
printf '%s|%s\n' "$(cat "$TMP/msg.txt")" "$(base64 -w0 "$TMP/sig.bin")"
