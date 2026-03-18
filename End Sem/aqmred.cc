#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer sources, router, sink;
  sources.Create (2);
  router.Create (1);
  sink.Create (1);

  // ---------- Access links ----------
  PointToPointHelper access;
  access.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  access.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d0r = access.Install (sources.Get (0), router.Get (0));
  NetDeviceContainer d1r = access.Install (sources.Get (1), router.Get (0));

  // ---------- Bottleneck ----------
  PointToPointHelper bottleneck;
  bottleneck.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  bottleneck.SetChannelAttribute ("Delay", StringValue ("10ms"));

  // Disable device buffering.the queue size is set to 1 packet (1p), which is effectively almost no buffering. Normally, a network device (NetDevice) has a default hardware/software buffer for packets. By setting it to just 1 packet, you are minimizing the device’s internal queue, so the queue won't store multiple packets, which is why the comment says “disable device buffering”.

//It doesn’t completely remove the queue (you always need at least 1 packet), but it prevents large queue buildup that could hide the effects of the AQM discipline being tested.
  bottleneck.SetQueue ("ns3::DropTailQueue<Packet>",
                       "MaxSize", QueueSizeValue (QueueSize ("1p")));

  NetDeviceContainer drs = bottleneck.Install (router.Get (0), sink.Get (0));

  // ---------- Internet ----------
  InternetStackHelper stack;
  stack.InstallAll ();

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (d0r);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  address.Assign (d1r);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer sinkIf = address.Assign (drs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  //TrafficControlHelper in NS-3 is used to install and configure queue disciplines (like RED, CoDel, or DropTail) on NetDevices. It allows you to control how packets are queued, scheduled, and dropped to manage congestion
  TrafficControlHelper tch;

  // REMOVE default queue disc FIRST
  tch.Uninstall (drs.Get (0));
  //we are installing the actual AQM (RED) queue on the bottleneck device.RED will start probabilistically dropping packets when the average queue length is between MinTh and MaxTh.SetRootQueueDisc is a method of TrafficControlHelper used to assign a specific queue discipline (e.g., RED, CoDel) as the root queue on a network device. It also allows setting the configuration parameters of that queue discipline, such as thresholds, queue size, and packet handling behavior.
  tch.SetRootQueueDisc (
      "ns3::RedQueueDisc",
      "MinTh", DoubleValue (2),
      "MaxTh", DoubleValue (5),
      "MaxSize", QueueSizeValue (QueueSize ("20p")),
      "LinkBandwidth", StringValue ("5Mbps"),
      "LinkDelay", StringValue ("10ms"),
      "MeanPktSize", UintegerValue (1500),
      "Gentle", BooleanValue (true)
  );

  tch.Install (drs.Get (0));

  // ---------- Applications ----------
  uint16_t port = 50000;

  PacketSinkHelper sinkApp ("ns3::TcpSocketFactory",
                            InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sinkApp.Install (sink.Get (0));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (20.0));

  for (uint32_t i = 0; i < sources.GetN (); i++)
    {
      BulkSendHelper bulk ("ns3::TcpSocketFactory",
                           InetSocketAddress (sinkIf.GetAddress (1), port));
      bulk.SetAttribute ("MaxBytes", UintegerValue (0));

      ApplicationContainer app = bulk.Install (sources.Get (i));
      app.Start (Seconds (1.0));
      app.Stop (Seconds (20.0));
    }

  // ---------- Flow Monitor ----------
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());

  for (auto const &flow : monitor->GetFlowStats ())
    {
      auto t = classifier->FindFlow (flow.first);
      std::cout << "Flow " << flow.first << " (" << t.sourceAddress
                << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Lost packets: " << flow.second.lostPackets << "\n";
      if (flow.second.rxPackets > 0)
        {
          std::cout << "  Mean delay: "
                    << flow.second.delaySum.GetSeconds () /
                           flow.second.rxPackets
                    << " s\n";
        }
    }

  Simulator::Destroy ();
  return 0;
}

