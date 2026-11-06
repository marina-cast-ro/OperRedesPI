# Guia de Red y Pruebas en el Laboratorio (Isla 2 - Koopatroopas)

---

## 1. Que computadora es cada una y que ejecuta

En el laboratorio se usan **3 computadoras** conectadas a nuestro switch Cisco (`SW-IN-Koopatroopa`):

| Computadora / Rol | Puerto Switch | Como obtiene la IP | Que programa corre aqui? |
| :--- | :---: | :--- | :--- |
| **Compu 1: Host (Emisor y Receptor)** | `Fa0/2` | **Automatica por DHCP** (El switch se la da sola) | • **Emisor:** `./host_emisor ...`<br>• **Receptor:** Programa que escucha mensajes |
| **Compu 2: Enrutador (Router)** | `Fa0/3` | Con el script automatizado | • **Enrutador:** `./enrutador --red config_koopatroopas.json 172.16.20.10` |
| **Compu 3: Repositorio de ROAs** | `Fa0/9` | Fija: `172.16.20.10` | • **Servidor ROA:** `./repoServer trusted-roots.pem 9000` |
| **Switch Central del Aula** | `Fa0/1` | - | Cable largo hacia el switch central del profesor |

---

## 2. Preparacion de cada computadora

### Compu 1: Host Emisor y Receptor (Cable en Fa0/2)
**No hay que configurar nada de red ni usar comandos.**  
El switch tiene servidor DHCP activo. Solo conectas el cable al puerto `Fa0/2` y la compu recibe su IP sola (ejemplo: `172.16.20.100`).

Para ver que IP te dio el switch:
```
hostname -I
```

---

### Compu 2: Enrutador (Cable en Fa0/3)
Como el enrutador debe hablar con 4 redes a la vez por el mismo cable (nuestra red local y 3 islas vecinas), se corre el script que ya dejamos listo:

```
sudo ./red/configurar_enrutador.sh
```
*(Si tu tarjeta de red no se llama eth0 sino por ejemplo enp3s0: `sudo ./red/configurar_enrutador.sh enp3s0`)*

**Que hace este script en 1 segundo?:**
* Habilita el reenvio de paquetes (modo router en Linux).
* Crea las 4 interfaces virtuales con sus IPs (`172.16.20.1` para la red local, y los enlaces hacia Isla 1, Isla 4 e Isla 5).

---

### Compu 3: Repositorio (Cable en Fa0/9)
Poner la IP fija `172.16.20.10` (que es la que esperan el emisor y el enrutador):

```
sudo ip addr add 172.16.20.10/24 dev eth0
sudo ip link set dev eth0 up
sudo ip route add default via 172.16.20.1
```

---

## 3. Pruebas paso a paso para saber que funciona

### Paso 1: Probar conexion local con ping
Desde la **Compu 1 (Host)**, probar que llega al switch, al enrutador y al repositorio:
```
ping -c 3 172.16.20.2
ping -c 3 172.16.20.1
ping -c 3 172.16.20.10
```
Si los 3 responden: la red de nuestra mesa esta lista.

---

### Paso 2: Iniciar y probar el Repositorio
En la **Compu 3 (Repositorio)**:
```
cd Etapa4/repositorio
./repoServer trusted-roots.pem 9000
```

Desde la **Compu 1 (Host)**, probar que el puerto 9000 esta escuchando:
```
nc -zv 172.16.20.10 9000
```
*(Debe decir que la conexion fue exitosa)*

---

### Paso 3: Publicar el ROA desde el Emisor
En la **Compu 1 (Host)**:
```
cd Etapa4/host
./host_emisor 172.16.20.10 9000 201 2 172.16.20.10 ../certificados/nodo.key ../certificados/nodo.crt 86400
```
*(Debe decir: `[OK] ROA publicado correctamente en el repositorio`)*

---

### Paso 4: Levantar el Enrutador
En la **Compu 2 (Enrutador)**:
```
cd Etapa4/vector
./enrutador --red config_koopatroopas.json 172.16.20.10
```
*(Debe decir que el Punto E sincronizo el ROA con el repositorio y mostrar la tabla de rutas)*

---

### Paso 5: Probar conexion con las demas islas del aula
Conectar el cable de `Fa0/1` al switch central del profesor.  
Desde la **Compu 2 (Enrutador)**, probar ping a los vecinos:
```
ping -c 3 172.30.12.1
ping -c 3 172.30.24.2
ping -c 3 172.30.25.2
```
Si responden: nuestra isla ya esta conectada con todo el salon.
