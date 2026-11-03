#include "roa_validate.h"

int main(int c, char **v) {
    char err[128]; 
	const char *motivo = "";
    
	roa_t r; 
	roa_trusted_roots_t R;
    
	if (roa_charge_roots(v[1], &R, err, sizeof err)) { 
		puts(err);
		return 2;
	}
    
	/**
	//char *b64 = leer v[2] sin \n, *linea = leer v[3];
    X509 *cert = roa_cert_desde_b64(b64);
    
	if (!cert || roa_parsear(linea, &r, err, sizeof err)) { 
		puts(cert ? err : "cert invalido"); 
		return 2;
	}
    
	int rc = roa_validar(&r, cert, &R, time(NULL), &motivo);
    printf("%d %s\n", rc, motivo);
	*/
}