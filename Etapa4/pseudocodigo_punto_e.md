# Punto E: validación de ROAs en el enrutador

```
// Parámetros
PUERTO = 9000
PERIODO = 75 s
ESPERA_INICIAL = azar(0, 75) s
TIMEOUT = 10 s

// Compartido entre los dos hilos, protegido con lectores-escritores
tabla = vacía                 // ip -> (isla, vence)
versionLocal = -1

// Hilo sincronizador (escritor)
dormir(ESPERA_INICIAL)
cada PERIODO:
    resp = repo("LIST") // OK|<version>|<n> + n líneas ROA
    si falla, timeout o llegan menos de n líneas: se conserva la tabla anterior
    si resp.version == versionLocal: no se hace nada
    nueva = vacía // se arma fuera del candado
    para cada roa en resp:
        si validarROA(roa): nueva[roa.ip] = (roa.isla, roa.vence)
        si no: registrar el motivo
    bloquear escritura
        tabla = nueva // double buffering
        versionLocal = resp.version
    desbloquear

repo(comando):
    conectar TCP a PUERTO con TIMEOUT
    enviar "HELLO|ENRUTADOR|<id>\n"
    si no responde "HELLO_OK|ENRUTADOR|-": cerrar y fallar
    enviar comando + "\n"
    leer la respuesta acumulando hasta cada "\n"
    cerrar

validarROA(ROA|IP|ISLA|NUM_EMISOR|EMISION|VENCE|FIRMA):
    1. tiene 7 campos y la IP es válida
    2. EMISION <= ahora <= VENCE
    3. cert = repo("CERT|" + NUM_EMISOR) es válido contra las raíces (punto A)
    4. CN de cert == "AS" + ISLA + "-" + NUM_EMISOR
    5. VENCE <= notAfter de cert
    6. FIRMA verifica con la llave de cert (RSA-2048, SHA-256) sobre el texto exacto "ROA|...|VENCE"

// Hilo de enrutamiento (lector), al recibir un anuncio con (IP, ISLA)
bloquear lectura
    roa = tabla[IP]
desbloquear
si no hay roa o roa.vence < ahora:  UNKNOWN, se acepta
si roa.isla == ISLA:                VALID, se acepta
si no:                              INVALID, se descarta y se registra
```
