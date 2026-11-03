#!/usr/bin/env python3
# Pruebas del servidor del repositorio. Se corre con make test
# La CA y los nodos de prueba se crean en una carpeta temporal

import base64
import copy
import socket
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
CERTDIR = HERE.parent / "certificados"
SERVER = HERE / "build" / "repoServer"
PORT = 9100
ISLAND = 65002
NOW = int(time.time())
ONE_DAY = 86400

failures = 0


def check(name, got, expected):
    global failures
    if got == expected:
        print(f"[ok]    {name}")
    else:
        failures += 1
        print(f"[FALLA] {name}\n        esperaba: {expected!r}\n        recibió:  {got!r}")


def openssl(*args, data=None):
    return subprocess.run(["openssl", *args], input=data, check=True, capture_output=True).stdout


def create_ca(folder, name):
    key = folder / f"{name}.key"
    crt = folder / f"{name}.crt"
    openssl("genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", str(key))
    openssl("req", "-new", "-x509", "-key", str(key), "-config", str(CERTDIR / "ca-ext.cnf"),
            "-subj", f"/CN={name}/O={name}/C=CR", "-sha256", "-days", "1825", "-out", str(crt))
    return key, crt


class Node:
    """Un nodo emisor/receptor con su llave y su certificado firmado por una CA."""

    def __init__(self, folder, ca, island, num, cn_num=None):
        self.island = island
        self.num = num
        name = f"AS{island}-{cn_num if cn_num is not None else num}"
        self.key = folder / f"{name}.key"
        csr = folder / f"{name}.csr"
        crt = folder / f"{name}.crt"
        openssl("genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", str(self.key))
        openssl("req", "-new", "-key", str(self.key), "-subj", f"/CN={name}/O=Prueba/C=CR", "-out", str(csr))
        openssl("x509", "-req", "-in", str(csr), "-CA", str(ca[1]), "-CAkey", str(ca[0]), "-CAcreateserial",
                "-days", "365", "-sha256", "-extfile", str(CERTDIR / "nodo-ext.cnf"), "-out", str(crt))
        der = openssl("x509", "-in", str(crt), "-outform", "DER")
        self.cert_b64 = base64.b64encode(der).decode()

    def sign(self, text):
        signature = openssl("dgst", "-sha256", "-sign", str(self.key), data=text.encode())
        return base64.b64encode(signature).decode()

    def roa(self, ip, island=None, num=None, issued=NOW - 60, expires=NOW + ONE_DAY):
        island = self.island if island is None else island
        num = self.num if num is None else num
        text = f"ROA|{ip}|{island}|{num}|{issued}|{expires}"
        return f"{text}|{self.sign(text)}"


class Connection:
    def __init__(self):
        self.sock = socket.create_connection(("127.0.0.1", PORT), timeout=15)
        self.buffer = b""

    def send(self, text):
        self.sock.sendall(text.encode())

    def line(self):
        while b"\n" not in self.buffer:
            chunk = self.sock.recv(65536)
            if not chunk:
                return None
            self.buffer += chunk
        text, self.buffer = self.buffer.split(b"\n", 1)
        return text.decode()

    def request(self, text):
        self.send(text + "\n")
        return self.line()

    def closed_by_server(self):
        return self.line() is None

    def close(self):
        self.sock.close()


def router(command):
    """HELLO de enrutador + un comando. Retorna la primera línea de la respuesta."""
    connection = Connection()
    connection.request("HELLO|ENRUTADOR|1")
    answer = connection.request(command)
    connection.close()
    return answer


def router_list():
    """LIST completo: (encabezado, líneas de ROA)."""
    connection = Connection()
    connection.request("HELLO|ENRUTADOR|1")
    header = connection.request("LIST")
    count = int(header.split("|")[2])
    lines = [connection.line() for _ in range(count)]
    connection.close()
    return header, lines


def owner_hello(node, num=None):
    connection = Connection()
    answer = connection.request(f"HELLO|OWNER|{node.num if num is None else num}|{node.cert_b64}")
    return connection, answer


