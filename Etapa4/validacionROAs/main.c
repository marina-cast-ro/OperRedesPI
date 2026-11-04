#include "roa_validate.h"

// Se corre así (en esta misma carpeta)...
//   make clean
//   chmod +x probar.sh roa-firmar.sh  Antes hay que hacer ejecutables de los scripts de prueba
//   make
//   make roa
//   make run

char *read_doc(const char *path) {
    FILE *f = fopen(path, "rb");

    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    rewind(f);

    if (length < 0) {
        fclose(f);
        return NULL;
    }

    char *data = malloc((size_t)length + 1);

    if (!data) {
        fclose(f);
        return NULL;
    }

    size_t read_data = fread(data, 1, (size_t)length, f);

    fclose(f);

    if (read_data != (size_t)length) {
        free(data);
        return NULL;
    }

    data[read_data] = '\0';

    return data;
}

int main(int c, char **v) {
    char err[128];
    const char *motivo = "";
    int salida = 2;

    if (c < 4) {
        fprintf(stderr, "Uso: %s truststore.pem certificado_b64 linea [hora]\n", v[0]);
        return 1;
    }

    roa_t r = {0};
    roa_trusted_roots_t R = {0};

    if (roa_charge_roots(v[1], &R, err, sizeof err)) {
        printf("ERROR: %s\n", err);
        return 2;
    }

    char *b64 = read_doc(v[2]);
    char *linea = read_doc(v[3]);
    X509 *cert = NULL;

    if (!b64 || !linea) {
        printf("ERROR: no se pudo leer el certificado o el ROA\n");
    } else {
        b64[strcspn(b64, "\n")] = '\0';
        cert = roa_cert_desde_b64(b64);

        if (!cert) {
            printf("ERROR: el certificado X509 es invalido\n");
        } else if (roa_parse(linea, &r, err, sizeof err)) {
            printf("PARSEO: %s\n", err);
        } else {
            // probar.sh manda la hora en el 4to argumento para probar la vigencia
            time_t hora = (c > 4) ? (time_t)atol(v[4]) : time(NULL);
            roa_veredicto_t rc = roa_validar(&r, cert, &R, hora, &motivo);

            if (rc == ROA_OK) {
                printf("OK\n");
            } else {
                printf("%s: %s\n", rc == ROA_RECHAZADO ? "RECHAZADO" : "RECUPERABLE", motivo);
            }
            salida = 0;
        }
    }

    free(b64);
    free(linea);
    X509_free(cert);
    roa_free(&r);
    roa_free_roots(&R);
    return salida;
}
