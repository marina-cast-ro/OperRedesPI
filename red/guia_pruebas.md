# 🚀 Guía Rápida de Pruebas en Mesa Local — Koopatroopas (Isla 2)

Esta guía explica paso a paso cómo correr y verificar todo el sistema (Repositorio, Enrutador C y Host Emisor/Receptor) de forma fácil y directa en la mesa del laboratorio.

---

## 0. Preparación antes de empezar

En **todas las máquinas** que vayan a usar en la mesa (o laptop), abran la terminal y descarguen los últimos cambios:
```bash
git fetch origin
git checkout pruebas_mr
git pull origin pruebas_mr
```

---

## 1. Distribución de Roles en la Mesa

Pueden usar 2 computadoras fijas (o 2 computadoras + 1 laptop):

| Rol | Máquina | Puerto | Qué corre |
| :--- | :--- | :--- | :--- |
| **Repositorio ROAs** | Compu 1 (o Laptop) | `9000` (TCP) | `./red/iniciar_repositorio.sh 9000` |
| **Enrutador C** | Compu 2 | `5005` (TCP) | `./enrutador --red config_koopatroopas.json <IP_REPO>` |
| **Host Emisor / Receptor** | Compu 1 o Compu 2 | - | `./red/iniciar_emisor_receptor.sh` |

> [!NOTE]
> Para saber la IP de cualquier máquina en la red local del switch, corran:
> ```bash
> hostname -I
> ```
> *(Ejemplo de IP típica asignada por el switch en VLAN 20: `172.16.20.10` o `172.16.20.100`)*.

---

## 2. Paso a Paso para la Prueba

### Paso 1: Iniciar el Repositorio de ROAs (Compu 1)
En la terminal de la máquina que actuará como repositorio:
```bash
./red/iniciar_repositorio.sh 9000
```
* **Salida esperada:**
  - Compila `repoServer` automáticamente si falta.
  - Muestra las IPs locales.
  - Imprime: `Repositorio escuchando en puerto 9000...`
* *Dejen esta terminal abierta ejecutándose.*

---

### Paso 2: Publicar el ROA al Repositorio (Compu 2 o Laptop)
En otra terminal, ejecuten el script interactivo:
```bash
./red/iniciar_emisor_receptor.sh
```
1. Escriban `1` y presionen `Enter` (*Publicar ROA al Repositorio*).
2. Ingresen la **IP del Repositorio** (la IP de la Compu 1, ej. `172.16.20.10`).
3. Para todo lo demás (*Puerto 9000, Emisor 201, Isla 2, Llaves*), **solo presionen `Enter`** para usar los valores por defecto automáticos.

* **Salida esperada de éxito:**
  ```text
  [1] Conectando a 172.16.20.10:9000...
  [2] Respuesta del repositorio: HELLO_OK|OWNER|<nonce>
  [3] ROA firmado: ROA|172.16.20.10|2|201|...|<firma_base64>
  [4] Comando PUT enviado al repositorio.
  [5] Resultado: PUT_OK
  ```
  *(El mensaje `PUT_OK` confirma que el certificado fue validado contra el truststore y el ROA quedó registrado).*

---

### Paso 3: Iniciar el Enrutador Koopatroopas (Compu 2)
En la Compu 2 (donde corre el router):
```bash
cd Etapa4/vector
make clean && make
./enrutador --red config_koopatroopas.json <IP_DEL_REPOSITORIO>
```
*(Reemplacen `<IP_DEL_REPOSITORIO>` por la IP de la Compu 1, ej. `172.16.20.10`).*

* **Salida esperada:**
  - El router arranca y se pone a escuchar en el puerto TCP `5005`.
  - Empieza a mandar `ANNOUNCE` periódicos cada 30 segundos.
  - Descarga los ROAs del repositorio para validar rutas.
* *Dejen esta terminal abierta ejecutándose.*

---

### Paso 4: Probar Receptor y Entrega de Datos (`output.txt`)

1. **Abrir el visor del Receptor (en una terminal):**
   ```bash
   ./red/iniciar_emisor_receptor.sh
   ```
   Seleccionen la **Opción 3** (*Ver mensajes recibidos en vivo*).
   Se quedará esperando datos mediante `tail -f output.txt`.

2. **Enviar paquete DATA de prueba (en otra terminal):**
   ```bash
   ./red/iniciar_emisor_receptor.sh
   ```
   Seleccionen la **Opción 2** (*Enviar mensaje DATA a otra isla*):
   - **IP del Router:** `127.0.0.1` (o la IP de la Compu 2 si están desde otra máquina)
   - **Puerto del Router:** `5005` (Enter)
   - **Isla Origen:** `1` (o cualquier otra isla)
   - **Isla Destino:** `2` (Nuestra isla Koopatroopas, para probar entrega)
   - **Mensaje:** `Hola Koopatroopas prueba exitosa`

* **Salida esperada:**
  - En la terminal del Router:
    ```text
    [ENTREGAR] Paquete llego a destino final (Isla 2).
    ```
  - En la terminal del Receptor (Opción 3):
    Aparecerá en tiempo real:
    ```text
    DATA|1|2|8|Hola Koopatroopas prueba exitosa
    ```
    Y quedará guardado permanentemente en `Etapa4/vector/output.txt`.

---

## 3. Pruebas Inter-Islas (Con otros grupos o Topología Estrella)

Cuando se conecten con otras mesas o con el router central:
1. En `Etapa4/vector/config_koopatroopas.json`, agreguen en la sección `"vecinos"` la IP y puerto del router vecino:
   ```json
   "vecinos": {
     "CENTRAL": { "equipo": "Centro", "ip": "172.30.X.Y", "puerto": 5005, "costo": 1 }
   }
   ```
2. El enrutador aprenderá automáticamente las rutas de las demás islas que anuncie el nodo central.
3. Para mandar un mensaje a otra isla (ej. Isla 4):
   Usan la **Opción 2** de `./red/iniciar_emisor_receptor.sh` poniendo `Destino: 4`. El router lo reenviará hacia el siguiente salto de menor costo según Bellman-Ford.
