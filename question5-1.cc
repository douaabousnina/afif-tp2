/*
 * Question 5.1 : Analyse des débits avec MIMO
 * Compare 1 flux spatial vs 2 flux spatiaux
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

NS_LOG_COMPONENT_DEFINE("MimoQ1");

int main(int argc, char *argv[])
{
    uint32_t nStreams = 1;
    double duration = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nStreams", "Number of spatial streams (1 or 2)", nStreams);
    cmd.AddValue("duration", "Simulation duration (seconds)", duration);
    cmd.Parse(argc, argv);

    std::cout << "\n========================================\n";
    std::cout << "MIMO Test: " << nStreams << " spatial stream(s)\n";
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

    // WiFi 802.11n 5GHz
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    std::string mcs = (nStreams == 1) ? "HtMcs7" : "HtMcs15";
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(mcs),
                                 "ControlMode", StringValue("HtMcs0"));

    // MAC - Configure streams via helper
    WifiMacHelper mac;
    Ssid ssid = Ssid("mimo-test");

    // STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    phy.Set("ChannelSettings", StringValue("{0, 20, BAND_5GHZ, 0}"));
    phy.Set("Antennas", UintegerValue(nStreams));
    phy.Set("MaxSupportedTxSpatialStreams", UintegerValue(nStreams));
    phy.Set("MaxSupportedRxSpatialStreams", UintegerValue(nStreams));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    // AP
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility - STA at 5m from AP
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP
    positionAlloc->Add(Vector(5.0, 0.0, 0.0)); // STA
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

    // UDP traffic: STA -> AP
    uint16_t port = 7;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(duration));

    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(100000));
    client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(duration));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // NetAnim
    std::string animFile = "mimo-q1-" + std::to_string(nStreams) + "stream.xml";
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

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        double throughput = i->second.rxBytes * 8.0 / (duration - 1.0) / 1000000.0;
        totalThroughput += throughput;
        totalTxPackets += i->second.txPackets;
        totalRxPackets += i->second.rxPackets;
    }

    double pdr = (totalTxPackets > 0) ? (totalRxPackets * 100.0 / totalTxPackets) : 0;

    std::cout << "Results:\n";
    std::cout << "  Spatial Streams: " << nStreams << "\n";
    std::cout << "  MCS Used:        " << mcs << "\n";
    std::cout << "  Throughput:      " << totalThroughput << " Mbps\n";
    std::cout << "  Packets TX:      " << totalTxPackets << "\n";
    std::cout << "  Packets RX:      " << totalRxPackets << "\n";
    std::cout << "  PDR:             " << pdr << " %\n\n";

    // Save to file
    std::ofstream outFile("mimo-results.txt", std::ios::app);
    outFile << nStreams << " " << totalThroughput << " " << pdr << "\n";
    outFile.close();

    std::cout << "Data saved to: mimo-results.txt\n";
    std::cout << "NetAnim file: " << animFile << "\n";

    // Activation de Wireshark
    // ======================

    // Capture sur l'interface AP
    std::string pcapPrefix = "mimo-q1-" + std::to_string(nStreams) + "stream";
    phy.EnablePcap(pcapPrefix, apDevice.Get(0));
    phy.EnablePcap(pcapPrefix + "-sta", staDevice.Get(0));

    std::cout << "Fichiers PCAP générés : " << pcapPrefix << "*.pcap\n";

    Simulator::Destroy();

    // Generate plot if 2 streams test completed
    std::ifstream checkFile("mimo-results.txt");
    int lineCount = 0;
    std::string line;
    while (std::getline(checkFile, line))
        lineCount++;
    checkFile.close();

    if (lineCount >= 2)
    {
        std::ofstream gnuFile("plot-mimo-q1.gnu");
        gnuFile << "set terminal png size 800,600\n";
        gnuFile << "set output 'mimo-q1-throughput.png'\n";
        gnuFile << "set title 'Débit selon le nombre de flux spatiaux (MIMO 2x2)'\n";
        gnuFile << "set xlabel 'Nombre de flux spatiaux'\n";
        gnuFile << "set ylabel 'Débit (Mbps)'\n";
        gnuFile << "set grid\n";
        gnuFile << "set xrange [0:3]\n";
        gnuFile << "set xtics 1\n";
        gnuFile << "set yrange [0:150]\n";
        gnuFile << "set style fill solid\n";
        gnuFile << "set boxwidth 0.5\n";
        gnuFile << "plot 'mimo-results.txt' using 1:2 with boxes title 'Débit' lc rgb 'blue'\n";
        gnuFile.close();

        int ret = system("gnuplot plot-mimo-q1.gnu 2>/dev/null");
        if (ret == 0)
        {
            std::cout << "\nGraphique généré: mimo-q1-throughput.png\n";
        }
    }

    return 0;
}