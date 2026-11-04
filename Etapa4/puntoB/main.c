#include "roa_validate.h"

// Antes que nada hay que hacer ejecutables de los scripts de prueba
//   chmod +x probar.sh roa-firmar.sh
// Luego...
//   make clean
//   make
//   make roa

int main(int c, char **v) {
    char err[128]; 
	const char *motivo = "";
    
	

	roa_t r; 
	roa_trusted_roots_t R;
    
	if (roa_charge_roots(v[1], &R, err, sizeof err)) { 
		puts(err);
		return 2;
	}
    
	// Eliminado del \n de v[2]
	char *b64 = v[2];
	b64[strcspn(b64, "\n")] = '\0';

	char *linea = v[3];

    X509 *cert = roa_cert_desde_b64(b64);
    
	
	if (!cert) {
	//if (!cert || roa_parsear(linea, &r, err, sizeof err)) { 
		puts(cert ? err : "cert invalido"); 
		return 2;
	}
    
	//int rc = roa_validar(&r, cert, &R, time(NULL), &motivo);
    //printf("%d %s\n", rc, motivo);

	fprintf(stderr, "Certificados cargados: %zu\n", R.n);
	roa_free_roots(&R);
}