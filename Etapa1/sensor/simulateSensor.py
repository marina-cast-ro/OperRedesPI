import argparse
import subprocess
import time
import threading
from datetime import datetime

from gpiozero import Device
from gpiozero.pins.mock import MockFactory
Device.pin_factory = MockFactory()  # <- reemplaza el hardware real por uno simulado

from gpiozero import MotionSensor
import storage


def parse_args():
    parser = argparse.ArgumentParser(description="Simulacion de sensor de movimiento (sin Raspberry Pi)")
    parser.add_argument("ip", help="IP destino del receptor")
    parser.add_argument("port", help="Puerto destino")
    parser.add_argument("--file", default="message.txt")
    parser.add_argument("--binary", default="../user/build/server")
    parser.add_argument("--interval", type=int, default=10, help="Intervalo de envio en segundos")
    parser.add_argument("--motion-every", type=int, default=4, help="Cada cuantos segundos simular un movimiento")
    return parser.parse_args()


args = parse_args()

pir = MotionSensor(17)

def onMotion():
    storage.storeData(f"[{datetime.now():%Y-%m-%d %H:%M:%S}] Detected movement (SIMULADO)", args.file)
    print("[SIMULACION] Movimiento detectado y guardado")

pir.when_motion = onMotion


def simulateMotionLoop():
    while True:
        time.sleep(args.motion_every)
        pir.pin.drive_high()   # simula que el sensor detecta movimiento
        time.sleep(0.5)
        pir.pin.drive_low()    # simula que el movimiento ceso


threading.Thread(target=simulateMotionLoop, daemon=True).start()

while True:
    time.sleep(args.interval)
    subprocess.run([args.binary, args.ip, args.port, args.file])