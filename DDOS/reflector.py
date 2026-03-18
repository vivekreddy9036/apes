# reflector.py
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 5300))

print("Simulated DNS reflector running...")

while True:
    data, addr = sock.recvfrom(1024)
    print("Received request of", len(data), "bytes from", addr)

    # Simulated amplified response
    response = b"DNS_RESPONSE_DATA" * 200
    sock.sendto(response, addr)

