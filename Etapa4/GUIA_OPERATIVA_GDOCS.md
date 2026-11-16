# Guía Operativa de Laboratorio — Isla 2 (Koopatroopas)

*Operación de Redes — Etapa 4 | Demo y Validación Oficial*

---

## 1. Conexiones Físicas y Distribución de las 3 PCs

En el laboratorio se utilizan **tres computadoras físicas independientes** conectadas al Switch Cisco 2960 (`SW-IN-Koopatroopa`):

### PC 1: Enrutador Linux

* **Puerto en Switch:** `Fa0/3` (Troncal 802.1Q)
* **IPs de Subinterfaces:**
  * LAN Local: `172.16.20.1/24` (`eth0.20`, Gateway de la isla)
  * Tránsitos: `eth0.112`, `eth0.124`, `eth0.125`
* **Servicio:** Enrutamiento por software en puerto TCP `5005`
* **Regla estricta:** `net.ipv4.ip_forward = 0` (kernel no enruta)

### PC 2: Servidor Repositorio ROA

* **Puerto en Switch:** `Fa0/9` (Acceso VLAN 20)
* **Configuración IP Fija:**
  * Dirección IP: `172.16.20.10`
  * Máscara: `255.255.255.0` (`/24`)
  * Gateway: `172.16.20.1`
* **Servicio:** Repositorio RPKI en puerto TCP `9000` (almacena en `disco.bin`)

### PC 3: Host LAN (Emisor y Receptor)

* **Puerto en Switch:** `Fa0/2` (Acceso VLAN 20)
* **Configuración IP:** Dinámica por DHCP desde el switch (`172.16.20.100` a `.199`)
* **Servicios:**
  * `host_emisor`: Publica ROAs hacia el repositorio de PC 2
  * `host_receptor`: Escucha mensajes de datos en puerto TCP `5005`
  * `host_datos`: Envía paquetes de datos hacia el enrutador de PC 1

---

### Enlaces hacia Vecinos Oficiales (Puerto `Fa0/1` Troncal)

* **Isla 1 — Pirateados (Costo 2):**
  VLAN `112` | Subinterfaz `eth0.112` (`172.30.12.2/24`) | IP Vecino: `172.30.12.1`
* **Isla 4 — 4bits (Costo 5):**
  VLAN `124` | Subinterfaz `eth0.124` (`172.30.24.1/24`) | IP Vecino: `172.30.24.2`
* **Isla 5 — Pi de Frambuesa (Costo 2):**
  VLAN `125` | Subinterfaz `eth0.125` (`172.30.25.1/24`) | IP Vecino: `172.30.25.2`

---

## 2. Secuencia de Arranque Paso a Paso

> **Orden de encendido:** Primero el Repositorio (PC 2), luego el Host Emisor (PC 3), y finalmente el Enrutador (PC 1) para que descargue el ROA oficial de inmediato al sincronizar.

### PASO 1: PC 2 — Levantar el Servidor Repositorio ROA

*Conectar cable de red al puerto **`Fa0/9`** del Switch.*

1. **Configurar IP Estática:**
   Asignar en la tarjeta de red la IP `172.16.20.10/24` con gateway `172.16.20.1`.

2. **Ejecutar el Servidor:**

   ```bash
   cd Etapa4/repositorio
   ./build/repoServer ../certificados/trusted-roots.pem 9000
   ```

   *(Salida esperada: `[REPO] Servidor escuchando en puerto 9000`).*

---

### PASO 2: PC 3 — Host Emisor y Receptor de la LAN

*Conectar cable de red al puerto **`Fa0/2`** del Switch.*

1. **Verificar IP asignada por DHCP desde el Switch:**

   ```bash
   ip addr show
   ```

   *(Anotar la IP obtenida en la VLAN 20, por ejemplo: `172.16.20.101`).*

2. **Iniciar Emisor y Receptor simultáneamente:**
   * **Opción Principal (Recomendada con script asistente):**

     ```bash
     ./red/iniciar_emisor_receptor.sh
     ```

     Elegir la **Opción 4** (*Publicar ROA y levantar Receptor TCP*):
     * Publica el ROA oficial en segundo plano con `--loop` (republica cada 60s y renueva si faltan <5min).
     * Levanta de inmediato el `host_receptor` en el puerto TCP `5005`.

   * **Opción Secundaria (Comandos manuales en dos terminales):**
     * Terminal 1: `cd Etapa4/host && ./host_emisor 172.16.20.10 9000 201 2 172.16.20.10 ../certificados/nodo.key ../certificados/nodo.crt 86400 --loop`
     * Terminal 2: `cd Etapa4/host && ./host_receptor 5005 mensajes_recibidos.txt`

---

### PASO 3: PC 1 — Enrutador Linux Koopatroopas

*Conectar cable de red al puerto **`Fa0/3`** del Switch.*

1. **Configurar subinterfaces VLAN y kernel:**
   * **Opción Principal:**

     ```bash
     sudo red/configurar_enrutador.sh
     ```

     *(Crea automáticamente `eth0.20`, `eth0.112`, `eth0.124`, `eth0.125` y pone `net.ipv4.ip_forward=0`).*

