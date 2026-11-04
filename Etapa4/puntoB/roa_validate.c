#include "roa_validate.h"

static void poner_err(char *err, size_t n, const char *msg) {
    if (err && n) snprintf(err, n, "%s", msg);
}

/* ---------------- base64 ---------------- */

/* Decodifica base64 en una sola linea. Devuelve un buffer malloc (o NULL) y su largo. */
static unsigned char *b64_decode(const char *s, size_t *length) {
    if (!s || !length) {
        fprintf(stderr, "Base64: puntero NULL\n");
        return NULL;
    }

    size_t n = strlen(s);
    fprintf(stderr, "Base64 recibido: %zu caracteres\n", n);
    
    if (n == 0) {
        fprintf(stderr, "ERROR: cadena vacia\n");
        return NULL;
    }

    if (n % 4 != 0) {
        fprintf(stderr, "ERROR: longitud no multiplo de 4: %zu\n", n);
        return NULL;
    }

    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!(isalnum(c) || c == '+' || c == '/' || c == '=')) { 
            fprintf(stderr, "ERROR: caracter invalido en posicion %zu: 0x%02X\n", i, c);
            return NULL;
        }
    }

    unsigned char *out = malloc(n / 4 * 3 + 1);

    if (!out) {
        fprintf(stderr, "ERROR: malloc\n");
        return NULL;
    }

    int len = EVP_DecodeBlock(out, (const unsigned char *)s, (int)n);
    
    if (len < 0) { 
        fprintf(stderr, "ERROR: EVP_DecodeBlock\n");
        free(out); 
        return NULL; 
    }
    
    if (s[n - 1] == '=') len--;       /* EVP_DecodeBlock cuenta el relleno */
    if (s[n - 2] == '=') len--;
    
    *length = (size_t)len;
    return out;
}

/* ---------------- parseo del ROA ---------------- */

static int entero(const char *s, long *out) {
    if (!*s) return -1;
    for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return -1;
    errno = 0;
    char *fin;
    long v = strtol(s, &fin, 10);
    if (errno || *fin) return -1;
    *out = v;
    return 0;
}

static int ipv4_valida(const char *s) {
    int partes = 0;
    const char *p = s;
    while (1) {
        if (!isdigit((unsigned char)*p)) return 0;
        int v = 0, d = 0;
        while (isdigit((unsigned char)*p)) { v = v * 10 + (*p - '0'); d++; p++; if (d > 3 || v > 255) return 0; }
        partes++;
        if (*p == '.') { p++; continue; }
        break;
    }
    return partes == 4 && *p == '\0';
}

int roa_parse(const char *linea, roa_t *out, char *err, size_t errlen) {
    memset(out, 0, sizeof *out);
    char *copia = strdup(linea);

    if (!copia) { 
		poner_err(err, errlen, "sin memoria"); 
		return -1;
	}

    size_t l = strlen(copia);

    while (l && (copia[l - 1] == '\n' || copia[l - 1] == '\r')) 
		copia[--l] = '\0';

    char *campo[8];
    int n = 0;
    char *p = copia;
    campo[n++] = p;
    for (; *p; p++) {
        if (*p == '|') {
            *p = '\0';
            if (n == 8) { n = 9; break; }
            campo[n++] = p + 1;
        }
    }
    if (n != 7 || strcmp(campo[0], "ROA") != 0) {
        poner_err(err, errlen, "formato: se esperan 7 campos y el primero debe ser ROA");
        free(copia);
        return -1;
    }

    long isla, num, emision, vence;
    if (!ipv4_valida(campo[1]) || strlen(campo[1]) > 15) {
        poner_err(err, errlen, "IP invalida"); free(copia); return -1;
    }
    if (entero(campo[2], &isla) || entero(campo[3], &num) ||
        entero(campo[4], &emision) || entero(campo[5], &vence)) {
        poner_err(err, errlen, "ISLA, NUM_EMISOR, EMISION y VENCE deben ser enteros");
        free(copia); return -1;
    }
    if (emision > vence) {
        poner_err(err, errlen, "EMISION no puede ser posterior a VENCE"); free(copia); return -1;
    }
    size_t flen;
    unsigned char *firma = b64_decode(campo[6], &flen);
    if (!firma || flen == 0) {
        poner_err(err, errlen, "FIRMA no es base64 valido"); free(firma); free(copia); return -1;
    }

    /* El texto firmado es la linea original sin el ultimo campo ni su '|'. */
    size_t largo_texto = (size_t)(campo[6] - copia) - 1;
    char *texto = malloc(largo_texto + 1);
    if (!texto) { free(firma); free(copia); poner_err(err, errlen, "sin memoria"); return -1; }
    /* 'copia' tiene '\0' en lugar de '|': se reconstruye desde 'linea'. */
    memcpy(texto, linea, largo_texto);
    texto[largo_texto] = '\0';

    strcpy(out->ip, campo[1]);
    out->isla = (int)isla;
    out->num_emisor = num;
    out->emision = emision;
    out->vence = vence;
    out->texto_firmado = texto;
    out->firma = firma;
    out->firma_len = flen;
    free(copia);
    return 0;
}

