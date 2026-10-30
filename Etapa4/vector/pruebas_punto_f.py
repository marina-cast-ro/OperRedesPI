import os
import unittest

from vector_distancia import INF, TTL_INICIAL, Nodo

# Costos de conexion acordados
TOPOLOGIA_CLASE = {
    1: {3: 5, 4: 2, 5: 2},  # Koopatroopas
    2: {3: 2, 5: 1},        # SHELL
    3: {1: 5, 2: 2, 4: 1},  # 4bits
    4: {1: 2, 3: 1, 5: 5},  # Pirateados
    5: {1: 2, 2: 1, 4: 5},  # Pi de Frambuesa
}


class PruebasPuntoF(unittest.TestCase):
    def _converger_red(self):
        # Pone a los 5 nodos a pasarse sus vectores hasta que las tablas queden listas
        nodos = {i: Nodo(i, vec) for i, vec in TOPOLOGIA_CLASE.items()}
        for _ in range(5):
            for emisor_id, nodo_emisor in nodos.items():
                for vecino_id in nodo_emisor.vecinos:
                    msg = nodo_emisor.armar_announce(vecino_id)
                    nodos[vecino_id].recibir(emisor_id, Nodo.texto_a_vector(msg))
        return nodos

    def test_carga_config_koopatroopas(self):
        # Revisa que lea bien el json de Koopatroopas
        ruta = os.path.join(os.path.dirname(__file__), "config_koopatroopas.json")
        koopa = Nodo.desde_config(ruta)
        self.assertEqual(koopa.id, 1)
        self.assertEqual(koopa.vecinos, {3: 5, 4: 2, 5: 2})

    def test_ruta_mas_corta_5_equipos(self):
        # Revisa que Koopatroopas calcule bien los caminos mas cortos hacia todos
        nodos = self._converger_red()
        resumen_koopa = nodos[1].resumen()
        self.assertEqual(resumen_koopa[2], (3, 5))  # A SHELL (2) cuesta 3 yendo por Frambuesa (5)
        self.assertEqual(resumen_koopa[3], (3, 4))  # A 4bits (3) cuesta 3 yendo por Pirateados (4), mejor que ir directo (5)
        self.assertEqual(resumen_koopa[4], (2, 4))  # A Pirateados (4) cuesta 2 directo
        self.assertEqual(resumen_koopa[5], (2, 5))  # A Frambuesa (5) cuesta 2 directo

    def test_poison_reverse_y_desempate(self):
        nodos = self._converger_red()
        # Como Koopatroopas llega a 4bits (3) pasando por Pirateados (4), a Pirateados le anuncia -1
        self.assertEqual(nodos[1].vector_para(4)[3], -1)
        self.assertEqual(nodos[1].vector_para(5)[3], 3)

        # Si se cae Pirateados (4), para ir a 4bits (3) empatan con costo 5 el enlace directo (3) y Frambuesa (5).
        # Por desempate tiene que ganar el menor id (isla 3)
        nodos[1].vecino_caido(4)
        self.assertEqual(nodos[1].mejor(3), (5, 3))

    def test_aviso_muerte_y_silencio(self):
        koopa = Nodo(1, TOPOLOGIA_CLASE[1])
        koopa.recibir(4, {3: 1}, ahora=10.0)
        self.assertEqual(koopa.mejor(3), (3, 4))

        # Si pasan mas de 90s sin saber nada del vecino 4, se da por caido
        self.assertFalse(koopa.revisar_silencio(tolerancia=90, ahora=50.0))
        self.assertTrue(koopa.revisar_silencio(tolerancia=90, ahora=105.0))
        self.assertEqual(koopa.mejor(3), (INF, None))

        # Si el vecino vuelve y luego manda aviso de muerte {4: -1}, tambien se da de baja de una vez
        koopa.recibir(4, {3: 1}, ahora=110.0)
        self.assertTrue(koopa.recibir(4, {4: -1}, ahora=112.0))
        self.assertEqual(koopa.mejor(4), (INF, None))

    def test_procesar_data_y_ttl(self):
        nodos = self._converger_red()
        # Mandamos un mensaje de Koopatroopas (1) a SHELL (2), tiene que saltar por Frambuesa (5) e ir bajando el TTL
        accion1, salto1, trama1 = nodos[1].procesar_data(f"DATA|1|2|{TTL_INICIAL}|Hola")
        self.assertEqual((accion1, salto1, trama1), ("REENVIAR", 5, "DATA|1|2|7|Hola"))

        accion2, salto2, trama2 = nodos[5].procesar_data(trama1)
        self.assertEqual((accion2, salto2, trama2), ("REENVIAR", 2, "DATA|1|2|6|Hola"))

        accion3, salto3, datos = nodos[2].procesar_data(trama2)
        self.assertEqual((accion3, salto3, datos), ("ENTREGAR", None, "Hola"))

        # Si viene con TTL=1 y todavia no ha llegado al destino, al restarle 1 queda en 0 y se descarta
        accion_ttl, salto_ttl, _ = nodos[1].procesar_data("DATA|1|2|1|Expira")
        self.assertEqual((accion_ttl, salto_ttl), ("DESCARTAR", None))


if __name__ == "__main__":
    unittest.main(verbosity=2)