2. **Sincronizar IP de la PC 3 en la configuración:**
   Abrir `Etapa4/vector/config_koopatroopas.json` y verificar:
   * `"servidor_local"`: Debe tener la IP DHCP anotada en la **PC 3** (ejemplo: `"172.16.20.101"`).
   * `"repositorio"."ip"`: Debe ser `"172.16.20.10"`.

3. **Iniciar el Enrutador:**

   ```bash
   cd Etapa4/vector
   ./enrutador --red config_koopatroopas.json
   ```

   *(Descarga ROAs desde PC 2, abre TCP 5005 y comienza anuncios cada 30s con validación RPKI).*

---

## 3. Pruebas de Transmisión de Datos y Métricas MMU

### A. Enviar mensajes DATA (Desde la PC 3)

* **Opción Principal:** Ejecutar `./red/iniciar_emisor_receptor.sh` -> **Opción 2** (ingresar isla destino y mensaje).
* **Opción Secundaria (Comandos directos en terminal):**

  ```bash
  # Prueba 1: Loopback local a nuestra propia isla (Isla 2):
  ./Etapa4/host/host_datos 172.16.20.1 5005 2 2 "Prueba interna Koopas"

  # Prueba 2: Enviar a Isla 5 (Pi de Frambuesa):
  ./Etapa4/host/host_datos 172.16.20.1 5005 2 5 "Saludos a Isla 5"

  # Prueba 3: Enviar a Isla 1 (Pirateados):
  ./Etapa4/host/host_datos 172.16.20.1 5005 2 1 "Saludos a Pirateados"
  ```

  *(El mensaje se imprime en tiempo real en la pantalla de `host_receptor` de la PC destino).*

### B. Visualizar Reporte del MMU (En la PC 1)

Al terminar las pruebas de tráfico o para mostrar la métrica al evaluador:

* Presionar **`Ctrl + C`** en la terminal del `enrutador` (PC 1).
* Muestra el consolidado de memoria compartida: paquetes recibidos, reenviados a vecinos, entregados al host local y descartados por TTL o falta de ruta.

---

## 4. Demostración de Seguridad RPKI (Demo Evaluador)

### Opción Principal (Script oficial automatizado)

Ejecutar desde la **PC 3** o laptop conectada a la red:

```bash
./red/probar_ataques.sh --lab --repo 172.16.20.10:9000 --router 172.16.20.1:5005
```

### Menú de Ataques Disponibles

* **Opción a (Prefix Hijacking en Vector de Distancias):**
  Envía un `ANNOUNCE|5|5:1` falsificado a PC 1.
  * *Resultado:* El enrutador consulta su tabla ROA, detecta origen inválido y marca la ruta como **`INVALID`**, descartándola.
* **Opción b (Suplantación de Origen en `PUT`):**
  Intenta registrar en PC 2 un ROA para la IP `172.16.50.10` de Isla 5 usando el cert de Koopas (`AS2-201`).
  * *Resultado:* El repositorio rechaza con **`ERR_PERMISO`**.
* **Opción c (Certificado de CA Falsa):**
  Envía `HELLO|OWNER|999|<b64>` con una CA pirata temporal hacia PC 2.
  * *Resultado:* Falla la cadena contra `trusted-roots.pem` y el repo rechaza con **`ERR_PERMISO`**.
* **Opción d (`DELETE` no Autorizado):**
  Firma una petición de borrado sobre un ROA ajeno hacia PC 2.
  * *Resultado:* El repositorio rechaza con **`ERR_PERMISO`**.

---

## 5. Respuestas Rápidas ante Imprevistos (Troubleshooting)

* **Problema: PC 3 no obtiene IP por DHCP**
  * *Causa:* Cable conectado a un puerto incorrecto en el switch.
  * *Solución:* Asegurarse de conectar en **`Fa0/2`**. Si el DHCP no responde, asignar manualmente `172.16.20.101/24` con gateway `172.16.20.1`.

* **Problema: El enrutador (PC 1) no entrega mensajes a PC 3**
  * *Causa:* La IP de PC 3 cambió o está desactualizada en el archivo de configuración.
  * *Solución:* Revisar `ip addr` en PC 3 y corregir `"servidor_local"` en `Etapa4/vector/config_koopatroopas.json`.

* **Problema: No hay conexión con el Repositorio (PC 2, puerto 9000)**
  * *Causa:* IP fija mal aplicada o firewall de Ubuntu activo.
  * *Solución:* En PC 2: `sudo ip addr add 172.16.20.10/24 dev eth0` y desactivar firewall: `sudo ufw disable`.

* **Problema: Falta la llave privada `nodo.key` en PC 3**
  * *Solución:* Regenerar y firmar en un comando:

    ```bash
    cd Etapa4/certificados && ./pedir-certificado.sh 2 201 . && ./firmar-certificado.sh nodo-AS2-201.csr . && mv nodo-AS2-201.key nodo.key && mv nodo-AS2-201.crt nodo.crt
    ```

* **Problema: El grupo 4bits entrega su CA renovada**
  * *Solución:* Copiar el archivo nuevo a `Etapa4/certificados/CRT/ca-4bits.crt` y regenerar truststore:

    ```bash
    cd Etapa4/certificados && ./actualizar-truststore.sh CRT fingerprints.txt .
    ```
