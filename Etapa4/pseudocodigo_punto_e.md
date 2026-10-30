# Punto E: validación de ROAs en los enrutadores (pseudocódigo común)

Este pseudocódigo junta lo que acordamos en el Doc ETAPA_3_PI_DISEÑO: la sección «Punto E», la «Parte II: Protocolo
Repo-Router» y los pasos de validación del punto B. La idea es que los cinco grupos programemos el enrutador con la
misma lógica, cada uno en su lenguaje, para que en la demo cualquier router funcione con cualquier repositorio.

Lo que está marcado **[PROPUESTA]** no está en el Doc. Lo marcado **[POR ACORDAR]** hay que cerrarlo entre todos.
Al final hay una lista de pruebas para hacer iguales en todos los grupos.

## 1. Constantes

```
PUERTO_REPO     <- 9000
PERIODO         <- 75       // segundos entre una descarga y la siguiente
ESPERA_INICIAL  <- 75       // tope de la espera al azar antes de la primera descarga
TIMEOUT         <- 10       // segundos para conectar y para cada lectura
ID_ENRUTADOR    <- id de este router
IP_REPO         <- IP del repositorio activo en la demo
RUTA_RAICES     <- "/etc/rpki/truststore/trusted-roots.pem"
```

## 2. Datos compartidos entre los dos hilos

```
estructura EntradaROA
    ip           // IPv4
    isla         // entero
    numEmisor    // entero
    emision      // segundos Unix UTC
    vence        // segundos Unix UTC

tablaActual   <- tabla vacía    // ip -> EntradaROA. La lee el hilo de enrutamiento
versionLocal  <- -1             // con -1 la primera descarga siempre se procesa
```

El repositorio guarda un ROA por IP (GET, PUT y DELETE van por IP), por eso la tabla usa la IP como llave.

Solo el hilo sincronizador usa estos dos:

```
truststore    <- raíces cargadas de RUTA_RAICES
cacheCerts    <- diccionario vacío    // numEmisor -> certificado   [PROPUESTA]
```

## 3. Semáforos: lectores y escritores

El hilo de enrutamiento es el lector y puede haber varios a la vez, uno por conexión de un vecino. El hilo
sincronizador es el escritor y entra solo.

```
mutexLectores <- semáforo(1)    // protege el contador
escritura     <- semáforo(1)    // lo tiene el escritor o el grupo de lectores
lectores      <- 0

entrarLector():
    esperar(mutexLectores)
    lectores <- lectores + 1
    si lectores == 1: esperar(escritura)     // el primer lector bloquea al escritor
    señalar(mutexLectores)

salirLector():
    esperar(mutexLectores)
    lectores <- lectores - 1
    si lectores == 0: señalar(escritura)     // el último lector lo suelta
    señalar(mutexLectores)

entrarEscritor(): esperar(escritura)
salirEscritor():  señalar(escritura)
```

En C esto mismo es un `pthread_rwlock_t`. Como el escritor solo toma el candado para cambiar un puntero (sección 4),
un mutex simple también sirve.

## 4. Hilo sincronizador (escritor)

### 4.1 Bucle principal

```
hiloSincronizador():
    dormir(número al azar entre 0 y ESPERA_INICIAL)    // solo la primera vez
    repetir para siempre:
        cicloDeSincronizacion()
        dormir(PERIODO)
```

### 4.2 Un ciclo

```
cicloDeSincronizacion():
    recargarRaicesSiCambiaron()                          // [PROPUESTA], sección 4.7

    respuesta <- pedirAlRepositorio("LIST")
    si respuesta es FALLA:
        registrar("LIST falló: " + motivo + ". Sigue la tabla de la versión " + versionLocal)
        terminar el ciclo

    (version, n, lineas) <- respuesta
    si version == versionLocal:
        registrar("Versión " + version + " sin cambios")
        terminar el ciclo                                // no se revisa ninguna firma

    // Se arma una tabla nueva FUERA del candado. Mientras tanto la vieja sigue en uso
    tablaNueva <- tabla vacía
    para cada linea en lineas:
        resultado <- validarROA(linea)
        si resultado es ABORTAR:                         // el repositorio se cayó a mitad del ciclo
            registrar("Se cayó el repositorio en un CERT. Sigue la tabla anterior")
            liberar(tablaNueva)
            terminar el ciclo
        si resultado es VALIDO:
            tablaNueva[resultado.ip] <- resultado
        si no:
            registrar("ROA descartado (" + linea + "): " + motivo)

    // Double buffering: el candado solo dura lo que tarda cambiar un puntero
    entrarEscritor()
    tablaVieja   <- tablaActual
    tablaActual  <- tablaNueva
    versionLocal <- version
    salirEscritor()

    liberar(tablaVieja)     // nadie la usa: el escritor entró cuando no había lectores
    registrar("Tabla ROA en la versión " + version + ": " + tamaño(tablaNueva) + " válidos de " + n)
```

