# client.py
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

message = b"DNS_QUERY"
sock.sendto(message, ("REFLECTOR_IP", 5300))

data, _ = sock.recvfrom(65535)

print("Sent:", len(message), "bytes")
print("Received:", len(data), "bytes")
print("Amplification factor:", len(data)/len(message))

