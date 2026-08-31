import argparse
import subprocess
import time
from datetime import datetime
from gpiozero import MotionSensor
import storage


def parse_args():
    parser = argparse.ArgumentParser(description="Captura de sensor de movimiento y envio periodico")
    parser.add_argument("ip", help="IP destino del receptor")
    parser.add_argument("port", help="Puerto destino")
    parser.add_argument("--file", default="message.txt", help="Ruta del archivo de datos (default: message.txt)")
    parser.add_argument("--binary", default="../user/build/server", help="Ruta al binario compilado")
    parser.add_argument("--interval", type=int, default=30, help="Intervalo de envio en segundos (default: 30)")
    return parser.parse_args()


args = parse_args()

pir = MotionSensor(17)  # 17 = GPIO17 = pin fisico 11

def onMotion():
    storage.storeData(f"[{datetime.now():%Y-%m-%d %H:%M:%S}] Detected movement", args.file)

pir.when_motion = onMotion

while True:
    time.sleep(args.interval)
    subprocess.run([args.binary, args.ip, args.port, args.file])