### 4.3 Una conexión con el repositorio

Cada conexión lleva un solo pedido: HELLO, un comando y se cierra.

```
pedirAlRepositorio(comando):
    sock <- conectarTCP(IP_REPO, PUERTO_REPO, espera máxima TIMEOUT)
    si no conecta: devolver FALLA("no conecta")
    poner timeout de lectura TIMEOUT en sock

    enviar(sock, "HELLO|ENRUTADOR|" + ID_ENRUTADOR + "\n")
    saludo <- leerLinea(sock)
    si saludo no empieza con "HELLO_OK|ENRUTADOR|":
        cerrar(sock)
        devolver FALLA("HELLO rechazado: " + saludo)     // se aborta de inmediato

    enviar(sock, comando + "\n")
    encabezado <- leerLinea(sock)
    si encabezado es FALLA o empieza con "ERR_":
        cerrar(sock)
        devolver FALLA(encabezado)                       // ERR_NO_EXISTE, ERR_FORMATO, timeout...

    si comando es "LIST":
        campos <- partir(encabezado, "|")                // OK|<version>|<n>
        si campos[0] != "OK" o cantidad(campos) != 3:
            cerrar(sock)
            devolver FALLA("encabezado de LIST mal formado")
        version <- entero(campos[1])
        n <- entero(campos[2])
        lineas <- lista vacía
        repetir n veces:
            linea <- leerLinea(sock)
            si linea es FALLA:
                cerrar(sock)
                devolver FALLA("LIST incompleto")        // se bota TODA la descarga
            agregar linea a lineas
        cerrar(sock)
        devolver (version, n, lineas)

    si comando empieza con "CERT|":
        cerrar(sock)
        campos <- partir(encabezado, "|")                // OK|<cert_b64>
        si campos[0] != "OK" o cantidad(campos) != 2:
            devolver FALLA("respuesta de CERT mal formada")
        devolver campos[1]
```

### 4.4 Leer una línea de TCP

TCP es un flujo de bytes. Una lectura puede traer media línea o varias juntas, así que se acumula hasta el salto de
línea.

```
leerLinea(sock):
    mientras buffer(sock) no tenga "\n":
        datos <- recibir(sock)                           // respeta el TIMEOUT
        si timeout o la conexión se cerró: devolver FALLA
        buffer(sock) <- buffer(sock) + datos
    linea <- lo que hay antes del primer "\n"
    buffer(sock) <- lo que sobra después del "\n"
    quitar el "\r" final de linea, si lo trae
    devolver linea
```

### 4.5 Conseguir el certificado de un emisor

```
obtenerCertificado(numEmisor):
    si numEmisor está en cacheCerts:                     // [PROPUESTA]
        cert <- cacheCerts[numEmisor]
        si certificadoValido(cert): devolver cert        // se revisa igual, por si venció
        quitar numEmisor de cacheCerts

    b64 <- pedirAlRepositorio("CERT|" + numEmisor)
    si b64 es FALLA:
        si el motivo es ERR_NO_EXISTE: devolver INVALIDO("no hay certificado para " + numEmisor)
        devolver ABORTAR                                 // timeout o no conecta [PROPUESTA]

    cert <- certificadoDesdeBase64(b64)                  // el base64 es del DER [POR ACORDAR]
    si cert es nulo: devolver INVALIDO("el base64 no es un certificado")
    si no certificadoValido(cert): devolver INVALIDO("certificado: " + motivo)
    cacheCerts[numEmisor] <- cert
    devolver cert

certificadoValido(cert):
    // Punto A del Doc (RFC 5280, PKI plana):
    // 1. la hora actual está entre notBefore y notAfter del nodo y de la raíz
    // 2. el issuer del nodo es una raíz del truststore
    // 3. la firma del certificado verifica con la llave de esa raíz
    // 4. la cadena mide exactamente 2 (nodo -> raíz)
    // 5. la raíz tiene CA:TRUE y keyCertSign críticos. El nodo tiene CA:FALSE y digitalSignature críticos
    // Si algo falla, devuelve falso y el motivo
```

