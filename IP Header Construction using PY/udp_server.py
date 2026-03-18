# udp_server.py
import socket

HOST = "0.0.0.0"
PORT = 9000

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind((HOST, PORT))

print("Server listening...")

while True:
    data, addr = s.recvfrom(1024)
    print("Received from:", addr)
    print("Message:", data.decode())

