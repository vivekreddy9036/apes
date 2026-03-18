#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpVsUdpBottleneck");

void CwndTracer(uint32_t oldCwnd, uint32_t newCwnd)
{
    std::cout << Simulator::Now().GetSeconds() << "s: CWND changed from "
              << oldCwnd << " to " << newCwnd << " bytes" << std::endl;
}

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer clients, router, server;
    clients.Create(2);
    router.Create(1);
    server.Create(1);

    // Access links: 100 Mbps
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    accessLink.SetChannelAttribute("Delay", StringValue("2ms"));

    // Bottleneck link: 5 Mbps, 5-packet queue
    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));
    bottleneck.SetQueue("ns3::DropTailQueue<Packet>",
                        "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, 5)));

    NetDeviceContainer d02 = accessLink.Install(clients.Get(0), router.Get(0));
    NetDeviceContainer d12 = accessLink.Install(clients.Get(1), router.Get(0));
    NetDeviceContainer d23 = bottleneck.Install(router.Get(0), server.Get(0));

    InternetStackHelper stack;
    stack.Install(clients);
    stack.Install(router);
    stack.Install(server);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    address.Assign(d02);
    address.SetBase("10.1.2.0", "255.255.255.0");
    address.Assign(d12);
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer serverIf = address.Assign(d23);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // === TCP Application ===
    uint16_t tcpPort = 9000;
    BulkSendHelper tcpClientHelper("ns3::TcpSocketFactory",
                                   InetSocketAddress(serverIf.GetAddress(1), tcpPort));
    tcpClientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited

    ApplicationContainer tcpApps = tcpClientHelper.Install(clients);
    tcpApps.Start(Seconds(1.0));
    tcpApps.Stop(Seconds(10.0));

    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer tcpSinks = tcpSinkHelper.Install(server);
    tcpSinks.Start(Seconds(0.0));
    tcpSinks.Stop(Seconds(10.0));

    // Trace congestion window of first TCP client
    Ptr<Socket> tcpSocket = Socket::CreateSocket(clients.Get(0), TcpSocketFactory::GetTypeId());
    tcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndTracer));

    // === UDP Application (comparison) ===
    uint16_t udpPort = 8000;
    OnOffHelper udpClient("ns3::UdpSocketFactory",
                          Address(InetSocketAddress(serverIf.GetAddress(1), udpPort)));
    udpClient.SetAttribute("DataRate", StringValue("20Mbps"));
    udpClient.SetAttribute("PacketSize", UintegerValue(1472));
    udpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer udpApps = udpClient.Install(clients.Get(1));
    udpApps.Start(Seconds(1.0));
    udpApps.Stop(Seconds(10.0));

    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer udpSinks = udpSinkHelper.Install(server);
    udpSinks.Start(Seconds(0.0));
    udpSinks.Stop(Seconds(10.0));

    // === FlowMonitor to measure throughput and packet drops ===
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        double throughput = it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1e6;
        std::cout << "Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ") ";
        std::cout << "Throughput: " << throughput << " Mbps, Lost packets: " << it->second.lostPackets << std::endl;
    }

    Simulator::Destroy();
    return 0;
}

