/*
 * Question 5.2 : Impact de la distance et du canal
 * Teste différentes distances et largeurs de canal
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MimoQ2");

int main(int argc, char *argv[])
{
    double distance = 5.0;
    uint32_t channelWidth = 20;
    double duration = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("distance", "Distance between STA and AP (meters)", distance);
    cmd.AddValue("channelWidth", "Channel width: 20 or 40 MHz", channelWidth);
    cmd.AddValue("duration", "Simulation duration (seconds)", duration);
    cmd.Parse(argc, argv);

    std::cout << "\n========================================\n";
    std::cout << "MIMO Distance Test\n";
    std::cout << "Distance: " << distance << " m\n";
    std::cout << "Channel:  " << channelWidth << " MHz\n";
    std::cout << "========================================\n\n";

    // Nodes
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // WiFi 802.11n 5GHz with 2x2 MIMO
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    // MAC
    WifiMacHelper mac;
    Ssid ssid = Ssid("mimo-distance");

    // Configure channel width and MIMO
    std::string channelSettings = "{0, " + std::to_string(channelWidth) + ", BAND_5GHZ, 0}";
    phy.Set("ChannelSettings", StringValue(channelSettings));
    phy.Set("Antennas", UintegerValue(2));
    phy.Set("MaxSupportedTxSpatialStreams", UintegerValue(2));
    phy.Set("MaxSupportedRxSpatialStreams", UintegerValue(2));

    // STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    // AP
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility - variable distance
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));      // AP
    positionAlloc->Add(Vector(distance, 0.0, 0.0)); // STA
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    // IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(staDevice);
    interfaces.Add(address.Assign(apDevice));

    // UDP traffic
    uint16_t port = 7;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(duration));

    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(MicroSeconds(10))); // 10x plus de trafic
    client.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(duration));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // NetAnim
    std::string animFile = "mimo-q2-" + std::to_string((int)distance) + "m-" +
                           std::to_string(channelWidth) + "mhz.xml";
    AnimationInterface anim(animFile);
    anim.SetMaxPktsPerTraceFile(500000);

    // Set node descriptions
    anim.UpdateNodeDescription(wifiApNode.Get(0), "AP");
    anim.UpdateNodeDescription(wifiStaNode.Get(0), "STA");

    // Set node colors
    anim.UpdateNodeColor(wifiApNode.Get(0), 255, 0, 0);  // Red for AP
    anim.UpdateNodeColor(wifiStaNode.Get(0), 0, 0, 255); // Blue for STA

    Simulator::Stop(Seconds(duration + 1));
    Simulator::Run();

    // Statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0;
    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    uint32_t totalLostPackets = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        double throughput = i->second.rxBytes * 8.0 / (duration - 1.0) / 1000000.0;
        totalThroughput += throughput;
        totalTxPackets += i->second.txPackets;
        totalRxPackets += i->second.rxPackets;
        totalLostPackets += i->second.lostPackets;
    }

    double pdr = (totalTxPackets > 0) ? (totalRxPackets * 100.0 / totalTxPackets) : 0;
    double plr = (totalTxPackets > 0) ? (totalLostPackets * 100.0 / totalTxPackets) : 0;

    std::cout << "Results:\n";
    std::cout << "  Distance:        " << distance << " m\n";
    std::cout << "  Channel Width:   " << channelWidth << " MHz\n";
    std::cout << "  Throughput:      " << totalThroughput << " Mbps\n";
    std::cout << "  Packets TX:      " << totalTxPackets << "\n";
    std::cout << "  Packets RX:      " << totalRxPackets << "\n";
    std::cout << "  Packets Lost:    " << totalLostPackets << "\n";
    std::cout << "  PDR:             " << pdr << " %\n";
    std::cout << "  PLR:             " << plr << " %\n\n";

    // Save to file
    std::string filename = "distance-" + std::to_string(channelWidth) + "mhz.dat";
    std::ofstream outFile(filename, std::ios::app);
    outFile << distance << " " << totalThroughput << " " << plr << "\n";
    outFile.close();

    std::cout << "Data saved to: " << filename << "\n";
    std::cout << "NetAnim file: " << animFile << "\n";

    Simulator::Destroy();
    return 0;
}