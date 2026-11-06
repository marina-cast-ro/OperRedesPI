#!/bin/bash
# Crea la CA raíz de Koopatroopas (punto A). Se corre UNA sola vez, en la máquina de la CA.
# Uso: ./crear-ca.sh [carpeta de la CA]        (por defecto ~/ca-koopatroopas)
# Deja ahí ca-Koopatroopas.key (secreta, nunca sale de esa carpeta)
# y ca-Koopatroopas.crt (pública, es la que se comparte con los otros grupos).
set -e

EQUIPO=Koopatroopas
DIR_SCRIPTS="$(cd "$(dirname "$0")" && pwd)"
DIR_CA="${1:-$HOME/ca-koopatroopas}"
LLAVE="$DIR_CA/ca-$EQUIPO.key"
CERT="$DIR_CA/ca-$EQUIPO.crt"

# Si ya hay una CA no se pisa: con una llave nueva, todos los certificados ya emitidos dejan de servir
if [ -f "$LLAVE" ]; then
    echo "Ya existe una CA en $DIR_CA. No se reemplaza."
    exit 1
fi

mkdir -p "$DIR_CA"
chmod 700 "$DIR_CA"

# Llave RSA de 2048 bits, que solo puede leer el dueño
openssl genpkey -quiet -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$LLAVE"
chmod 600 "$LLAVE"

# Certificado autofirmado por 5 años, con las extensiones de ca-ext.cnf
openssl req -new -x509 -key "$LLAVE" -config "$DIR_SCRIPTS/ca-ext.cnf" -sha256 -days 1825 -out "$CERT"

echo
openssl verify -CAfile "$CERT" "$CERT"
openssl x509 -in "$CERT" -noout -subject -issuer -dates
echo
echo "Huella para mandar al chat de los grupos:"
openssl x509 -in "$CERT" -noout -fingerprint -sha256
