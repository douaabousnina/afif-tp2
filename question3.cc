#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"    

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Third3NetAnim");

int main(int argc, char *argv[])
{
    bool verbose = true;
    uint32_t nWifi = 6;        // Valeur par défaut
    uint32_t nCsma = 4;
    std::string mode = "medium";
    uint32_t intervalUs = 10000; 
    uint32_t packetSize = 1024;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifi", "Nombre de stations WiFi", nWifi);
    cmd.AddValue("nCsma", "Nombre de nœuds CSMA", nCsma);
    cmd.AddValue("mode", "Mode de charge: low, medium, high, extreme", mode);
    cmd.AddValue("verbose", "Activer les logs applicatifs", verbose);
    cmd.Parse(argc, argv);

    // Configuration de l'intervalle selon le mode
    if (mode == "low")        intervalUs = 1000000;   // 1 s
    else if (mode == "medium") intervalUs = 10000;    // 10 ms
    else if (mode == "high")   intervalUs = 500;      // 500 µs

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NS_LOG_UNCOND("=== CONFIGURATION ===");
    NS_LOG_UNCOND("Mode: " << mode);
    NS_LOG_UNCOND("nWifi: " << nWifi << " | nCsma: " << nCsma);
    NS_LOG_UNCOND("Intervalle de base: " << intervalUs << " µs");

    // ========================
    // Création des nœuds
    // ========================
    NodeContainer p2pNodes;
    p2pNodes.Create(2);

    NodeContainer wifiApNode = p2pNodes.Get(0);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);

    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes.Get(1));
    csmaNodes.Create(nCsma - 1); 

    // ========================
    // Installation des canaux
    // ========================
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevices = p2p.Install(p2pNodes);

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
    Ssid ssid = Ssid("TP2-NetAnim");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration", BooleanValue(true));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

    // ========================
    // Mobilité fixe (grille)
    // ========================
    MobilityHelper mobility;
    uint32_t gridWidth = (uint32_t)std::ceil(std::sqrt(nWifi));
    double spacing = 10.0;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(spacing),
                                  "DeltaY", DoubleValue(spacing),
                                  "GridWidth", UintegerValue(gridWidth),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);  // AP fixe

    // ========================
    // Pile IP
    // ========================
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

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ========================
    // Applications (charge normalisée)
    // ========================
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(csmaNodes.Get(nCsma - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(35.0));

    // Normalisation de la charge totale
    uint32_t adjustedInterval = intervalUs * nWifi;
    NS_LOG_UNCOND("Intervalle ajusté par STA: " << adjustedInterval << " µs");
    NS_LOG_UNCOND("Charge totale offerte estimée: ~" << (8.0 * packetSize * nWifi / adjustedInterval) << " Mbps");

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nWifi; ++i)
    {
        UdpEchoClientHelper echoClient(csmaIf.GetAddress(nCsma - 1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(100000));
        echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(adjustedInterval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer clientApp = echoClient.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(2.0 + i * 0.01));  // Décalage pour éviter burst initial
        clientApp.Stop(Seconds(31.0));
        clientApps.Add(clientApp);
    }

    // ========================
    // NetAnim
    // ========================
    AnimationInterface anim("animation_tp2.xml");
    anim.EnablePacketMetadata(true);
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Descriptions et couleurs
    anim.UpdateNodeDescription(wifiApNode.Get(0), "AP-WiFi");
    anim.UpdateNodeColor(wifiApNode.Get(0), 255, 0, 0);        // Rouge

    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        anim.UpdateNodeDescription(wifiStaNodes.Get(i), "STA" + std::to_string(i));
        anim.UpdateNodeColor(wifiStaNodes.Get(i), 0, 0, 255);  // Bleu
    }

    anim.UpdateNodeDescription(p2pNodes.Get(1), "Routeur");
    anim.UpdateNodeColor(p2pNodes.Get(1), 255, 255, 0);        // Jaune

    for (uint32_t i = 0; i < csmaNodes.GetN(); ++i)
    {
        std::string name = (i == csmaNodes.GetN() - 1) ? "Serveur" : "CSMA" + std::to_string(i);
        anim.UpdateNodeDescription(csmaNodes.Get(i), name);
        anim.UpdateNodeColor(csmaNodes.Get(i), 0, 255, 0);      // Vert
    }

    // ========================
    // FlowMonitor
    // ========================
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(36.0));
    NS_LOG_UNCOND("Lancement de la simulation...");
    Simulator::Run();

    // ========================
    // Résultats FlowMonitor
    // ========================
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::cout << "\n=== RÉSULTATS FLOWMONITOR ===\n";
    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        double duration = it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds();
        if (duration <= 0) duration = 29.0;

        double throughput = it->second.rxBytes * 8.0 / duration / 1e6;
        double lossRate = it->second.txPackets > 0 ?
                          100.0 * it->second.lostPackets / it->second.txPackets : 0;

        std::cout << "Flow " << it->first << " : " << t.sourceAddress << " → " << t.destinationAddress << "\n";
        std::cout << "  Throughput: " << throughput << " Mbps | "
                  << "Loss: " << lossRate << "% | "
                  << "Delay: " << (it->second.delaySum.GetSeconds() / it->second.rxPackets * 1000) << " ms\n";
    }

    monitor->SerializeToXmlFile("flowmon_tp2.xml", true, true);
    Simulator::Destroy();
    NS_LOG_UNCOND("Simulation terminée. Fichier NetAnim: animation_tp2.xml");
    return 0;
}
