/*
 * TP2 - Question 4 : Deux réseaux WiFi
 */

 #include "ns3/applications-module.h"
 #include "ns3/core-module.h"
 #include "ns3/internet-module.h"
 #include "ns3/mobility-module.h"
 #include "ns3/network-module.h"
 #include "ns3/point-to-point-module.h"
 #include "ns3/ssid.h"
 #include "ns3/yans-wifi-helper.h"
 #include "ns3/netanim-module.h"
 #include <fstream>
 #include <vector>
 
 using namespace ns3;
 
 NS_LOG_COMPONENT_DEFINE("Question4");
 
 std::vector<double> g_delays;
 Time g_lastSend;
 
 void TxTrace(Ptr<const Packet> packet)
 {
     g_lastSend = Simulator::Now();
 }
 
 void RxTrace(Ptr<const Packet> packet)
 {
     Time delay = Simulator::Now() - g_lastSend;
     g_delays.push_back(delay.GetMilliSeconds());
 }
 
 int main(int argc, char* argv[])
 {
     uint32_t nWifi = 4;
     uint32_t nPackets = 10;
     bool tracing = true;
 
     CommandLine cmd(__FILE__);
     cmd.AddValue("nWifi", "Number of wifi STA devices per network (max 9)", nWifi);
     cmd.AddValue("nPackets", "Number of packets to send (max 20)", nPackets);
     cmd.AddValue("tracing", "Enable pcap tracing", tracing);
     cmd.Parse(argc, argv);
 
     if (nWifi > 9)
     {
         std::cout << "nWifi should be 9 or less" << std::endl;
         return 1;
     }
     if (nPackets > 20)
     {
         std::cout << "nPackets should be 20 or less" << std::endl;
         return 1;
     }
 
     std::cout << "Simulation: " << 2 * nWifi << " WiFi nodes (" << nWifi 
               << " per network), " << nPackets << " packets" << std::endl;
 
     // Point-to-Point
     NodeContainer p2pNodes;
     p2pNodes.Create(2);
 
     PointToPointHelper pointToPoint;
     pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
     pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
 
     NetDeviceContainer p2pDevices;
     p2pDevices = pointToPoint.Install(p2pNodes);
 
     // WiFi1
     NodeContainer wifiStaNodes1;
     wifiStaNodes1.Create(nWifi);
     NodeContainer wifiApNode1 = p2pNodes.Get(0);
 
     YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
     YansWifiPhyHelper phy1;
     phy1.SetChannel(channel1.Create());
 
     WifiMacHelper mac1;
     Ssid ssid1 = Ssid("wifi1");
     WifiHelper wifi1;
 
     NetDeviceContainer staDevices1;
     mac1.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
     staDevices1 = wifi1.Install(phy1, mac1, wifiStaNodes1);
 
     NetDeviceContainer apDevices1;
     mac1.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
     apDevices1 = wifi1.Install(phy1, mac1, wifiApNode1);
 
     // WiFi2
     NodeContainer wifiStaNodes2;
     wifiStaNodes2.Create(nWifi);
     NodeContainer wifiApNode2 = p2pNodes.Get(1);
 
     YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default();
     YansWifiPhyHelper phy2;
     phy2.SetChannel(channel2.Create());
 
     WifiMacHelper mac2;
     Ssid ssid2 = Ssid("wifi2");
     WifiHelper wifi2;
 
     NetDeviceContainer staDevices2;
     mac2.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
     staDevices2 = wifi2.Install(phy2, mac2, wifiStaNodes2);
 
     NetDeviceContainer apDevices2;
     mac2.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
     apDevices2 = wifi2.Install(phy2, mac2, wifiApNode2);
 
     // Mobility
     MobilityHelper mobility;
 
     mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue(0.0),
                                   "MinY", DoubleValue(0.0),
                                   "DeltaX", DoubleValue(5.0),
                                   "DeltaY", DoubleValue(10.0),
                                   "GridWidth", UintegerValue(3),
                                   "LayoutType", StringValue("RowFirst"));
     mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
     mobility.Install(wifiStaNodes1);
 
     mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
     mobility.Install(wifiApNode1);
 
     mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue(100.0),
                                   "MinY", DoubleValue(0.0),
                                   "DeltaX", DoubleValue(5.0),
                                   "DeltaY", DoubleValue(10.0),
                                   "GridWidth", UintegerValue(3),
                                   "LayoutType", StringValue("RowFirst"));
     mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(50, 150, -50, 50)));
     mobility.Install(wifiStaNodes2);
 
     mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
     mobility.Install(wifiApNode2);
 
     // Internet stack
     InternetStackHelper stack;
     stack.Install(wifiApNode1);
     stack.Install(wifiApNode2);
     stack.Install(wifiStaNodes1);
     stack.Install(wifiStaNodes2);
 
     // IP addresses
     Ipv4AddressHelper address;
 
     address.SetBase("10.1.1.0", "255.255.255.0");
     Ipv4InterfaceContainer p2pInterfaces;
     p2pInterfaces = address.Assign(p2pDevices);
 
     address.SetBase("10.1.2.0", "255.255.255.0");
     Ipv4InterfaceContainer wifi2Interfaces;
     wifi2Interfaces = address.Assign(staDevices2);
     wifi2Interfaces.Add(address.Assign(apDevices2));
 
     address.SetBase("10.1.3.0", "255.255.255.0");
     Ipv4InterfaceContainer wifi1Interfaces;
     wifi1Interfaces = address.Assign(staDevices1);
     wifi1Interfaces.Add(address.Assign(apDevices1));
 
     // Applications
     UdpEchoServerHelper echoServer(9);
     ApplicationContainer serverApps = echoServer.Install(wifiStaNodes2.Get(nWifi - 1));
     serverApps.Start(Seconds(1.0));
     serverApps.Stop(Seconds(50.0));
 
     UdpEchoClientHelper echoClient(wifi2Interfaces.GetAddress(nWifi - 1), 9);
     echoClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
     echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
     echoClient.SetAttribute("PacketSize", UintegerValue(1024));
 
     ApplicationContainer clientApps = echoClient.Install(wifiStaNodes1.Get(nWifi - 1));
     clientApps.Start(Seconds(2.0));
     clientApps.Stop(Seconds(50.0));
 
     // Traces
     clientApps.Get(0)->TraceConnectWithoutContext("Tx", MakeCallback(&TxTrace));
     serverApps.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&RxTrace));
 
     Ipv4GlobalRoutingHelper::PopulateRoutingTables();
 
     if (tracing)
     {
         phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
         phy2.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
         pointToPoint.EnablePcapAll("q4");
         phy1.EnablePcap("q4-wifi1", apDevices1.Get(0));
         phy2.EnablePcap("q4-wifi2", apDevices2.Get(0));
         
         std::cout << "\nPcap files saved: q4.pcap , q4-wifi1.pcap , q4-wifi2.pcap \n";
     }
 
     // NetAnim
     AnimationInterface anim("q4-animation.xml");
     anim.SetMaxPktsPerTraceFile(500000);
 
     Simulator::Stop(Seconds(50.0));
     Simulator::Run();
     Simulator::Destroy();
 
     // Save data
     std::ofstream dataFile("delay-data.dat");
     dataFile << "# Packet Delay(ms)\n";
     for (size_t i = 0; i < g_delays.size(); i++)
     {
         dataFile << (i + 1) << " " << g_delays[i] << "\n";
         std::cout << "Packet " << (i + 1) << ": " << g_delays[i] << " ms\n";
     }
     dataFile.close();
 
     // Gnuplot script
     std::ofstream gnuFile("plot.gnu");
     gnuFile << "set terminal png size 800,600\n";
     gnuFile << "set output 'delay-plot.png'\n";
     gnuFile << "set title 'End-to-end Delay vs Packet Number'\n";
     gnuFile << "set xlabel 'Packet Number'\n";
     gnuFile << "set ylabel 'Delay (ms)'\n";
     gnuFile << "set grid\n";
     gnuFile << "set key top right\n";
     gnuFile << "plot 'delay-data.dat' using 1:2 with linespoints title 'WiFi-WiFi' lw 2 pt 7 ps 1.5\n";
     gnuFile.close();
 
     int ret = system("gnuplot plot.gnu 2>/dev/null");
     if (ret == 0)
     {
         std::cout << "\nPlot saved: delay-plot.png\n";
     }
     else
     {
         std::cout << "\nData saved: delay-data.dat (run 'gnuplot plot.gnu' manually)\n";
     }
 
     std::cout << "NetAnim file: q4-animation.xml\n";
 
     return 0;
 }