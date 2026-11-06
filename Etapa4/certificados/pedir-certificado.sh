#!/bin/bash
# Lo corre el NODO emisor/receptor: crea su llave y la solicitud de certificado (CSR) para la CA.
# Uso: ./pedir-certificado.sh <ISLA> <NUM_EMISOR> [carpeta]        ejemplo: ./pedir-certificado.sh 65002 201
# Deja nodo-AS<ISLA>-<NUM_EMISOR>.key (secreta, se queda en el nodo)
# y nodo-AS<ISLA>-<NUM_EMISOR>.csr (se le pasa a la CA para que la firme).
set -e

if [ $# -lt 2 ]; then
    echo "Uso: $0 <ISLA> <NUM_EMISOR> [carpeta]"
    exit 1
fi

ISLA=$1
NUM_EMISOR=$2
DIR="${3:-.}"

if ! [[ "$ISLA" =~ ^[0-9]+$ && "$NUM_EMISOR" =~ ^[0-9]+$ ]]; then
    echo "ISLA y NUM_EMISOR tienen que ser enteros"
    exit 1
fi

# El CN AS<ISLA>-<NUM_EMISOR> es el que revisa el punto B contra los campos del ROA
NOMBRE="AS$ISLA-$NUM_EMISOR"
LLAVE="$DIR/nodo-$NOMBRE.key"
CSR="$DIR/nodo-$NOMBRE.csr"

mkdir -p "$DIR"
openssl genpkey -quiet -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$LLAVE"
chmod 600 "$LLAVE"

# La CSR lleva la identidad y la llave pública, firmadas con la llave privada. La privada no va adentro
openssl req -new -key "$LLAVE" -subj "/C=CR/O=Koopatroopas/CN=$NOMBRE" -out "$CSR"

echo "Listo: $CSR"
openssl req -in "$CSR" -noout -subject
