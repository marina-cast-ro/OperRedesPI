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
    
	if (c < 4) {
		fprintf(stderr, "Uso: %s truststore.pem certificado_b64 linea\n", v[0]);
        return 1;
	}

	roa_t r = {0}; 
	roa_trusted_roots_t R = {0};
    
	if (roa_charge_roots(v[1], &R, err, sizeof err)) { 
		puts(err);
		return 2;
	}
    
	// Lectura del certificado
	char *b64 = read_doc(v[2]);
	if (!b64) {
		fprintf(stderr, "No se pudo leer el certificado\n");
		roa_free_roots(&R);
		return 2;
	}
	b64[strcspn(b64, "\n")] = '\0';

	char *linea = read_doc(v[3]);

    X509 *cert = roa_cert_desde_b64(b64);
    
	// Error en el certificado
	if (!cert) { 
		fprintf(stderr, "ERROR: el certificado X509 es invalido\n");
		roa_free_roots(&R);
		return 2;
	}
    fprintf(stderr, "OK: certificado X509 cargado y validado\n");

	// Error en el parseo de líneas ROA
	fprintf(stderr, "LINEA ROA:\n[%s]\n", linea);
	if (roa_parse(linea, &r, err, sizeof err)) { 
		fprintf(stderr, "ERROR: parseo ROA resulto invalido\n");
		X509_free(cert);
		roa_free(&r);
		roa_free_roots(&R);
		return 2;
	}
	fprintf(stderr, "OK: parseo ROA finalizado\n");

	int rc = roa_validar(&r, cert, &R, time(NULL), &motivo);
    printf("%d %s\n", rc, motivo);

	free(b64);
	free(linea);
	X509_free(cert);
	roa_free(&r);
	roa_free_roots(&R);
}