void roa_free(roa_t *r) {
    if (!r) return;
    free(r->texto_firmado);
    free(r->firma);
    memset(r, 0, sizeof *r);
}

/* ---------------- truststore ---------------- */

int roa_charge_roots(const char *ruta, roa_trusted_roots_t *out, char *err, size_t errlen) {
    out->v = NULL;
    out->n = 0;
    BIO *bio = BIO_new_file(ruta, "r");

    if (!bio) { 
        poner_err(err, errlen, "no se pudo abrir el truststore"); 
        return -1; 
    }

    size_t cap = 0;
    X509 *c;
    
    while ((c = PEM_read_bio_X509(bio, NULL, NULL, NULL)) != NULL) {
        if (out->n == cap) {
            size_t new_cap = cap ? cap * 2 : 8;
            X509 **nv = realloc(out->v, new_cap * sizeof *nv);
            
            if (!nv) { 
                X509_free(c); 
                BIO_free(bio);
                ERR_clear_error();
                
                for (size_t i = 0; i < out->n; i++)
                    X509_free(out->v[i]);

                free(out->v);
                out->v = NULL;
                out->n = 0;

                poner_err(err, errlen, "error de memoria al cargar las raices");

                return -1;
            }
            
            out->v = nv;
            cap = new_cap;
        }
        out->v[out->n++] = c;
    }
    BIO_free(bio);
    ERR_clear_error();   // el fin de archivo deja un error de PEM en la cola
    
    if (out->n == 0) {
        free(out->v);
        out->v = NULL;
        poner_err(err, errlen, "el truststore no contiene ninguna raiz");
        return -1;
    }
    
    return 0;
}

void roa_free_roots(roa_trusted_roots_t *r) {
    if (!r) return;

    for (size_t i = 0; i < r->n; i++) 
		X509_free(r->v[i]);
    
	free(r->v);
    r->v = NULL;
    r->n = 0;
}

/* ---------------- certificados ---------------- */

X509 *roa_cert_desde_b64(const char *b64) {
    size_t n = 0;
    unsigned char *raw = b64_decode(b64, &n);

    if (!raw) {
        fprintf(stderr, "ERROR: Base64 no valido\n");
        return NULL;
    }

    fprintf(stderr, "Base64: %zu caracteres\n", strlen(b64));
    fprintf(stderr, "Decodificado: %zu bytes\n", n);
    fprintf(stderr, "Primeros bytes: ");
    for (size_t i = 0; i < n && i < 30; i++)
        fprintf(stderr, "%02X ", raw[i]);
    fprintf(stderr, "\n");

    X509 *cert = NULL;
    static const char PEM_INI[] = "-----BEGIN CERTIFICATE-----";
    if (n >= sizeof PEM_INI - 1 
        && memcmp(raw, PEM_INI, sizeof PEM_INI - 1) == 0) {
        
        fprintf(stderr, "Formato detectado: PEM\n");
        BIO *bio = BIO_new_mem_buf(raw, (int)n);
        if (bio) { 
            cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
            BIO_free(bio); 
        }
    } else {
        fprintf(stderr, "Formato detectado: DER\n");

        const unsigned char *p = raw;
        cert = d2i_X509(NULL, &p, (long)n);

        if (cert && p != raw + n)
            fprintf(stderr, "ADVERTENCIA: bytes sobrantes despues del DER\n");
    }

    if (!cert) {
        fprintf(stderr, "ERROR OPENSSL:\n");
        ERR_print_errors_fp(stderr);
    }

    free(raw);
    //ERR_clear_error();
    return cert;
}

/* ---------------- tiempo ---------------- */

/* Convierte un ASN1_TIME a segundos Unix. 0 si esta bien. */
static int asn1_a_unix(const ASN1_TIME *t, long *out) {
    ASN1_TIME *epoca = ASN1_TIME_adj(NULL, 0, 0, 0);
    if (!epoca) return -1;
    int dias, segs;
    int ok = ASN1_TIME_diff(&dias, &segs, epoca, t);
    ASN1_STRING_free(epoca);
    if (!ok) return -1;
    *out = (long)dias * 86400L + segs;
    return 0;
}

static int vigente(X509 *c, time_t ahora) {
    long ini, fin;
    if (asn1_a_unix(X509_get0_notBefore(c), &ini) || asn1_a_unix(X509_get0_notAfter(c), &fin)) return 0;
    return ini <= (long)ahora && (long)ahora <= fin;
}

/* ---------------- cadena (punto A, 05_validar_cadena) ---------------- */

/* 1 si tiene la extension critica y cumple lo pedido. */
static int basic_constraints_ok(X509 *c, int debe_ser_ca) {
    int crit = -1;
    BASIC_CONSTRAINTS *bc = X509_get_ext_d2i(c, NID_basic_constraints, &crit, NULL);
    if (!bc) return 0;
    int ok = (crit == 1) && ((bc->ca != 0) == (debe_ser_ca != 0));
    if (ok && debe_ser_ca) {
        /* pathlen:0 -> la raiz no puede firmar otras CA */
        ok = bc->pathlen && ASN1_INTEGER_get(bc->pathlen) == 0;
    }
    BASIC_CONSTRAINTS_free(bc);
    return ok;
}

