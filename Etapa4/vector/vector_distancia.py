import ctypes
from datetime import datetime
import functools
import json
import os
import signal
import socket
import sys
import threading
import time


def _con_candado(f):
    # Como cada conexion entra en un hilo aparte, usamos este candado para que
    # dos hilos no modifiquen la tabla al mismo tiempo y no se caiga el programa.
    @functools.wraps(f)
    def envuelta(self, *args, **kwargs):
        with self._candado:
            return f(self, *args, **kwargs)
    return envuelta


INF = 100                 # Si el costo llega a 100 o mas, se toma como inalcanzable
POISON_REVERSE = True     # Si llego a un destino por X, a X le digo costo -1 para evitar ciclos
TTL_INICIAL = 8           # Saltos maximos que puede dar un mensaje DATA
INTERVALO_ANUNCIO = 30    # Cada cuantos segundos se manda el ANNOUNCE a los vecinos
TOLERANCIA_SILENCIO = 90  # Si un vecino no habla en 90s (3 anuncios de 30s), lo damos por muerto
PUERTO_TCP = 5005         # Puerto acordado en el documento de diseno para hablar entre routers

# Resultados que devuelve el codigo en C (definidos en roa_tabla.h)
ROA_VALID = 0    # La IP y la isla coinciden con un ROA valido -> Se acepta
ROA_INVALID = 1  # Esa IP tiene un ROA pero pertenece a OTRA isla -> Se descarta
ROA_UNKNOWN = 2  # No hay ROA para esa IP (o ya vencio) -> Se acepta


class ValidadorPuntoE:
    """Puente minimo para llamar desde Python a la tabla ROA en C."""

    def __init__(self, ruta_so=None):
        # Buscamos el archivo compilado libpuntoE.so en la carpeta vecina Etapa4/puntoE/
        if ruta_so is None:
            ruta_so = os.path.abspath(
                os.path.join(os.path.dirname(__file__), "..", "puntoE", "libpuntoE.so")
            )
        self.lib = None
        if os.path.exists(ruta_so) and os.name != "nt":
            self.lib = ctypes.CDLL(ruta_so)
            # Indicamos los tipos de datos que reciben y devuelven las funciones en C de roa_tabla.c
            self.lib.roa_tabla_crear.argtypes = [ctypes.c_size_t]
            self.lib.roa_tabla_crear.restype = ctypes.c_void_p
            self.lib.roa_tabla_agregar.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_long]
            self.lib.roa_tabla_agregar.restype = ctypes.c_int
            self.lib.roa_almacen_publicar.argtypes = [ctypes.c_void_p, ctypes.c_long]
            self.lib.roa_almacen_consultar.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_long]
            self.lib.roa_almacen_consultar.restype = ctypes.c_int
            # Inicializamos los semaforos de la tabla local del Punto E
            self.lib.roa_almacen_iniciar()

    def cargar_roas(self, lista_roas, version=1):
        """Mete una lista de tuplas (ip, isla, vence) en la tabla en C."""
        if not self.lib:
            return
        # 1. Creamos la tabla nueva en C fuera del semaforo
        tabla = self.lib.roa_tabla_crear(max(1, len(lista_roas)))
        for ip, isla, vence in lista_roas:
            self.lib.roa_tabla_agregar(tabla, ip.encode(), int(isla), int(vence))
        # 2. La publicamos (hace el cambio de puntero protegido con el semaforo de escritura)
        self.lib.roa_almacen_publicar(tabla, int(version))

    def evaluar(self, ip, isla, ahora=None):
        """Consulta en la tabla con semaforos si 'isla' puede anunciar 'ip'."""
        if not self.lib or not ip:
            return ROA_UNKNOWN
        hora_actual = int(time.time()) if ahora is None else int(ahora)
        return self.lib.roa_almacen_consultar(ip.encode(), int(isla), hora_actual)

    def cerrar(self):
        """Destruye los semaforos y libera la memoria en C al terminar."""
        if self.lib:
            self.lib.roa_almacen_cerrar()
            self.lib = None