def put(node, roa, sign_with=None, nonce=None):
    connection, hello = owner_hello(node)
    if not hello.startswith("HELLO_OK|OWNER|"):
        connection.close()
        return hello
    used_nonce = hello.split("|")[2] if nonce is None else nonce
    signature = (sign_with or node).sign(f"PUT|{roa}|{used_nonce}")
    answer = connection.request(f"PUT|{roa}|{signature}")
    connection.close()
    return answer


def delete(node, ip):
    connection, hello = owner_hello(node)
    signature = node.sign(f"DELETE|{ip}|{hello.split('|')[2]}")
    answer = connection.request(f"DELETE|{ip}|{signature}")
    connection.close()
    return answer


def run_protocol_tests(node, other, fake, wrong_num):
    print("\n-- Enrutador: solo lee --")
    check("HELLO de enrutador", Connection().request("HELLO|ENRUTADOR|7"), "HELLO_OK|ENRUTADOR|-")
    check("VERSION con el repositorio vacío", router("VERSION"), "OK|0")
    check("LIST con el repositorio vacío", router_list(), ("OK|0|0", []))
    check("GET de una IP sin ROA", router("GET|10.2.0.1"), "ERR_NO_EXISTE")
    check("CERT de un emisor sin certificado", router("CERT|999"), "ERR_NO_EXISTE")
    check("Un enrutador no puede hacer PUT", router("PUT|ROA|1.1.1.1|1|1|1|1|x|y"), "ERR_PERMISO")
    check("Un enrutador no puede hacer DELETE", router("DELETE|1.1.1.1|x"), "ERR_PERMISO")

    print("\n-- Errores de formato y de orden --")
    check("Comando sin HELLO", Connection().request("LIST"), "ERR_SIN_HELLO")
    check("Línea desconocida sin HELLO", Connection().request("HOLA"), "ERR_FORMATO")
    check("Rol desconocido", Connection().request("HELLO|ADMIN|1"), "ERR_FORMATO")
    check("Comando desconocido", router("BORRAR_TODO"), "ERR_FORMATO")
    check("GET con una IP inválida", router("GET|999.1.1.1"), "ERR_FORMATO")
    check("GET con espacio, como en la tabla del Doc", router("GET| 10.2.0.1"), "ERR_NO_EXISTE")
    check("CERT con un número inválido", router("CERT|abc"), "ERR_FORMATO")

    connection = Connection()
    connection.send("A" * 20000)
    check("Línea demasiado larga", connection.line(), "ERR_FORMATO")
    connection.close()

    print("\n-- La conexión: HELLO + 1 comando + cierre --")
    connection = Connection()
    connection.send("HELLO|ENRUTADOR|1\nVERSION\nVERSION\n")
    check("Dos líneas en un solo envío: HELLO", connection.line(), "HELLO_OK|ENRUTADOR|-")
    check("Dos líneas en un solo envío: comando", connection.line(), "OK|0")
    check("Se cierra después del comando", connection.closed_by_server(), True)
    connection.close()

    connection = Connection()
    connection.send("HELLO|ENR")
    time.sleep(0.2)
    connection.send("UTADOR|1\r\n")
    check("Línea partida en dos envíos y con \\r\\n", connection.line(), "HELLO_OK|ENRUTADOR|-")
    connection.close()

    connection = Connection()
    connection.request("HOLA")
    check("Se cierra después de un HELLO rechazado", connection.closed_by_server(), True)
    connection.close()

    print("\n-- HELLO de dueño --")
    connection, hello = owner_hello(node)
    fields = hello.split("|")
    check("Certificado válido recibe nonce", (fields[0], fields[1], len(fields[2])), ("HELLO_OK", "OWNER", 32))
    connection.close()
    second = owner_hello(node)
    check("El nonce cambia en cada conexión", second[1] != hello, True)
    second[0].close()
    check("Certificado de una CA que no es de confianza", owner_hello(fake)[1], "ERR_PERMISO")
    check("Certificado de otro número de emisor", owner_hello(wrong_num)[1], "ERR_PERMISO")
    check("Base64 que no es un certificado",
          Connection().request("HELLO|OWNER|201|QUJD"), "ERR_FORMATO")
    pem_node = copy.copy(node)
    pem_node.cert_b64 = base64.b64encode(openssl("x509", "-inform", "DER", data=base64.b64decode(node.cert_b64))).decode()
    connection, hello = owner_hello(pem_node)
    check("Certificado en base64 de PEM recibe nonce", hello.split("|")[:2], ["HELLO_OK", "OWNER"])
    connection.close()
    bad_pem = base64.b64encode(b"-----BEGIN CERTIFICATE-----\nbasura\n-----END CERTIFICATE-----\n").decode()
    check("PEM que no trae un certificado", Connection().request(f"HELLO|OWNER|201|{bad_pem}"), "ERR_FORMATO")

    print("\n-- PUT --")
    ip = "10.2.0.5"
    first = node.roa(ip)
    check("PUT de un ROA nuevo sube la versión", put(node, first), "OK|1")
    check("VERSION", router("VERSION"), "OK|1")
    check("LIST trae el ROA", router_list(), ("OK|1|1", [first]))
    check("GET trae el ROA", router(f"GET|{ip}"), f"OK|{first}")
    check("CERT trae el certificado que mandó", router(f"CERT|{node.num}"), f"OK|{node.cert_b64}")
    check("PUT idéntico (republicación) no sube la versión", put(node, first), "OK|1")
    second_roa = node.roa(ip, issued=NOW - 30)
    check("PUT que reemplaza sube la versión", put(node, second_roa), "OK|2")
    check("Después de reemplazar sigue habiendo un solo ROA", router_list(), ("OK|2|1", [second_roa]))
    check("PUT con el certificado en PEM", put(pem_node, second_roa), "OK|2")
    check("CERT lo entrega en DER aunque llegara en PEM", router(f"CERT|{node.num}"), f"OK|{node.cert_b64}")

    print("\n-- PUT rechazados --")
    check("Petición firmada con otra llave", put(node, node.roa("10.2.0.6"), sign_with=other), "ERR_FIRMA")
    check("Petición grabada, con un nonce viejo", put(node, node.roa("10.2.0.6"), nonce="0" * 32), "ERR_FIRMA")
    tampered = node.roa("10.2.0.6").replace("10.2.0.6", "10.2.0.7")
    check("ROA modificado después de firmarlo", put(node, tampered), "ERR_FIRMA")
    check("ROA vencido", put(node, node.roa("10.2.0.6", issued=NOW - 200, expires=NOW - 100)), "ERR_EXPIRADO")
    check("ROA que todavía no empieza", put(node, node.roa("10.2.0.6", issued=NOW + 100)), "ERR_EXPIRADO")
    check("ROA que vence después que el certificado",
          put(node, node.roa("10.2.0.6", expires=NOW + 400 * ONE_DAY)), "ERR_EXPIRADO")
    check("ROA con otro NUM_EMISOR", put(node, node.roa("10.2.0.6", num=202)), "ERR_PERMISO")
    check("ROA con otra isla", put(node, node.roa("10.2.0.6", island=65003)), "ERR_PERMISO")
    check("ROA de otro dueño no se puede reemplazar", put(other, other.roa(ip)), "ERR_PERMISO")
    check("ROA con menos campos", put(node, "ROA|10.2.0.6|65002"), "ERR_FORMATO")
    check("Nada de eso cambió el repositorio", router("VERSION"), "OK|2")

    print("\n-- DELETE --")
    check("DELETE de un ROA de otro dueño", delete(other, ip), "ERR_PERMISO")
    check("DELETE de una IP sin ROA", delete(node, "10.2.0.99"), "ERR_NO_EXISTE")
    connection, hello = owner_hello(node)
    answer = connection.request(f"DELETE|{ip}|{other.sign(f'DELETE|{ip}|' + hello.split('|')[2])}")
    connection.close()
    check("DELETE firmado con otra llave", answer, "ERR_FIRMA")
    check("DELETE del dueño sube la versión", delete(node, ip), "OK|3")
    check("El ROA ya no está", router(f"GET|{ip}"), "ERR_NO_EXISTE")
    check("Se puede volver a publicar", put(node, first), "OK|4")
    check("Se borra para seguir", delete(node, ip), "OK|5")


