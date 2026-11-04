#!/bin/bash
# Casos del punto B. Uso: ./probar.sh <roa_test> <trusted-roots.pem> <cert.b64> <roa.txt> <nodo.key>
# Cada caso dice que veredicto espera. Termina con 0 si todos coinciden.
BIN=$1; ROOTS=$2; CERT=$3; ROA=$4; KEY=$5
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
fallos=0; total=0

# caso "<descripcion>" <esperado> <archivo roa> [hora unix]
#   esperado: OK | RECHAZADO | RECUPERABLE | PARSEO | ERROR (certificado/truststore)
caso() {
    local desc=$1 esperado=$2 archivo=$3 hora=$4 roots=${5:-$ROOTS}
    local salida; salida=$("$BIN" "$roots" "$CERT" "$archivo" $hora 2>&1)
    local veredicto=${salida%%[:$'\n']*}
    total=$((total+1))
    if [ "$veredicto" = "$esperado" ] && ! grep -q "LeakSanitizer\|runtime error" <<< "$salida"; then
        printf "  ok     %-48s %s\n" "$desc" "$salida"
    else
        printf "  FALLA  %-48s esperado %s, salio: %s\n" "$desc" "$esperado" "$salida"; fallos=$((fallos+1))
    fi
}

LINEA=$(cat "$ROA")
IPROA=$(cut -d"|" -f2 "$ROA")
EMISION=$(cut -d'|' -f5 "$ROA"); VENCE=$(cut -d'|' -f6 "$ROA")

echo "Validos"
caso "ROA recien firmado"                    OK         "$ROA"
printf '%s\r\n' "$LINEA" > "$T/crlf";            caso "con \\r\\n al final"  OK "$T/crlf"
python3 -I -c "d=open('$ROA','rb').read().rstrip(b'\n');open('$T/blk','wb').write(d+b'\0'*(512-len(d)))"
caso "bloque de 512 bytes con relleno \\0"   OK         "$T/blk"

echo "Paso 1: formato"
cut -d'|' -f1-6 "$ROA" > "$T/f6";                       caso "6 campos"          PARSEO "$T/f6"
echo "$LINEA|x" > "$T/f8";                              caso "8 campos"          PARSEO "$T/f8"
echo "${LINEA/ROA|/XXX|}" > "$T/tipo";                  caso "primer campo no es ROA" PARSEO "$T/tipo"
echo "${LINEA/$IPROA/300.1.1.1}" > "$T/ip";           caso "IP 300.1.1.1"      PARSEO "$T/ip"
echo "${LINEA%|*}|@@@@" > "$T/b64";                     caso "FIRMA no es base64" PARSEO "$T/b64"

echo "Paso 2: vigencia"
caso "una hora despues de VENCE"             RECHAZADO   "$ROA" $((VENCE+3600))
caso "antes de EMISION"                      RECUPERABLE "$ROA" $((EMISION-3600))
caso "justo en VENCE (aun valido)"           OK          "$ROA" $VENCE

echo "Paso 3: cadena"
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/o.key" -out "$T/o.pem" -days 30 -subj "/CN=Otra" \
    -addext "basicConstraints=critical,CA:TRUE,pathlen:0" -addext "keyUsage=critical,keyCertSign" 2>/dev/null
caso "truststore con otra CA"                RECUPERABLE "$ROA" "" "$T/o.pem"

echo "Paso 4: CN"
echo "${LINEA/|$(cut -d'|' -f3 "$ROA")|/|65004|}" > "$T/isla"; caso "ISLA distinta"      RECHAZADO "$T/isla"
echo "${LINEA/|$(cut -d'|' -f4 "$ROA")|/|18|}" > "$T/num";     caso "NUM_EMISOR distinto" RECHAZADO "$T/num"

echo "Paso 5: VENCE contra notAfter del certificado"
E=$(date +%s); V=$((E+400*86400))
printf '%s' "ROA|$IPROA|$(cut -d'|' -f3 "$ROA")|$(cut -d'|' -f4 "$ROA")|$E|$V" > "$T/m"
openssl dgst -sha256 -sign "$KEY" -out "$T/s" "$T/m"; echo "$(cat "$T/m")|$(base64 -w0 "$T/s")" > "$T/largo"
caso "VENCE 400 dias despues (cert dura 365)" RECHAZADO "$T/largo"

echo "Paso 6: firma"
echo "${LINEA/$IPROA/10.1.0.6}" > "$T/alt";           caso "IP alterada tras firmar"    RECHAZADO "$T/alt"
echo "${LINEA%????????}" > "$T/trunc";                   caso "firma truncada"             RECHAZADO "$T/trunc"
openssl genpkey -quiet -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$T/aj.key"
openssl dgst -sha256 -sign "$T/aj.key" -out "$T/s2" "$T/m"
printf '%s|%s\n' "$(printf '%s' "ROA|$IPROA|$(cut -d'|' -f3 "$ROA")|$(cut -d'|' -f4 "$ROA")|$E|$((E+100))")" "$(base64 -w0 "$T/s2")" > "$T/ajena"
caso "firmado con la llave de otro nodo"     RECHAZADO "$T/ajena"

echo
echo "$((total-fallos)) de $total casos como se esperaba"
[ "$fallos" -eq 0 ]