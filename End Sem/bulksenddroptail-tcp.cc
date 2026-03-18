#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // ======================
    // Nodes
    // ======================
    NodeContainer clients, router, server;
    clients.Create(2);
    router.Create(1);
    server.Create(1);

    // ======================
    // Links
    // ======================
    PointToPointHelper access;
    access.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    access.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));
    bottleneck.SetQueue("ns3::DropTailQueue<Packet>",
                        "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, 20)));

    NetDeviceContainer d0r = access.Install(clients.Get(0), router.Get(0));
    NetDeviceContainer d1r = access.Install(clients.Get(1), router.Get(0));
    NetDeviceContainer drs = bottleneck.Install(router.Get(0), server.Get(0));

    // ======================
    // Internet stack
    // ======================
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    address.Assign(d0r);

    address.SetBase("10.1.2.0", "255.255.255.0");
    address.Assign(d1r);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer serverIf = address.Assign(drs);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ======================
    // TCP Applications
    // ======================
    uint16_t port = 9000;

    BulkSendHelper sender("ns3::TcpSocketFactory",
                          InetSocketAddress(serverIf.GetAddress(1), port));
    sender.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited

    ApplicationContainer apps;
    apps.Add(sender.Install(clients.Get(0)));
    apps.Add(sender.Install(clients.Get(1)));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(server.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // ======================
    // Flow Monitor
    // ======================
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    auto stats = monitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        auto t = classifier->FindFlow(it->first);
        std::cout << "Flow " << it->first
                  << " (" << t.sourceAddress << " -> "
                  << t.destinationAddress << ")\n";
        std::cout << "  Lost packets: " << it->second.lostPackets << "\n";
        std::cout << "  Mean delay: "
                  << (it->second.delaySum.GetSeconds() /
                      it->second.rxPackets) << " s\n";
    }

    Simulator::Destroy();
    return 0;
}