class Nodo:
    def __init__(self, mi_id, vecinos, ips_islas=None, validador_roa=None, puerto=PUERTO_TCP):
        self._candado = threading.RLock()
        self.id = int(mi_id)                                         # Nuestro numero de isla (1..5)
        self.vecinos = {int(k): int(v) for k, v in vecinos.items()}  # Vecinos directos y cuanto cuesta su enlace
        self.tabla = {}                                              # Guarda todos los caminos: tabla[destino][por_cual_vecino] = costo
        self.ultimo_oido = {}                                        # Guarda cuando fue la ultima vez que nos hablo cada vecino
        self.ips_islas = {int(k): str(v) for k, v in (ips_islas or {}).items()}  # Diccionario {id_isla: ip}
        self.ip_a_isla = {ip: isla for isla, ip in self.ips_islas.items()}       # Diccionario inverso {ip: id_isla}
        self.validador_roa = validador_roa                           # Objeto ValidadorPuntoE (conectado con el C del Punto E)
        self.puerto = int(puerto)                                    # Puerto TCP (5005 por defecto)
        self.nombres_islas = {}                                      # Nombre de cada equipo para la impresion

    @classmethod
    def desde_config(cls, ruta_json, validador_roa=None):
        """Carga nuestro id, los vecinos y las IPs desde el archivo json de configuracion."""
        with open(ruta_json, "r", encoding="utf-8") as f:
            datos = json.load(f)
        ips = {}
        nombres = {}
        if "islas" in datos:
            for k, v in datos["islas"].items():
                id_isla = int(k)
                if isinstance(v, dict):
                    ips[id_isla] = str(v.get("ip", ""))
                    nombres[id_isla] = str(v.get("equipo") or v.get("nombre", f"isla {id_isla}"))
                else:
                    ips[id_isla] = str(v)
        else:
            fuente = datos.get("ips") or datos.get("ips_islas") or {}
            ips = {int(k): str(v) for k, v in fuente.items()}

        vecinos = {}
        for k, v in datos["vecinos"].items():
            id_vec = int(k)
            if isinstance(v, dict):
                vecinos[id_vec] = int(v["costo"])
                if "ip" in v:
                    ips.setdefault(id_vec, v["ip"])
            else:
                vecinos[id_vec] = int(v)
        puerto = datos.get("puerto_tcp", PUERTO_TCP)
        nodo = cls(datos["mi_id"], vecinos, ips_islas=ips, validador_roa=validador_roa, puerto=puerto)
        nodo.nombres_islas = nombres
        return nodo

    def _ruta_valida_roa(self, isla):
        """Pregunta al Punto E si la isla tiene permiso de anunciar su IP.
        Devuelve True si es ROA_VALID o ROA_UNKNOWN, y False si es ROA_INVALID."""
        if self.validador_roa is None:
            return True
        ip = self.ips_islas.get(int(isla)) or f"172.16.{isla}0.10"
        if not ip:
            return True
        if self.validador_roa.evaluar(ip, int(isla)) == ROA_INVALID:
            print(f"[ROA] Ruta descartada: la isla {isla} no tiene permiso de anunciar la IP {ip}")
            return False
        return True

    # ---------------- consultas de la tabla ----------------
    @_con_candado
    def mejor(self, destino):
        """Busca el camino mas barato hacia el destino: devuelve (costo, siguiente_salto).
        Si hay empate en el costo, min() escoge automaticamente al vecino con menor id."""
        opciones = [(c, via) for via, c in self.tabla.get(int(destino), {}).items() if c < INF]
        return min(opciones) if opciones else (INF, None)

    @_con_candado
    def resumen(self):
        """Devuelve un diccionario con la mejor ruta para cada destino: {destino: (costo, salto)}."""
        return {d: self.mejor(d) for d in self.tabla}

    @_con_candado
    def siguiente_salto(self, destino):
        """Nos dice por cual vecino mandar un paquete DATA para llegar al destino.
        Si devuelve None es que no hay ruta (hay que descartar el paquete y anotarlo en bitacora)."""
        return self.mejor(int(destino))[1]

    # ---------------- cuando nos llegan rutas ----------------
    @_con_candado
    def recibir(self, vecino, vector, ahora=None):
        """Procesa las rutas que nos mando un vecino ({destino: costo}).
        Devuelve True si nuestra tabla de mejores rutas cambio.
        Nota de los costos: > 0 es distancia normal, -1 es inalcanzable, 0 es sin ruta (se ignora)."""
        vecino = int(vecino)
        if vecino not in self.vecinos:
            return False                                   # Si no es vecino nuestro, lo ignoramos
        vector = {int(k): int(v) for k, v in vector.items()}
        if vector.get(vecino) == -1:                       # Si el vecino se anuncia con -1 a si mismo,
            return self.vecino_caido(vecino)               # es su aviso de muerte porque se acaba de apagar

        # Revisamos con el Punto E que el vecino que nos habla no tenga ROA_INVALID
        if not self._ruta_valida_roa(vecino):
            return False

        antes = self.resumen()
        for d, fila in self.tabla.items():                 # Borramos las rutas viejas que venian por este vecino
            if d != vecino:                                # para quedarnos solo con lo nuevo que nos esta diciendo
                fila[vecino] = INF
        self.ultimo_oido[vecino] = time.monotonic() if ahora is None else ahora
        self.tabla.setdefault(vecino, {})[vecino] = self.vecinos[vecino]    # El enlace directo hacia ese vecino sigue vivo
        for destino, costo in vector.items():
            if destino in (self.id, vecino) or costo == 0:  # Ignoramos rutas hacia nosotros mismos, hacia el mismo vecino o con 0
                continue
            # Si nos anuncia una ruta activa (costo > 0), validamos su ROA con el modulo del Punto E
            if costo > 0 and not self._ruta_valida_roa(destino):
                continue
            # Ecuacion de Bellman-Ford: lo que me cuesta llegar al vecino + lo que le cuesta a el llegar al destino
            self.tabla.setdefault(destino, {})[vecino] = (
                INF if costo < 0 else min(INF, self.vecinos[vecino] + costo)
            )
        return self.resumen() != antes

    @_con_candado
    def vecino_caido(self, v):
        """Marca como inalcanzables todas las rutas que pasaban por el vecino v.
        Como guardamos los costos por cada vecino, si habia otro camino se usa de una vez."""
        v = int(v)
        antes = self.resumen()
        self.ultimo_oido.pop(v, None)
        for fila in self.tabla.values():
            fila[v] = INF
        return self.resumen() != antes

    @_con_candado
    def revisar_silencio(self, tolerancia=TOLERANCIA_SILENCIO, ahora=None):
        """Revisa si algun vecino lleva mas tiempo callado que la tolerancia (90s)."""
        ahora = time.monotonic() if ahora is None else ahora
        cambio = False
        for v, t in list(self.ultimo_oido.items()):
            if ahora - t > tolerancia:
                cambio |= self.vecino_caido(v)
        return cambio

    # ---------------- para armar lo que enviamos ----------------
    @_con_candado
    def vector_para(self, vecino):
        """Arma el diccionario {destino: costo} que le vamos a anunciar a ese vecino.
        Si la mejor ruta hacia un destino pasa por ese mismo vecino, le mandamos -1 (poison reverse)."""
        vecino = int(vecino)
        out = {}
        for d, (costo, via) in self.resumen().items():
            if d == vecino:
                continue
            out[d] = -1 if costo >= INF or (POISON_REVERSE and via == vecino) else costo
        return out

    @_con_candado
    def aviso_de_muerte(self):
        """Lo que mandamos a los vecinos cuando apagan el router con Ctrl+C (SIGINT/SIGTERM)."""
        return {self.id: -1}

    @_con_candado
    def imprimir(self):
        """Imprime la tabla completa con la hora actual (lo que pide la bitacora cuando hay cambios)."""
        hora = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        mi_nombre = self.nombres_islas.get(self.id, "")
        etiqueta_mi_isla = f"isla {self.id} ({mi_nombre})" if mi_nombre else f"isla {self.id}"
        print(f"[{hora}] Tabla de {etiqueta_mi_isla}:")
        for d, (costo, via) in sorted(self.resumen().items()):
            nom_d = self.nombres_islas.get(d, "")
            etiq_d = f"isla {d} ({nom_d})" if nom_d else f"isla {d}"
            if via:
                nom_via = self.nombres_islas.get(via, "")
                etiq_via = f"isla {via} ({nom_via})" if nom_via else f"isla {via}"
                print(f"  -> {etiq_d}: costo {costo}, por {etiq_via}")
            else:
                print(f"  -> {etiq_d}: INALCANZABLE")

    # ---------------- manejo de los mensajes de texto (ANNOUNCE y DATA) ----------------
    @_con_candado
    def armar_announce(self, vecino, muerte=False):
        """Convierte las rutas en texto con el formato 'ANNOUNCE|DESTINO|VALOR_DE_RUTA'."""
        vector = self.aviso_de_muerte() if muerte else self.vector_para(vecino)
        if not vector:
            # Si todavia no conocemos rutas, mandamos costo 0 como KeepAlive (el otro lo ignora como ruta pero sabe que estamos vivos)
            return f"ANNOUNCE|{self.id}|0"
        return "\n".join(f"ANNOUNCE|{dest}|{costo}" for dest, costo in sorted(vector.items()))

    @staticmethod
    def texto_a_vector(texto):
        """Pasa el texto 'ANNOUNCE|DESTINO|VALOR_DE_RUTA' a un diccionario {destino: costo} para meterlo en recibir()."""
        vector = {}
        for linea in texto.strip().splitlines():
            partes = [p.strip() for p in linea.split("|")]
            if len(partes) >= 3 and partes[0].upper() == "ANNOUNCE":
                vector[int(partes[1])] = int(partes[2])
        return vector

    @_con_candado
    def procesar_data(self, mensaje_data):
        """Recibe un paquete 'DATA|ISLA_ORIGEN|ISLA_DESTINO|TTL|DATOS' y decide que hacer:
        - Si ya llego a nuestra isla -> ("ENTREGAR", None, datos)
        - Si el TTL se acabo o no hay ruta -> ("DESCARTAR", None, motivo)
        - Si toca reenviarlo -> ("REENVIAR", siguiente_salto, mensaje_con_ttl_restado)
        """
        partes = [p.strip() for p in mensaje_data.split("|", 4)]
        if len(partes) < 5 or partes[0].upper() != "DATA":
            return ("DESCARTAR", None, "Formato DATA invalido")

        origen = int(partes[1])
        destino = int(partes[2])
        ttl = int(partes[3])
        datos = partes[4]

        # 1. Si el mensaje era para nosotros, lo entregamos y no se reenvia
        if destino == self.id:
            return ("ENTREGAR", None, datos)

        # 2. Le restamos 1 al TTL; si llega a 0 lo descartamos
        ttl -= 1
        if ttl <= 0:
            return ("DESCARTAR", None, f"TTL agotado (origen={origen}, destino={destino})")

        # 3. Buscamos cual es el siguiente salto en la tabla
        salto = self.siguiente_salto(destino)
        if salto is None:
            return ("DESCARTAR", None, f"Sin ruta hacia isla {destino} (origen={origen})")

        # 4. Si todo bien, armamos el paquete con el nuevo TTL para mandarlo al siguiente salto
        nuevo_msg = f"DATA|{origen}|{destino}|{ttl}|{datos}"
        return ("REENVIAR", salto, nuevo_msg)

    # ---------------- envio y recepcion por red TCP (puerto 5005) ----------------
    def enviar_tcp(self, id_vecino, texto):
        """Abre conexion TCP al puerto 5005 del vecino, envia el mensaje y cierra."""
        ip = self.ips_islas.get(int(id_vecino))
        if not ip:
            return False
        try:
            with socket.create_connection((ip, self.puerto), timeout=5) as s:
                s.sendall((texto.strip() + "\n").encode("utf-8"))
            return True
        except OSError:
            return False

    def anunciar_a_vecinos(self, muerte=False):
        """Envia el ANNOUNCE correspondiente a cada uno de los vecinos configurados."""
        for v in list(self.vecinos.keys()):
            self.enviar_tcp(v, self.armar_announce(v, muerte=muerte))

    def _atender_conexion(self, conn, addr):
        """Procesa un mensaje ANNOUNCE o DATA que llego por TCP desde otro equipo."""
        try:
            datos = conn.recv(4096).decode("utf-8", errors="replace").strip()
            if not datos:
                return
            if datos.upper().startswith("ANNOUNCE"):
                vecino = self.ip_a_isla.get(addr[0])
                vector = self.texto_a_vector(datos)
                if vecino is not None and self.recibir(vecino, vector):
                    self.imprimir()
            elif datos.upper().startswith("DATA"):
                accion, salto, info = self.procesar_data(datos)
                if accion == "ENTREGAR":
                    print(f"[DATA RECIBIDO] {info}")
                elif accion == "REENVIAR":
                    self.enviar_tcp(salto, info)
                else:
                    print(f"[DATA DESCARTADO] {info}")
        finally:
            conn.close()

    def ejecutar_en_red(self):
        """Levanta el socket TCP 5005, manda ANNOUNCE cada 30s y revisa silencios."""
        servidor = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        servidor.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        servidor.bind(("0.0.0.0", self.puerto))
        servidor.listen(16)

        def cerrar_ordenado(_sig, _frame):
            print("\nCerrando nodo y enviando aviso de muerte (-1) a los vecinos...")
            self.anunciar_a_vecinos(muerte=True)
            servidor.close()
            sys.exit(0)

        signal.signal(signal.SIGINT, cerrar_ordenado)
        signal.signal(signal.SIGTERM, cerrar_ordenado)

        def ciclo_periodico():
            while True:
                self.anunciar_a_vecinos()
                if self.revisar_silencio():
                    self.imprimir()
                time.sleep(INTERVALO_ANUNCIO)

        threading.Thread(target=ciclo_periodico, daemon=True).start()
        print(f"Nodo de isla {self.id} escuchando en TCP puerto {self.puerto}...")
        while True:
            conn, addr = servidor.accept()
            threading.Thread(target=self._atender_conexion, args=(conn, addr), daemon=True).start()


