import socket
import struct
import sys

if len(sys.argv) != 4:
    print(f"Uso: {sys.argv[0]} <ip> <puerto> <mensaje>")
    sys.exit(1)

ip = sys.argv[1]
port = int(sys.argv[2])
message = sys.argv[3].encode()

# Header: type (1 byte) + seqNumber (1 byte) + payloadLength (2 bytes, big-endian)
# type=0 -> PROTOCOL_FRAME_DATA, seqNumber=1 (lo que espera keepListening al inicio)
header = struct.pack("!BBH", 0, 1, len(message))
frame = header + message

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(frame, (ip, port))
print("Frame enviado, esperando ACK...")

ackData, _ = sock.recvfrom(1024)
ackType, ackSeq, ackLen = struct.unpack("!BBH", ackData[:4])
print(f"ACK recibido: type={ackType} seq={ackSeq} len={ackLen}")