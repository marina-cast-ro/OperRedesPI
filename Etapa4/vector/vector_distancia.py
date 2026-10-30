from datetime import datetime
import functools
import json
import os
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
TOLERANCIA_SILENCIO = 90  # Si un vecino no habla en 90s (3 anuncios de 30s), lo damos por muerto


class Nodo:
    def __init__(self, mi_id, vecinos):
        self._candado = threading.RLock()
        self.id = int(mi_id)                                         # Nuestro numero de isla (1..5)
        self.vecinos = {int(k): int(v) for k, v in vecinos.items()}  # Vecinos directos y cuanto cuesta su enlace
        self.tabla = {}                                              # Guarda todos los caminos: tabla[destino][por_cual_vecino] = costo
        self.ultimo_oido = {}                                        # Guarda cuando fue la ultima vez que nos hablo cada vecino

    @classmethod
    def desde_config(cls, ruta_json):
        """Carga nuestro id y los vecinos desde el archivo json de configuracion."""
        with open(ruta_json, "r", encoding="utf-8") as f:
            datos = json.load(f)
        vecinos = {
            int(k): (int(v["costo"]) if isinstance(v, dict) else int(v))
            for k, v in datos["vecinos"].items()
        }
        return cls(datos["mi_id"], vecinos)

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
        # //TODO: Conectar con Etapa 2 para guardar este diccionario en la memoria virtual cada vez que cambie la tabla
        return {d: self.mejor(d) for d in self.tabla}

    @_con_candado
    def siguiente_salto(self, destino):
        """Nos dice por cual vecino mandar un paquete DATA para llegar al destino.
        Si devuelve None es que no hay ruta (hay que descartar el paquete y anotarlo en bitacora)."""
        # //TODO: Si en Etapa 2 la consulta de rutas se lee directo de la memoria virtual, llamar a esa funcion aqui
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

        # //TODO: Conectar con Punto E (Daniel - Validacion de ROAs) para filtrar rutas INVALID antes de meterlas a la tabla

        antes = self.resumen()
        for d, fila in self.tabla.items():                 # Borramos las rutas viejas que venian por este vecino
            if d != vecino:                                # para quedarnos solo con lo nuevo que nos esta diciendo
                fila[vecino] = INF
        self.ultimo_oido[vecino] = time.monotonic() if ahora is None else ahora
        self.tabla.setdefault(vecino, {})[vecino] = self.vecinos[vecino]    # El enlace directo hacia ese vecino sigue vivo
        for destino, costo in vector.items():
            if destino in (self.id, vecino) or costo == 0:  # Ignoramos rutas hacia nosotros mismos, hacia el mismo vecino o con 0
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
        # //TODO: Llamar a esta funcion cada pocos segundos desde el hilo/timer del enrutador
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
        # //TODO: Enviar este aviso por socket TCP a los vecinos cuando se cierre el programa
        return {self.id: -1}

    @_con_candado
    def imprimir(self):
        """Imprime la tabla completa con la hora actual (lo que pide la bitacora cuando hay cambios)."""
        # //TODO: Llamar cada vez que recibir(), vecino_caido() o revisar_silencio() devuelvan True
        hora = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        print(f"[{hora}] Tabla de isla {self.id}:")
        for d, (costo, via) in sorted(self.resumen().items()):
            print(f"  -> isla {d}: " + (f"costo {costo}, por isla {via}" if via else "INALCANZABLE"))

    # ---------------- manejo de los mensajes de texto (ANNOUNCE y DATA) ----------------
    @_con_candado
    def armar_announce(self, vecino, muerte=False):
        """Convierte las rutas en texto con el formato 'ANNOUNCE|DESTINO|VALOR_DE_RUTA'."""
        # //TODO: Enviar este texto por el socket TCP (puerto 5005) al iniciar y cada 30 segundos
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
        # //TODO: Conectar con el socket receptor de Etapa 2 cuando llegue un mensaje tipo DATA
        partes = [p.strip() for p in mensaje_data.split("|", 4)]
        if len(partes) < 5 or partes[0].upper() != "DATA":
            return ("DESCARTAR", None, "Formato DATA invalido")

        origen = int(partes[1])
        destino = int(partes[2])
        ttl = int(partes[3])
        datos = partes[4]

        # Si el mensaje era para nosotros, lo entregamos y no se reenvia
        if destino == self.id:
            return ("ENTREGAR", None, datos)

        # Le restamos 1 al TTL y si llega a 0 lo descartamos
        ttl -= 1
        if ttl <= 0:
            return ("DESCARTAR", None, f"TTL agotado (origen={origen}, destino={destino})")

        # Buscamos cual es el siguiente salto en la tabla
        salto = self.siguiente_salto(destino)
        if salto is None:
            return ("DESCARTAR", None, f"Sin ruta hacia isla {destino} (origen={origen})")

        # Armamos el paquete con el nuevo TTL para mandarlo al siguiente salto
        nuevo_msg = f"DATA|{origen}|{destino}|{ttl}|{datos}"
        return ("REENVIAR", salto, nuevo_msg)


# ---------------- Prueba rapida con las 5 islas de la clase ----------------
if __name__ == "__main__":
    # 1=Koopatroopas, 2=SHELL, 3=4bits, 4=Pirateados, 5=Pi de Frambuesa
    ruta_cfg = os.path.join(os.path.dirname(__file__), "config_koopatroopas.json")
    topologia = {
        1: Nodo.desde_config(ruta_cfg).vecinos if os.path.exists(ruta_cfg) else {3: 5, 4: 2, 5: 2},
        2: {3: 2, 5: 1},        # SHELL
        3: {1: 5, 2: 2, 4: 1},  # 4bits
        4: {1: 2, 3: 1, 5: 5},  # Pirateados
        5: {1: 2, 2: 1, 4: 5},  # Pi de Frambuesa
    }

    nodos = {id_isla: Nodo(id_isla, vecinos) for id_isla, vecinos in topologia.items()}

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
