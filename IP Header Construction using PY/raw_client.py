# raw_client.py
import socket
import struct

def checksum(msg):
    s = 0
    for i in range(0, len(msg), 2):
        w = (msg[i] << 8) + (msg[i+1])
        s += w
    s = (s >> 16) + (s & 0xffff)
    s += (s >> 16)
    return ~s & 0xffff

# Create raw socket
s = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_RAW)
s.setsockopt(socket.IPPROTO_IP, socket.IP_HDRINCL, 1)

source_ip = "127.0.0.1"
dest_ip = "127.0.0.1"

# IP Header fields
version = 4
ihl = 5
ver_ihl = (version << 4) + ihl
tos = 0
tot_len = 20 + 8
id = 54321
frag_off = 0
ttl = 10  # Custom TTL
protocol = socket.IPPROTO_UDP
check = 0
saddr = socket.inet_aton(source_ip)
daddr = socket.inet_aton(dest_ip)

ip_header = struct.pack('!BBHHHBBH4s4s',
                        ver_ihl, tos, tot_len,
                        id, frag_off,
                        ttl, protocol,
                        check, saddr, daddr)

# UDP Header
source_port = 1234
dest_port = 9000
length = 8
checksum_udp = 0

udp_header = struct.pack('!HHHH',
                         source_port,
                         dest_port,
                         length,
                         checksum_udp)

packet = ip_header + udp_header

s.sendto(packet, (dest_ip, 0))
print("Packet sent")