# ---------------- Prueba rapida con las 5 islas de la clase ----------------
if __name__ == "__main__":
    ruta_cfg = os.path.join(os.path.dirname(__file__), "config_koopatroopas.json")
    validador = ValidadorPuntoE()
    nodo_koopa = (
        Nodo.desde_config(ruta_cfg, validador_roa=validador)
        if os.path.exists(ruta_cfg)
        else Nodo(1, {3: 5, 4: 2, 5: 2}, validador_roa=validador)
    )

    # Si se corre con: python3 vector_distancia.py --red, levanta el servidor TCP 5005 real
    if "--red" in sys.argv:
        nodo_koopa.ejecutar_en_red()

    # Si se corre normal (python3 vector_distancia.py), hace la simulacion de las 5 islas
    topologia = {
        1: nodo_koopa.vecinos,
        2: {3: 2, 5: 1},        # SHELL
        3: {1: 5, 2: 2, 4: 1},  # 4bits
        4: {1: 2, 3: 1, 5: 5},  # Pirateados
        5: {1: 2, 2: 1, 4: 5},  # Pi de Frambuesa
    }

    nombres_sim = {
        1: "Koopatroopas",
        2: "SHELL",
        3: "4bits",
        4: "Pirateados",
        5: "Pi de Frambuesa",
    }

    nodos = {}
    for id_isla, vecinos in topologia.items():
        n = (nodo_koopa if id_isla == 1 else Nodo(id_isla, vecinos))
        n.nombres_islas = nombres_sim
        nodos[id_isla] = n

    print("=== 1. Convergencia inicial de la red (5 equipos) ===")
    for _ in range(4):
        for id_emisor, nodo_emisor in nodos.items():
            for id_vecino in nodo_emisor.vecinos:
                msg_announce = nodo_emisor.armar_announce(id_vecino)
                vec_recibido = Nodo.texto_a_vector(msg_announce)
                if nodos[id_vecino].recibir(id_emisor, vec_recibido) and id_vecino == 1:
                    nodos[1].imprimir()

    print("\n=== 2. Prueba de reenvio DATA desde Koopatroopas (1) hacia SHELL (2) ===")
    paquete = f"DATA|1|2|{TTL_INICIAL}|Hola SHELL desde Koopatroopas"
    actual = 1
    while True:
        accion, salto, info = nodos[actual].procesar_data(paquete)
        print(f"  Isla {actual} -> accion={accion}, siguiente_salto={salto}, info='{info}'")
        if accion != "REENVIAR":
            break
        paquete = info
        actual = salto

    print("\n=== 3. Caida de Pirateados (isla 4) con aviso de muerte ===")
    for v in nodos[4].vecinos:
        msg_muerte = nodos[4].armar_announce(v, muerte=True)
        nodos[v].recibir(4, Nodo.texto_a_vector(msg_muerte))
    while True:
        cambio = False
        for id_emisor in (1, 2, 3, 5):
            for id_vecino in nodos[id_emisor].vecinos:
                if id_vecino != 4:
                    msg = nodos[id_emisor].armar_announce(id_vecino)
                    cambio |= nodos[id_vecino].recibir(id_emisor, Nodo.texto_a_vector(msg))
        if not cambio:
            break
    nodos[1].imprimir()
    validador.cerrar()