def run_full_disk_test(node):
    print("\n-- Disco lleno --")
    stored = []
    answer = ""
    for i in range(1, 65):
        ip = f"10.3.0.{i}"
        answer = put(node, node.roa(ip))
        if not answer.startswith("OK|"):
            break
        stored.append(ip)
    check("Cuando no hay espacio responde ERR_LLENO", answer, "ERR_LLENO")
    check("Lo que ya estaba guardado sigue completo", len(router_list()[1]), len(stored))
    for ip in stored:
        delete(node, ip)
    check("Después de borrar queda vacío", len(router_list()[1]), 0)


def run_concurrency_test(node):
    print("\n-- Varios clientes a la vez --")
    roas = [node.roa("10.4.0.1", issued=NOW - 60 - i) for i in range(5)]
    problems = []
    done = threading.Event()

    def reader():
        last_version = -1
        while not done.is_set():
            header, lines = router_list()
            fields = header.split("|")
            version = int(fields[1])
            if version < last_version:
                problems.append(f"la versión bajó: {last_version} -> {version}")
            last_version = version
            if len(lines) != int(fields[2]) or any(line is None or line.count("|") != 6 for line in lines):
                problems.append(f"LIST incompleto: {header}")

    readers = [threading.Thread(target=reader) for _ in range(8)]
    for thread in readers:
        thread.start()
    for roa in roas * 2:
        answer = put(node, roa)
        if not answer.startswith("OK|"):
            problems.append(f"PUT rechazado: {answer}")
    done.set()
    for thread in readers:
        thread.join()
    check("8 lectores mientras un dueño escribe: LIST siempre completo", problems, [])
    delete(node, "10.4.0.1")


