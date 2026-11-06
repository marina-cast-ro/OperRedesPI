#!/bin/bash
# Lo corre la CA: revisa la CSR de un nodo y le firma el certificado por 365 días.
# Uso: ./firmar-certificado.sh <archivo.csr> [carpeta de la CA]        (por defecto ~/ca-koopatroopas)
# Deja el certificado al lado de la CSR, con el mismo nombre y extensión .crt
set -e

EQUIPO=Koopatroopas
DIR_SCRIPTS="$(cd "$(dirname "$0")" && pwd)"

if [ $# -lt 1 ]; then
    echo "Uso: $0 <archivo.csr> [carpeta de la CA]"
    exit 1
fi

CSR=$1
DIR_CA="${2:-$HOME/ca-koopatroopas}"
CERT_CA="$DIR_CA/ca-$EQUIPO.crt"
LLAVE_CA="$DIR_CA/ca-$EQUIPO.key"
CRT="${CSR%.csr}.crt"

# 1. La CSR viene firmada con la llave del nodo. Si no verifica, alguien la alteró en el camino
openssl req -in "$CSR" -noout -verify

# 2. El CN tiene que tener la forma AS<ISLA>-<NUM_EMISOR>, que es lo que el punto B compara con el ROA
CN=$(openssl req -in "$CSR" -noout -subject -nameopt multiline | awk -F' = ' '/commonName/ {print $2}')
if ! [[ "$CN" =~ ^AS[0-9]+-[0-9]+$ ]]; then
    echo "CN inválido: '$CN'. Tiene que ser AS<ISLA>-<NUM_EMISOR>, por ejemplo AS65002-201"
    exit 1
fi

# 3. La CA firma con SHA-256 y le pone las extensiones de nodo-ext.cnf.
#    -CAcreateserial lleva la cuenta de números de serie en ca-Koopatroopas.srl
openssl x509 -req -in "$CSR" -CA "$CERT_CA" -CAkey "$LLAVE_CA" -CAcreateserial \
    -days 365 -sha256 -extfile "$DIR_SCRIPTS/nodo-ext.cnf" -out "$CRT"

echo
openssl verify -CAfile "$CERT_CA" "$CRT"
openssl x509 -in "$CRT" -noout -subject -issuer -dates
