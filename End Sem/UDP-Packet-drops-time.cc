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

/* ---------- QUEUE DROP COUNTER ---------- */
uint64_t totalQueueDrops = 0;

/* ---------- TX TRACE CALLBACKS ---------- */
void
Client1TxTrace(Ptr<const Packet> packet)
{
    client1TxPackets++;
}

void
Client2TxTrace(Ptr<const Packet> packet)
{
    client2TxPackets++;
}

/* ---------- QUEUE DROP TRACE ---------- */
void
QueueDiscDropTrace(Ptr<const QueueDiscItem> item)
{
    totalQueueDrops++;

    cout << "[QUEUE DROP] "
         << "Time = " << Simulator::Now().GetSeconds()
         << " s, Packet Size = "
         << item->GetPacket()->GetSize()
         << " bytes"
         << endl;
}

int
main(int argc, char *argv[])
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

    /* ---------- APPLICATIONS ---------- */
    uint16_t port1 = 5000;
    uint16_t port2 = 5001;

    OnOffHelper onoff1("ns3::UdpSocketFactory",
        Address(InetSocketAddress(serverIf.GetAddress(1), port1)));
    OnOffHelper onoff2("ns3::UdpSocketFactory",
        Address(InetSocketAddress(serverIf.GetAddress(1), port2)));

    onoff1.SetAttribute("DataRate", StringValue("20Mbps"));
    onoff1.SetAttribute("PacketSize", UintegerValue(1472));
    onoff1.SetAttribute("OnTime",
        StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime",
        StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    onoff2.SetAttribute("DataRate", StringValue("20Mbps"));
    onoff2.SetAttribute("PacketSize", UintegerValue(1472));
    onoff2.SetAttribute("OnTime",
        StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime",
        StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer app1 = onoff1.Install(clients.Get(0));
    ApplicationContainer app2 = onoff2.Install(clients.Get(1));

    app1.Start(Seconds(1.0));
    app2.Start(Seconds(1.0));
    app1.Stop(Seconds(2.0));
    app2.Stop(Seconds(2.0));

    Ptr<OnOffApplication> app1Ptr =
        DynamicCast<OnOffApplication>(app1.Get(0));
    Ptr<OnOffApplication> app2Ptr =
        DynamicCast<OnOffApplication>(app2.Get(0));
//whenever onoff application sends a packet,Client1TxTrace/Client12TxTrace will be called 
    app1Ptr->TraceConnectWithoutContext(
        "Tx", MakeCallback(&Client1TxTrace));
    app2Ptr->TraceConnectWithoutContext(
        "Tx", MakeCallback(&Client2TxTrace));

    /* ---------- FLOW MONITOR ---------- */
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    

    /* ---------- RUN ---------- */
    Simulator::Stop(Seconds(3.0));
    Simulator::Run();

    /* ---------- RESULTS ---------- */
    monitor->CheckForLostPackets();

    cout << "\n=== TRANSMISSION SUMMARY ===\n";
    cout << "Client 1 TX packets: " << client1TxPackets << endl;
    cout << "Client 2 TX packets: " << client2TxPackets << endl;
    cout << "Total TX packets   : "
         << (client1TxPackets + client2TxPackets) << endl;

    cout << "\nTotal queue drops: " << totalQueueDrops << endl;

    Simulator::Destroy();
    return 0;
}