static int key_usage_ok(X509 *c, int bit) {
    int crit = -1;
    ASN1_BIT_STRING *ku = X509_get_ext_d2i(c, NID_key_usage, &crit, NULL);
    if (!ku) return 0;
    int ok = (crit == 1) && ASN1_BIT_STRING_get_bit(ku, bit);
    ASN1_BIT_STRING_free(ku);
    return ok;
}

roa_veredicto_t roa_validar_cadena(X509 *cert, const roa_trusted_roots_t *raices, time_t ahora, const char **motivo) {
    /* Paso 1: vigencia del nodo */
    if (!vigente(cert, ahora)) {
        *motivo = "el certificado del nodo no esta vigente";
        return ROA_RECHAZADO_RECUPERABLE;
    }
    /* Pasos 2 y 3: emisor en el truststore y firma de esa raiz */
    X509 *raiz = NULL;
    for (size_t i = 0; i < raices->n; i++) {
        if (X509_NAME_cmp(X509_get_subject_name(raices->v[i]), X509_get_issuer_name(cert)) != 0) continue;
        EVP_PKEY *pk = X509_get0_pubkey(raices->v[i]);
        if (pk && X509_verify(cert, pk) == 1) { raiz = raices->v[i]; break; }
    }
    ERR_clear_error();
    if (!raiz) {
        *motivo = "el emisor no esta en el truststore o la firma de la CA no es valida";
        return ROA_RECHAZADO_RECUPERABLE;      /* puede llegar una raiz nueva sin que cambie la version */
    }
    /* Paso 1 (raiz) */
    if (!vigente(raiz, ahora)) {
        *motivo = "la CA raiz no esta vigente";
        return ROA_RECHAZADO_RECUPERABLE;
    }
    /* Paso 4: extensiones criticas */
    if (!basic_constraints_ok(raiz, 1) || !key_usage_ok(raiz, 5 /* keyCertSign */)) {
        *motivo = "extensiones incorrectas o no criticas en la raiz";
        return ROA_RECHAZADO;
    }
    if (!basic_constraints_ok(cert, 0) || !key_usage_ok(cert, 0 /* digitalSignature */)) {
        *motivo = "extensiones incorrectas o no criticas en el nodo";
        return ROA_RECHAZADO;
    }
    return ROA_OK;
}

/* ---------------- ROA (punto B, pasos 2 a 6) ---------------- */

roa_veredicto_t roa_validar(const roa_t *roa, X509 *cert, const roa_trusted_roots_t *raices, time_t ahora, const char **motivo) {
    /* Paso 2: vigencia del ROA */
    if (ahora < roa->emision) {
        *motivo = "ROA aun no vigente (EMISION en el futuro)";
        return ROA_RECHAZADO_RECUPERABLE;      /* reloj desajustado */
    }
    if (ahora > roa->vence) {
        *motivo = "ROA fuera de su rango EMISION-VENCE";
        return ROA_RECHAZADO;
    }
    /* Paso 3: cadena de confianza */
    roa_veredicto_t v = roa_validar_cadena(cert, raices, ahora, motivo);
    if (v != ROA_OK) return v;

    /* Paso 4: CN exactamente AS<ISLA>-<NUM_EMISOR> */
    char esperado[64], cn[128];
    snprintf(esperado, sizeof esperado, "AS%d-%ld", roa->isla, roa->num_emisor);
    int largo = X509_NAME_get_text_by_NID(X509_get_subject_name(cert), NID_commonName, cn, sizeof cn);
    if (largo < 0 || strcmp(cn, esperado) != 0) {
        *motivo = "el CN no coincide con la isla y el emisor del ROA";
        return ROA_RECHAZADO;
    }
    /* Paso 5: VENCE no puede ser posterior al notAfter del certificado */
    long fin;
    if (asn1_a_unix(X509_get0_notAfter(cert), &fin) || roa->vence > fin) {
        *motivo = "VENCE es posterior al notAfter del certificado";
        return ROA_RECHAZADO;
    }
    /* Paso 6: firma RSA-2048 + SHA-256 (PKCS#1 v1.5) con la llave del nodo */
    EVP_PKEY *pk = X509_get0_pubkey(cert);
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    int ok = 0;
    if (pk && ctx && EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, pk) == 1 &&
        EVP_DigestVerifyUpdate(ctx, roa->texto_firmado, strlen(roa->texto_firmado)) == 1 &&
        EVP_DigestVerifyFinal(ctx, roa->firma, roa->firma_len) == 1)
        ok = 1;
    EVP_MD_CTX_free(ctx);
    ERR_clear_error();
    if (!ok) {
        *motivo = "firma invalida";
        return ROA_RECHAZADO;
    }
    return ROA_OK;
}
