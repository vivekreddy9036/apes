#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// Set up namespace
using namespace ns3;

// NS_LOG_COMPONENT_DEFINE is necessary for the logging system
NS_LOG_COMPONENT_DEFINE("PointToPointDelay");

int main(int argc, char *argv[])
{
    // --- 1. CONFIGURATION ---
    // Configuration variables for the link and traffic
    std::string dataRate = "10Mbps";
    std::string delay = "10ms";
    uint32_t packetSize = 1024; // Bytes (1024 B)
    uint32_t numPackets = 1;

    // Command-line arguments to allow easy variation of parameters
    CommandLine cmd;
    cmd.AddValue("dataRate", "Data rate of the Point-to-Point link (e.g., 10Mbps)", dataRate);
    cmd.AddValue("delay", "Propagation delay of the link (e.g., 10ms)", delay);
    cmd.AddValue("packetSize", "Size of the UDP Echo packet in bytes (e.g., 1024)", packetSize);
    cmd.Parse(argc, argv);

    // Calculate theoretical Transmission Delay (Ttx) and End-to-End Delay (E2E)
    // Ttx = Packet Size (bits) / Data Rate (bits/s)
    // Packet Size in bits = packetSize * 8
    // DataRate in bits/s is set by the string, ns-3 handles the parsing.

    // Ttx = (1024 bytes * 8 bits/byte) / (10,000,000 bits/s) = 8192 / 10,000,000 = 0.0008192 seconds (0.8192 ms)
    // E2E Delay (approx) = Transmission Delay + Propagation Delay
    // E2E Delay = 0.8192 ms + 10 ms = 10.8192 ms (This is the value we look for in Wireshark)
    
    // --- 2. TOPOLOGY SETUP (Nodes and Link) ---
    NS_LOG_INFO("Creating topology: Two nodes (A and B) connected by P2P link.");
     
    //The following two lines create two nodes without network interface card and internet stack 
    NodeContainer nodes;
    nodes.Create(2);

    //PointToPointHelper creates a point to point link and assigns data rate and delay to that link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    //The following two lines install network interface card to the two nodes
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    //The following two lines install the complete Internet Protocol (IP) stack onto the two nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    //Defines the network ID (10.1.1.0) and subnet mask (255.255.255.0 or /24) for the link. This establishes the subnet.
    address.SetBase("10.1.1.0", "255.255.255.0");
    //Iterates through all the network devices (devices) and assigns the next available sequential IP address (e.g., 10.1.1.1, 10.1.1.2, etc.) to each one.
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // --- 4. TRAFFIC GENERATION (UDP Echo) ---
    NS_LOG_INFO("Installing UDP Echo applications.");
    
    // UDP Echo Server (Node B - index 1)
    uint16_t port = 7; // Standard Echo Port
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
    //Start the server application at the 0th second
    serverApp.Start(Seconds(0.0));
    //Stop running and listening server application at t=5.0 seconds
    serverApp.Stop(Seconds(5.0));

    // UDP Echo Client (Node A - index 0)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    //To send one packet every 1.0 second
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    
    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0)); // Client starts sending at 1.0s
    clientApp.Stop(Seconds(5.0));

    // --- 5. OBSERVATION (PCAP TRACING) ---
    // This generates files that can be opened with Wireshark
    NS_LOG_INFO("Enabling Pcap tracing for Wireshark analysis.");
    //It records all network traffic passing through one side of the point to point link into a Wireshark-readable file.
    pointToPoint.EnablePcap("point-to-point-delay", devices.Get(0), true);

    // --- 6. SIMULATION RUN ---
    NS_LOG_INFO("Running simulation for 5 seconds.");
    //The following line instructs ns-3 event simulator to execute the "stop simulation" command precisely at t=5.0 seconds
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");
    
    return 0;
}
