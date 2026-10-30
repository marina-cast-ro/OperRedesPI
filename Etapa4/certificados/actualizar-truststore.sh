#!/bin/bash
# Arma el truststore: junta en trusted-roots.pem las CA raíz de los grupos cuya huella coincide con la
# que ese grupo anunció por el chat. Lo corre cada máquina (nodos, enrutadores y repositorio).
#
# Uso: ./actualizar-truststore.sh <carpeta trust-anchors> <fingerprints.txt> [carpeta destino]
#      (destino por defecto: /etc/rpki/truststore, que pide sudo)
#
# trust-anchors/ trae un ca-<Equipo>.crt por grupo.
# fingerprints.txt tiene una línea por grupo, «<Equipo> <huella SHA-256>», copiada a mano del chat:
#     Koopatroopas 3A:91:...:C4
set -e

if [ $# -lt 2 ]; then
    echo "Uso: $0 <carpeta trust-anchors> <fingerprints.txt> [carpeta destino]"
    exit 1
fi

ANCLAS=$1
HUELLAS=$2
DESTINO="${3:-/etc/rpki/truststore}"
TEMPORAL="$DESTINO/trusted-roots.pem.tmp"

# Las huellas se comparan sin los dos puntos y en mayúscula, para que no importe cómo se copiaron
normalizar() {
    tr -d ':' | tr 'a-f' 'A-F'
}

mkdir -p "$DESTINO"
: > "$TEMPORAL"
aceptadas=0

for crt in "$ANCLAS"/ca-*.crt; do
    [ -e "$crt" ] || continue
    equipo=$(basename "$crt" .crt)
    equipo=${equipo#ca-}

    esperada=$(awk -v e="$equipo" '$1 == e {print $2}' "$HUELLAS" | normalizar)
    real=$(openssl x509 -in "$crt" -noout -fingerprint -sha256 2>/dev/null | cut -d= -f2 | normalizar)

    if [ -z "$esperada" ]; then
        echo "RECHAZADA $equipo: no tiene huella en $HUELLAS"
        continue
    fi
    if [ "$real" != "$esperada" ]; then
        echo "RECHAZADA $equipo: la huella no coincide con la del chat"
        continue
    fi

    # Tiene que ser una raíz de verdad: autofirmada, vigente, con CA:TRUE y keyCertSign
    if ! openssl verify -CAfile "$crt" "$crt" > /dev/null 2>&1; then
        echo "RECHAZADA $equipo: no es autofirmada o está vencida"
        continue
    fi
    extensiones=$(openssl x509 -in "$crt" -noout -ext basicConstraints,keyUsage)
    if ! grep -q "CA:TRUE" <<< "$extensiones" || ! grep -q "Certificate Sign" <<< "$extensiones"; then
        echo "RECHAZADA $equipo: le falta CA:TRUE o keyCertSign"
        continue
    fi

    cat "$crt" >> "$TEMPORAL"
    echo "ACEPTADA  $equipo"
    aceptadas=$((aceptadas + 1))
done

# Si no pasó ninguna, se conserva el truststore anterior
if [ "$aceptadas" -eq 0 ]; then
    rm -f "$TEMPORAL"
    echo "Ninguna raíz pasó. No se tocó $DESTINO/trusted-roots.pem"
    exit 1
fi

# mv dentro de la misma carpeta es un rename atómico: ningún proceso lee un archivo a medio escribir
mv -f "$TEMPORAL" "$DESTINO/trusted-roots.pem"
cp "$HUELLAS" "$DESTINO/fingerprints.txt"
echo "$aceptadas raíces en $DESTINO/trusted-roots.pem"