En C, Koopatroopas ya tiene estas funciones en `Etapa4/certificados/certificates.h`: `certFromBase64`,
`validateNodeCert`, `cnMatches`, `getNotAfter` y `getPublicKey`. Los grupos que también usan C pueden usarlas tal cual.

### 4.6 Validar un ROA (pasos 1 a 6 del punto B)

```
validarROA(linea):
    ahora <- hora actual en segundos Unix (UTC)
    campos <- partir(linea, "|")

    // Paso 1. Siete campos y la IP es válida
    si cantidad(campos) != 7 o campos[0] != "ROA": devolver INVALIDO("no tiene 7 campos")
    ip <- campos[1]
    isla <- campos[2]
    numEmisor <- campos[3]
    emision <- campos[4]
    vence <- campos[5]
    firma <- campos[6]
    si ip no es IPv4 o isla, numEmisor, emision o vence no son enteros:
        devolver INVALIDO("campo mal formado")

    // Paso 2. Vigente
    si no (emision <= ahora <= vence): devolver INVALIDO("fuera de EMISION y VENCE")

    // Paso 3. El certificado del emisor es válido
    cert <- obtenerCertificado(numEmisor)
    si cert es ABORTAR: devolver ABORTAR
    si cert es INVALIDO: devolver cert

    // Paso 4. El CN es exactamente AS<ISLA>-<NUM_EMISOR>
    si nombreComun(cert) != "AS" + isla + "-" + numEmisor: devolver INVALIDO("el CN no coincide")

    // Paso 5. El ROA no vence después que el certificado
    si vence > notAfter(cert): devolver INVALIDO("VENCE pasa el notAfter del certificado")

    // Paso 6. La firma verifica. Se firma el texto EXACTO desde "ROA" hasta VENCE, tal como llegó,
    // sin el último "|" ni la firma. No se vuelve a armar con los números ya convertidos
    mensaje <- linea hasta antes del último "|"
    si no verificarRSA_SHA256(llavePublica(cert), mensaje, decodificarBase64(firma)):
        devolver INVALIDO("la firma no verifica")

    devolver EntradaROA(ip, isla, numEmisor, emision, vence)    // VALIDO
```

### 4.7 Recargar las raíces [PROPUESTA]

Las CA de los otros grupos pueden llegar con el router ya corriendo. Si las raíces solo se cargan al arrancar, el
router no las ve hasta reiniciarlo.

```
recargarRaicesSiCambiaron():
    si la fecha de modificación de RUTA_RAICES cambió desde la última carga:
        nuevas <- cargarRaices(RUTA_RAICES)
        si nuevas no es nulo:
            truststore <- nuevas
            vaciar cacheCerts
            versionLocal <- -1          // obliga a revisar todo otra vez con las raíces nuevas
```

## 5. Hilo de enrutamiento (lector)

```
evaluarAnuncio(ip, islaOrigen):
    ahora <- hora actual en segundos Unix (UTC)

    entrarLector()
    roa <- tablaActual.buscar(ip)
    si roa existe:
        islaROA <- roa.isla          // se copia lo necesario. No se guarda el puntero a la tabla
        venceROA <- roa.vence
    salirLector()

    si no existe roa o venceROA < ahora:  resultado <- UNKNOWN    // ver «Por acordar», punto 4
    si no, si islaROA == islaOrigen:      resultado <- VALID
    si no:                                resultado <- INVALID
    registrar("Anuncio " + ip + " desde la isla " + islaOrigen + ": " + resultado)
    devolver resultado

alRecibirAnuncio(anuncio):
    para cada (ip, islaOrigen) del anuncio:          // [POR ACORDAR] el anuncio tiene que traer la isla
        si evaluarAnuncio(ip, islaOrigen) == INVALID:
            registrar("Ruta descartada: " + ip + " no es de la isla " + islaOrigen)
            no se agrega a la tabla de rutas        // el punto F nunca la ve
        si no:
            se pasa la ruta al punto F (ruta más corta)
```

