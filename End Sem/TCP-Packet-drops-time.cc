#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;
using namespace std;

/* ---------- TX COUNTERS ---------- */
uint64_t client1TxPackets = 0;
uint64_t client2TxPackets = 0;

/* ---------- APP TX TRACE ---------- */
void
Client1TxTrace (Ptr<const Packet> packet)
{
    client1TxPackets++;
}

void
Client2TxTrace (Ptr<const Packet> packet)
{
    client2TxPackets++;
}

/* ---------- QUEUE DROP TRACE ---------- */
uint64_t totalQueueDrops = 0;

void
QueueDiscDropTrace (Ptr<const QueueDiscItem> item)
{
    totalQueueDrops++;

    cout << "[QUEUE DROP] Time = "
         << Simulator::Now().GetSeconds()
         << " s, Packet Size = "
         << item->GetPacket()->GetSize()
         << " bytes" << endl;
}

int
main (int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    /* ---------- NODES ---------- */
    NodeContainer clients, router, server;
    clients.Create(2);
    router.Create(1);
    server.Create(1);

    

    /* ---------- LINKS ---------- */
    PointToPointHelper access;
    access.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    access.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer d0r = access.Install(clients.Get(0), router.Get(0));
    NetDeviceContainer d1r = access.Install(clients.Get(1), router.Get(0));
    NetDeviceContainer drs = bottleneck.Install(router.Get(0), server.Get(0));

    /* ---------- INTERNET ---------- */
    InternetStackHelper stack;
    stack.InstallAll();

    /* ---------- IP ADDRESSING ---------- */
    Ipv4AddressHelper addr;

    addr.SetBase("10.1.1.0", "255.255.255.0");
    addr.Assign(d0r);

    addr.SetBase("10.1.2.0", "255.255.255.0");
    addr.Assign(d1r);

    addr.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer serverIf = addr.Assign(drs);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    /* ---------- TRAFFIC CONTROL ---------- */
    TrafficControlHelper tch;
    tch.Uninstall(drs.Get(0));

    tch.SetRootQueueDisc(
        "ns3::PfifoFastQueueDisc",
        "MaxSize", QueueSizeValue(QueueSize("5p"))
    );

    QueueDiscContainer qdiscs = tch.Install(drs.Get(0));
    qdiscs.Get(0)->TraceConnectWithoutContext(
        "Drop", MakeCallback(&QueueDiscDropTrace));

    /* ---------- APPLICATIONS (TCP) ---------- */
    uint16_t port1 = 5000;
    uint16_t port2 = 5001;

    // TCP Sinks
    PacketSinkHelper sink1(
        "ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), port1));

    PacketSinkHelper sink2(
        "ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), port2));

    ApplicationContainer sinkApps;
    sinkApps.Add(sink1.Install(server.Get(0)));
    sinkApps.Add(sink2.Install(server.Get(0)));
    sinkApps.Start(Seconds(0.5));
    sinkApps.Stop(Seconds(3.0));

    // TCP BulkSend clients
    BulkSendHelper bulk1(
        "ns3::TcpSocketFactory",
        InetSocketAddress(serverIf.GetAddress(1), port1));

    BulkSendHelper bulk2(
        "ns3::TcpSocketFactory",
        InetSocketAddress(serverIf.GetAddress(1), port2));

    bulk1.SetAttribute("MaxBytes", UintegerValue(0));
    bulk2.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer app1 = bulk1.Install(clients.Get(0));
    ApplicationContainer app2 = bulk2.Install(clients.Get(1));

    app1.Start(Seconds(1.0));
    app2.Start(Seconds(1.0));
    app1.Stop(Seconds(2.0));
    app2.Stop(Seconds(2.0));

    // Attach TX traces SAFELY
    Ptr<BulkSendApplication> b1 =
        DynamicCast<BulkSendApplication>(app1.Get(0));
    Ptr<BulkSendApplication> b2 =
        DynamicCast<BulkSendApplication>(app2.Get(0));

    b1->TraceConnectWithoutContext("Tx", MakeCallback(&Client1TxTrace));
    b2->TraceConnectWithoutContext("Tx", MakeCallback(&Client2TxTrace));

    

    /* ---------- RUN ---------- */
    Simulator::Stop(Seconds(3.0));
    Simulator::Run();

    /* ---------- RESULTS ---------- */
    cout << "\n=== TCP TRANSMISSION SUMMARY ===\n";
    cout << "Client 1 TX packets: " << client1TxPackets << endl;
    cout << "Client 2 TX packets: " << client2TxPackets << endl;
    cout << "Total TX packets   : "
         << (client1TxPackets + client2TxPackets) << endl;
    cout << "Total queue drops  : " << totalQueueDrops << endl;

    Simulator::Destroy();
    return 0;
}

