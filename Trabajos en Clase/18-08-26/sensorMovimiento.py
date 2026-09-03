#!/usr/bin/env python3
"""
Lectura del sensor de movimiento PIR HC-SR501 en Raspberry Pi 4.
Trabajo de familiarizacion en clase, 18-08-2026.
"""

from gpiozero import MotionSensor

pir = MotionSensor(17)          # 17 = GPIO17 = pin fisico 11

while True:
    pir.wait_for_motion()
    print("movimiento detectado")
    pir.wait_for_no_motion()
    print("se detuvo")