VALID y UNKNOWN se aceptan. Solo INVALID se descarta, igual que en el RPKI real (RFC 6811).

## 6. Arranque del router

```
iniciarRouter():
    truststore <- cargarRaices(RUTA_RAICES)
    si truststore es nulo:
        registrar("No hay raíces de confianza. No se puede validar ningún ROA")
        terminar
    iniciar los semáforos
    crear el hilo hiloSincronizador()
    seguir con el router de siempre (ANNOUNCE, DATA y punto F)
    // Antes de la primera descarga la tabla está vacía: todo da UNKNOWN y se acepta
```

El enunciado dice «proceso aparte». Usamos un hilo porque comparte la memoria con el de enrutamiento y la tabla se
protege directo con los semáforos. Con un proceso de verdad, la tabla iría en memoria compartida y el semáforo tendría
nombre. La idea de sincronización es la misma.

## 7. Cambio de repositorio en la demo [PROPUESTA]

Las versiones de dos repositorios distintos no se pueden comparar. Si el repositorio nuevo también va en la versión 12,
el router creería que no hubo cambios.

```
cuando cambia IP_REPO:
    versionLocal <- -1
    vaciar cacheCerts
```

Como los dueños reenvían su ROA cada 60 s, el repositorio nuevo se llena solo en un minuto.

## 8. Por acordar entre los grupos

1. **El anuncio de ruta tiene que traer la isla de origen.** Para validar hace falta la pareja (IP, isla). Hoy el
   ADVERTISE de la Etapa 2 solo lleva la IP, y el ANNOUNCE del punto F lleva la IP del vecino y el costo. Sin la isla
   no hay con qué comparar el ROA. Hay que agregar ese campo al formato.
2. **El certificado en base64 es el DER**, en una sola línea, en HELLO y en la respuesta de CERT. Con OpenSSL:
   `openssl x509 -in nodo.crt -outform DER | base64 -w0`.
3. **NUM_EMISOR único en toda la red**, porque CERT solo lleva ese número. Propuesta: cada grupo usa su centena.
4. **Un ROA vencido cuenta como si no existiera** (UNKNOWN, se acepta). El Doc dice que VENCE se compara con la hora
   en el momento del anuncio, pero no dice qué resultado da. Esto sale del punto B: si falla un paso, el ROA se
   descarta.
5. **Las extensiones de los certificados tienen que ir críticas**, como dice el punto A. Si una CA o un nodo no las
   trae críticas, los routers que siguen el Doc rechazan sus ROAs. Conviene que todos usemos los mismos archivos
   `ca-ext.cnf` y `nodo-ext.cnf`.

## 9. Bitácora: qué registrar

1. Cada ciclo: versión remota, versión local y si cambió o no.
2. Cada ROA descartado, con el motivo.
3. Cada fallo con el repositorio (no conecta, timeout, HELLO rechazado, LIST incompleto).
4. Cada anuncio evaluado, con VALID, INVALID o UNKNOWN.
5. Cada ruta descartada por INVALID.

## 10. Pruebas para hacer iguales en todos los grupos

1. Un ROA bien firmado y un anuncio de la misma isla dan VALID.
2. Un anuncio de esa misma IP desde otra isla da INVALID y la ruta no entra a la tabla.
3. Una IP sin ROA da UNKNOWN y la ruta se acepta.
4. Si se cambia un carácter de la IP en un ROA ya firmado, se descarta con «la firma no verifica».
5. Un ROA firmado por un nodo de otro grupo da VALID si la CA de ese grupo está en las raíces.
6. Con el repositorio apagado, a los 10 s el router sigue con la tabla anterior y reintenta en 75 s.
7. Si el LIST trae la misma versión, no se pide ningún CERT ni se revisa ninguna firma.
8. Un ROA vencido no da VALID.
