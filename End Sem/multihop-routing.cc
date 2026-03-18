/*
 * ns-3 Exercise 2: Mesh Topology and Routing Analysis
 *
 * Objective: Trace a packet's path through an intermediate router (R)
 * and observe the accumulated end-to-end delay across two different links.
 *
 * Topology: Node 0 (Client A) -- Node 1 (Router R) -- Node 2 (Server B)
 *
 * Link A-R (Node 0 to Node 1): 10Mbps / 5ms
 * Link R-B (Node 1 to Node 2): 5Mbps / 10ms (The Slower Link)
 *
 * To run:
 * 1. Build: ./ns3 build
 * 2. Run: ./ns3 run mesh-routing-analysis
 * 3. Analyze: Open 'mesh-routing-analysis.tr' (text editor) and 'mesh-routing-analysis.xml' (NetAnim).
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h" // Include for NetAnim

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshRoutingAnalysis");

int main(int argc, char *argv[])
{
    // --- 1. CONFIGURATION (Default Values) ---
    std::string dataRateAR = "10Mbps";
    std::string delayAR = "5ms";
    std::string dataRateRB = "5Mbps"; // Slower link
    std::string delayRB = "10ms";     // Higher delay link
    
    uint32_t packetSize = 1024;
    double simTime = 5.0;
    
    // Command Line Parsing is kept simple as per request
    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total duration of the simulation in seconds", simTime);
    cmd.Parse(argc, argv);

    // --- 2. TOPOLOGY SETUP ---
    NS_LOG_INFO("Creating three-node dumbbell topology: A(0) - R(1) - B(2).");
    NodeContainer nodes;
    nodes.Create(3); // Node 0 (Client A), Node 1 (Router R), Node 2 (Server B)

    // --- 3. LINK CONFIGURATION ---

    // 3a. A-R Link (Node 0 to Node 1)
    PointToPointHelper p2pAR;
    p2pAR.SetDeviceAttribute("DataRate", StringValue(dataRateAR));
    p2pAR.SetChannelAttribute("Delay", StringValue(delayAR));
    NetDeviceContainer dAR = p2pAR.Install(nodes.Get(0), nodes.Get(1));

    // 3b. R-B Link (Node 1 to Node 2)
    PointToPointHelper p2pRB;
    p2pRB.SetDeviceAttribute("DataRate", StringValue(dataRateRB));
    p2pRB.SetChannelAttribute("Delay", StringValue(delayRB));
    NetDeviceContainer dRB = p2pRB.Install(nodes.Get(1), nodes.Get(2));

    // --- 4. PROTOCOL STACK AND IP ADDRESSING ---
    InternetStackHelper stack;
    stack.Install(nodes);

    // 4a. IP Subnetting for A-R link (10.1.1.0)
    Ipv4AddressHelper addressA;
    addressA.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iAR = addressA.Assign(dAR);

    // 4b. IP Subnetting for R-B link (10.1.2.0)
    Ipv4AddressHelper addressB;
    addressB.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iRB = addressB.Assign(dRB);

    // --- 5. ROUTING INSTALLATION ---
    // This is CRITICAL: it creates the routing tables that allow Node 1 to forward packets.
    //The Ipv4GlobalRoutingHelper::PopulateRoutingTables() command creates the routing table on every node in the simulation
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // --- 6. TRAFFIC GENERATION (Client A -> Server B) ---
    uint16_t port = 7; 
    Ipv4Address serverAddress = iRB.GetAddress(1); // Server B is 10.1.2.2

    // 6a. UDP Echo Server on Server B (Node 2)
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // 6b. UDP Echo Client on Client A (Node 0)
    UdpEchoClientHelper echoClient(serverAddress, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    
    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0)); // Send packet at 1.0s
    clientApp.Stop(Seconds(simTime));

    // --- 7. ANALYSIS (ASCII Tracing & NetAnim) ---

    // 7a. ASCII TRACE (.tr file) for detailed path and timing analysis
    AsciiTraceHelper ascii;
    // Both links log to the same trace file
    p2pAR.EnableAsciiAll(ascii.CreateFileStream("mesh-routing-analysis.tr"));
    p2pRB.EnableAsciiAll(ascii.CreateFileStream("mesh-routing-analysis.tr"));
    
    // 7b. NETANIM (.xml file) for visualization
    AnimationInterface anim("mesh-routing-analysis.xml");
    anim.SetConstantPosition(nodes.Get(0), 5.0, 10.0); // Client A
    anim.SetConstantPosition(nodes.Get(1), 20.0, 10.0); // Router R
    anim.SetConstantPosition(nodes.Get(2), 35.0, 10.0); // Server B

    // --- 8. SIMULATION RUN ---
    NS_LOG_INFO("Running simulation. Trace files created.");
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
