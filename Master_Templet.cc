#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;
using namespace std;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);
    
    NodeContainer clients, router, server;
    clients.Create(2);
    router.Create(1);
    server.Create(1);
    
    PointToPointHelper access;
    access.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    access.SetChannelAttribute("Delay", StringValue("2ms"));
    
    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));
    bottleneck.SetQueue("ns3::DropTailQueue<Packet>",
                        "MaxSize", QueueSizeValue(QueueSize("5p")));
    
    NetDeviceContainer d0r = access.Install(clients.Get(0), router.Get(0));
    NetDeviceContainer d1r = access.Install(clients.Get(1), router.Get(0));
    NetDeviceContainer drs = bottleneck.Install(router.Get(0), server.Get(0));
    
    InternetStackHelper stack;
    stack.InstallAll();
    
    Ipv4AddressHelper addr;
    
    addr.SetBase("10.1.1.0", "255.255.255.0");
    addr.Assign(d0r);
    
    addr.SetBase("10.1.2.0", "255.255.255.0");
    addr.Assign(d1r);
    
    addr.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer serverIf = addr.Assign(drs);
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    TrafficControlHelper tch;
    tch.Uninstall(drs.Get(0));
    
    tch.SetRootQueueDisc(
        "ns3::RedQueueDisc",
        "MaxSize", QueueSizeValue(QueueSize("20p"))
    );
    
    QueueDiscContainer qdiscs = tch.Install(drs.Get(0));
    
    uint16_t tcpPort = 9000;
    uint16_t udpPort = 8000;
    
    BulkSendHelper tcpClient(
        "ns3::TcpSocketFactory",
        InetSocketAddress(serverIf.GetAddress(1), tcpPort));
    
    tcpClient.SetAttribute("MaxBytes", UintegerValue(0));
    
    ApplicationContainer tcpApps = tcpClient.Install(clients);
    tcpApps.Start(Seconds(1.0));
    tcpApps.Stop(Seconds(10.0));
    
    PacketSinkHelper tcpSink(
        "ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    
    ApplicationContainer sinkApp1 = tcpSink.Install(server.Get(0));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(10.0));
    
    OnOffHelper udpClient(
        "ns3::UdpSocketFactory",
        InetSocketAddress(serverIf.GetAddress(1), udpPort));
    
    udpClient.SetAttribute("DataRate", StringValue("20Mbps"));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    udpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    
    ApplicationContainer udpApps = udpClient.Install(clients.Get(1));
    udpApps.Start(Seconds(1.0));
    udpApps.Stop(Seconds(10.0));
    
    PacketSinkHelper udpSink(
        "ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    
    ApplicationContainer sinkApp2 = udpSink.Install(server.Get(0));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(10.0));
    
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    
    AnimationInterface anim("anim.xml");
    
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    
    monitor->CheckForLostPackets();
    
    Simulator::Destroy();
    return 0;
}
