#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Third3");

int main(int argc, char *argv[])
{
    bool verbose = true;
    uint32_t nWifi = 3;
    uint32_t nCsma = 3;
    bool tracing = false;
    std::string mode = "low";        
    uint32_t intervalUs = 1000000;
    uint32_t packetSize = 1024;
    DataRate cbrRate("6Mbps");       

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifi", "Nombre de STA WiFi", nWifi);
    cmd.AddValue("nCsma", "Nombre de noeuds CSMA", nCsma);
    cmd.AddValue("mode", "Mode de charge: low, medium, high, extreme", mode);
    cmd.AddValue("intervalUs", "Intervalle entre paquets (µs) pour modes echo", intervalUs);
    cmd.AddValue("packetSize", "Taille des paquets", packetSize);
    cmd.AddValue("cbrRate", "Débit CBR pour le mode cbr", cbrRate);
    cmd.AddValue("tracing", "Activer pcap + flowmonitor", tracing);
    cmd.AddValue("verbose", "Logs des applications", verbose);
    cmd.Parse(argc, argv);

    // Configuration de l'intervalle selon le mode
    if (mode == "low")
    {
        intervalUs = 1000000;   // 1 s
    }
    else if (mode == "medium")
    {
        intervalUs = 10000;     // 10 ms
    }
    else if (mode == "high")
    {
        intervalUs = 500;       // 500 µs
    }

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }

    NS_LOG_UNCOND("=== CONFIGURATION ===");
    NS_LOG_UNCOND("Mode: " << mode);
    NS_LOG_UNCOND("nWifi: " << nWifi);
    NS_LOG_UNCOND("nCsma: " << nCsma);
    NS_LOG_UNCOND("Intervalle: " << intervalUs << " µs");
    NS_LOG_UNCOND("Taille paquet: " << packetSize << " bytes");

    // Création des nœuds
    NodeContainer p2pNodes;
    p2pNodes.Create(2);
    
    NodeContainer wifiApNode = p2pNodes.Get(0);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);

    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes.Get(1));
    csmaNodes.Create(nCsma);

    NS_LOG_UNCOND("Noeuds créés: " << nWifi << " STAs WiFi, " << nCsma << " noeuds CSMA");

    // P2P 5 Mbps / 2 ms
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevices = p2p.Install(p2pNodes);

    // CSMA 100 Mbps
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    // WiFi 802.11a
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("TP2-Net");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration", BooleanValue(true));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);
    MobilityHelper mobility;
    // Calcul dynamique de la zone selon le nombre de nœuds
    uint32_t gridWidth = (uint32_t)std::ceil(std::sqrt(nWifi));
    double spacing = 10.0;  // Espacement entre les nœuds
    
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0), 
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(spacing), 
                                  "DeltaY", DoubleValue(spacing),
                                  "GridWidth", UintegerValue(gridWidth), 
                                  "LayoutType", StringValue("RowFirst"));
    
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    // AP fixe
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    // Pile IP
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pIf = address.Assign(p2pDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaIf = address.Assign(csmaDevices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiIf = address.Assign(staDevices);
    address.Assign(apDevices);

    NS_LOG_UNCOND("Adresse du serveur CSMA: " << csmaIf.GetAddress(nCsma));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    uint16_t port = 9;
    ApplicationContainer clientApps, serverApps;

    UdpEchoServerHelper echoServer(port);
    serverApps = echoServer.Install(csmaNodes.Get(nCsma));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(35.0));
    uint32_t adjustedInterval = intervalUs * nWifi;
    NS_LOG_UNCOND("Intervalle par nœud ajusté: " << adjustedInterval << " µs");
    NS_LOG_UNCOND("Charge totale théorique: ~" << (8.0 * packetSize / adjustedInterval) << " Mbps");
    for (uint32_t i = 0; i < nWifi; i++)
    {
        UdpEchoClientHelper echoClient(csmaIf.GetAddress(nCsma), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(100000));
        echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(adjustedInterval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        
        ApplicationContainer clientApp = echoClient.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(2.0 + i * 0.01));  // Décalage de 10ms entre clients
        clientApp.Stop(Seconds(31.0));
        
        clientApps.Add(clientApp);
    }

    NS_LOG_UNCOND("Applications configurées: " << nWifi << " clients WiFi actifs");

    // Tracing + FlowMonitor
    if (tracing)
    {
        p2p.EnablePcapAll("tracemetrics", true);
        csma.EnablePcapAll("tracemetrics", true);
        phy.EnablePcapAll("tracemetrics", true);
        AsciiTraceHelper ascii;
        phy.EnableAsciiAll(ascii.CreateFileStream("tracemetrics-wifi.tr"));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(35.0));  // 31s + marge de 4s
    
    NS_LOG_UNCOND("Lancement de la simulation...");
    Simulator::Run();
    NS_LOG_UNCOND("Simulation terminée");

    // Résultats FlowMonitor
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "RÉSULTATS - Mode = " << mode << " | Intervalle = " << intervalUs << " µs\n";
    std::cout << "nWifi = " << nWifi << " | nCsma = " << nCsma << "\n";
    std::cout << "============================================================\n";

    double totalRxBytes = 0;
    for (auto i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        double duration = i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds();
        if (duration <= 0) duration = 9.0; 

        double throughput = i->second.rxBytes * 8.0 / duration / 1e6;
        totalRxBytes += i->second.rxBytes;

        std::cout << "Flow " << i->first << " : " << t.sourceAddress << " → " << t.destinationAddress << "\n";
        std::cout << "  Tx/Rx/Lost   : " << i->second.txPackets << " / "
                  << i->second.rxPackets << " / " << i->second.lostPackets << "\n";
        std::cout << "  Throughput   : " << throughput << " Mbps\n";
        std::cout << "  Loss rate    : " << (i->second.txPackets > 0 ?
                  100.0 * i->second.lostPackets / i->second.txPackets : 0) << " %\n";
        std::cout << "  Mean delay   : " << (i->second.rxPackets > 0 ?
                  i->second.delaySum.GetSeconds() / i->second.rxPackets * 1000 : 0) << " ms\n";
        std::cout << "  --------------------------------------------------\n";
    }

    std::cout << "Nombre total de flux: " << stats.size() << "\n";
    std::cout << "============================================================\n";

    monitor->SerializeToXmlFile("saturation_flowmon.xml", true, true);
    Simulator::Destroy();
    return 0;
}
