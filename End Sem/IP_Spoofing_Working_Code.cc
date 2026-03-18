#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

/* ============================================================
 * TRUE INGRESS FILTER (DETECTION ONLY)
 * ============================================================ */
static void IngressFilterRx (
  Ptr<const Packet> packet,
  Ptr<Ipv4> ipv4,
  uint32_t interface)
{
  // Only attacker-facing interface (router interface 1)
  if (interface != 1)
    return;

  Ptr<Packet> p = packet->Copy ();
  Ipv4Header ip;
  if (!p->PeekHeader (ip))
    return;

  // Only UDP packets (our attacker traffic)
  if (ip.GetProtocol () != 17)
    return;

  Ipv4Address src = ip.GetSource ();

  // Ignore router-originated packets
  if (ipv4->IsDestinationAddress (src, interface))
    return;

  // Interface subnet
  Ipv4InterfaceAddress ifAddr =
      ipv4->GetAddress (interface, 0);

  Ipv4Address ifaceSubnet =
      ifAddr.GetLocal ().CombineMask (ifAddr.GetMask ());

  // TRUE ingress filtering rule
  if (src.CombineMask (ifAddr.GetMask ()) != ifaceSubnet)
    {
      std::cout << Simulator::Now ().GetSeconds ()
                << "s  INGRESS FILTER: DETECTED spoofed packet from "
                << src
                << " on interface "
                << interface
                << std::endl;
    }
}

/* ============================================================
 * SEND SPOOFED PACKETS (RAW SOCKET)
 * ============================================================ */
static void
SendSpoofedPacket (
  Ptr<Socket> socket,
  Ipv4Address spoofedSrc,
  Ipv4Address dst)
{
  Ptr<Packet> pkt = Create<Packet> (512);

  Ipv4Header ip;
  ip.SetSource (spoofedSrc);
  ip.SetDestination (dst);
  ip.SetProtocol (17); // UDP
  ip.SetPayloadSize (512);
  ip.SetTtl (64);

  pkt->AddHeader (ip);
  socket->SendTo (pkt, 0, InetSocketAddress (dst, 9));
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main ()
{
  NodeContainer nodes;
  nodes.Create (3); // 0=attacker, 1=router, 2=victim

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d01 =
      p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer d12 =
      p2p.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper internet;
  internet.InstallAll ();

  Ipv4AddressHelper addr;

  // Attacker ↔ Router
  addr.SetBase ("10.1.1.0", "255.255.255.0");
  addr.Assign (d01);

  // Router ↔ Victim
  addr.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if12 = addr.Assign (d12);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* Attach ingress filter to router */
  Ptr<Ipv4> ipv4Router =
      nodes.Get (1)->GetObject<Ipv4> ();
      
 //It connects your function IngressFilterRx() to the router’s IPv4 receive event.So whenever any IPv4 packet is received by the router, your function is automatically called.
  ipv4Router->TraceConnectWithoutContext (
      "Rx",
      MakeCallback (&IngressFilterRx));

  /* RAW socket on attacker */
  Ptr<Socket> raw =
      Socket::CreateSocket (
          nodes.Get (0),
          Ipv4RawSocketFactory::GetTypeId ());

  raw->SetAttribute ("Protocol", UintegerValue (17));
  raw->SetAttribute ("IpHeaderInclude", BooleanValue (true));
  raw->BindToNetDevice (d01.Get (0));

  /* Schedule spoofed packets */
  for (double t = 1.0; t <= 2.0; t += 0.2)
    {
      // Same subnet spoof → allowed
      Simulator::Schedule (
          Seconds (t),
          &SendSpoofedPacket,
          raw,
          Ipv4Address ("10.1.1.10"),
          if12.GetAddress (1));

      // Different subnet spoof → detected
      Simulator::Schedule (
          Seconds (t + 0.1),
          &SendSpoofedPacket,
          raw,
          Ipv4Address ("10.1.2.10"),
          if12.GetAddress (1));
    }

  Simulator::Stop (Seconds (3.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

