#ifndef CERTIFICATES_H
#define CERTIFICATES_H

// Punto A de la Etapa 4: certificados X.509 de los nodos y su validación contra las CA raíz.
// Usa la biblioteca de OpenSSL (libcrypto): en Ubuntu, sudo apt install libssl-dev, y enlazar con -lcrypto

#include <stddef.h>
#include <time.h>
#include <openssl/x509.h>

// Carga trusted-roots.pem, el archivo con las CA raíz de los grupos que arma actualizar-truststore.sh.
// Retorna el almacén de confianza, o NULL si el archivo no existe o no trae ningún certificado.
// Se libera con X509_STORE_free
X509_STORE *loadTrustStore(const char *path);

// Lee un certificado PEM desde un archivo (por ejemplo nodo-AS65002-201.crt).
// Retorna NULL si no se pudo. Se libera con X509_free
X509 *certFromFile(const char *path);

// Pasa el certificado a base64 en una sola línea, como viaja en HELLO|OWNER y en la respuesta de CERT.
// Es el certificado en binario (DER) codificado en base64, sin encabezados ni saltos de línea.
// Retorna un string hecho con malloc (hay que hacerle free), o NULL si falla
char *certToBase64(X509 *cert);

// Lo contrario: arma el certificado a partir del base64 que llegó por la red.
// Retorna NULL si el texto no es un certificado. Se libera con X509_free
X509 *certFromBase64(const char *base64);

// Valida la cadena nodo -> raíz como pide el diseño (RFC 5280, PKI de un solo nivel): fechas vigentes,
// emitido y firmado por una CA del truststore, sin CA intermedias, y extensiones críticas correctas
// (raíz con CA:TRUE y keyCertSign, nodo con CA:FALSE y digitalSignature).
// Retorna 1 si es válido. Si no, retorna 0 y deja en reason el motivo, para registrarlo en la bitácora
int validateNodeCert(X509_STORE *store, X509 *cert, char *reason, size_t reasonSize);

// Copia el CN del certificado (por ejemplo "AS65002-201") en cn.
// Retorna 0 si lo encontró, -1 si no tiene CN o no cabe en cn
int getCommonName(X509 *cert, char *cn, size_t cnSize);

// Revisa que el CN sea exactamente AS<island>-<emitterNum>, el paso 4 de la validación del ROA (punto B).
// Retorna 1 si coincide, 0 si no
int cnMatches(X509 *cert, int island, int emitterNum);

// Fin de la vigencia del certificado (notAfter) en segundos Unix. El punto B lo usa para revisar
// que el VENCE del ROA no se pase de esa fecha. Retorna -1 si no se pudo leer
time_t getNotAfter(X509 *cert);

// Llave pública del nodo, para que el punto B verifique la FIRMA del ROA.
// Le pertenece al certificado: no se libera aparte, se va con el X509_free del certificado
EVP_PKEY *getPublicKey(X509 *cert);

#endif // CERTIFICATES_H