def run_timeout_test():
    print("\n-- Timeout --")
    connection = Connection()
    start = time.time()
    closed = connection.closed_by_server()
    elapsed = time.time() - start
    connection.close()
    check("Un cliente callado se desconecta a los 10 s", closed and 9 <= elapsed <= 12, True)


def run_truststore_reload_test(folder, truststore, fake):
    print("\n-- Raíces nuevas sin reiniciar --")
    check("Antes de agregar la raíz se rechaza", owner_hello(fake)[1], "ERR_PERMISO")
    time.sleep(1.1)  # La fecha del archivo se compara en segundos
    fake_root = (folder / "CA-Falsa.crt").read_bytes()
    temporary = folder / "roots.tmp"
    temporary.write_bytes(truststore.read_bytes() + fake_root)
    temporary.replace(truststore)
    connection, hello = owner_hello(fake)
    connection.close()
    check("Después de agregarla se acepta", hello.startswith("HELLO_OK|OWNER|"), True)


def main():
    if not SERVER.exists():
        print(f"Falta {SERVER}: primero hay que correr make")
        return 1

    with tempfile.TemporaryDirectory() as temporary_folder:
        folder = Path(temporary_folder)
        print("Creando CA y nodos de prueba...")
        ca = create_ca(folder, "CA-Prueba")
        fake_ca = create_ca(folder, "CA-Falsa")
        node = Node(folder, ca, ISLAND, 201)
        other = Node(folder, ca, 65003, 301)
        fake = Node(folder, fake_ca, 65009, 901)
        wrong_num = Node(folder, ca, ISLAND, 201, cn_num=777)
        truststore = folder / "trusted-roots.pem"
        truststore.write_bytes(ca[1].read_bytes())

        log = open(folder / "server.log", "w")
        server = subprocess.Popen([str(SERVER), str(truststore), str(PORT)], stdout=log, stderr=subprocess.STDOUT)
        time.sleep(0.5)
        try:
            run_protocol_tests(node, other, fake, wrong_num)
            run_full_disk_test(node)
            run_concurrency_test(node)
            run_truststore_reload_test(folder, truststore, fake)
            run_timeout_test()
        finally:
            server.terminate()
            exit_code = server.wait(timeout=5)
            log.close()
        check("El servidor cierra limpio con SIGTERM", exit_code, 0)

    print(f"\n{'Todo pasó' if failures == 0 else f'{failures} pruebas fallaron'}")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
