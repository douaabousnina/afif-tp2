#include "ns3/aodv-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ping-helper.h"
#include "ns3/position-allocator.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/udp-server.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"  // Optionnel si tu veux générer un fichier XML pour NetAnim

#include <iostream>
#include <cmath>
#include <string>
#include <fstream>

using namespace ns3;
using namespace std;

class AodvExample
{
public:
  AodvExample ();
  bool Configure (int argc, char **argv);
  void Run ();
  void Report (std::ostream & os);

private:
  uint32_t size;
  double step;
  double simTime;
  bool pcap;
  bool printRoutes;

  std::string topology = "scratch/manet100.csv";
  double txrange = 50;
  uint32_t interval = 10;
  bool verbose = false;
  bool tracing = true;

  std::string outputFilename = "manet"; // <-- fixed

  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;
  Address serverAddress[50];
  YansWifiPhyHelper wifiPhy ;
  WifiMacHelper wifiMac;

private:
  void CreateNodes ();
  void CreateDevices ();
  void InstallInternetStack ();
  void InstallApplications ();
};

NS_LOG_COMPONENT_DEFINE ("ManetTest");

int main (int argc, char **argv)
{
  AodvExample test;
  if (!test.Configure (argc, argv))
    NS_FATAL_ERROR ("Configuration failed. Aborted.");

  test.Run ();
  test.Report (std::cout);
  return 0;
}

AodvExample::AodvExample () :
  size (10),
  step (5),
  simTime (50),
  pcap (true),
  printRoutes (false)
{
}

bool
AodvExample::Configure (int argc, char **argv)
{
  SeedManager::SetSeed (12345);
  CommandLine cmd;

  cmd.AddValue ("pcap", "Write PCAP traces.", pcap);
  cmd.AddValue ("printRoutes", "Print routing table dumps.", printRoutes);
  cmd.AddValue ("size", "Number of nodes.", size);
  cmd.AddValue ("simTime", "Simulation time, in seconds.", simTime);
  cmd.AddValue ("outputFilename", "Output filename", outputFilename); // <-- string
  cmd.AddValue ("topology", "Topology file.", topology);
  cmd.AddValue ("txrange", "Transmission range per node, in meters.", txrange);
  cmd.AddValue ("interval", "Interval between each iteration.", interval);
  cmd.AddValue ("verbose", "Verbose tracking.", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
  {
    LogComponentEnable ("UdpSocket", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  }

  return true;
}

void
AodvExample::Run ()
{
  CreateNodes ();
  CreateDevices ();
  InstallInternetStack ();
  InstallApplications ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
}

void
AodvExample::Report (std::ostream &)
{
  // optional reporting
}

void
AodvExample::CreateNodes ()
{
  std::cout << "Creating " << (unsigned)size << " nodes with transmission range " << txrange << "m.\n";
  nodes.Create (size);

  for (uint32_t i = 0; i < size; ++i)
  {
    std::ostringstream os;
    os << "node-" << i;
    Names::Add (os.str (), nodes.Get (i));
  }

  Ptr<ListPositionAllocator> positionAllocS = CreateObject<ListPositionAllocator> ();

  std::string line;
  ifstream file(topology);
  uint16_t i = 0;
  double vec[3];

  if(file.is_open())
  {
    while(getline(file,line))
    {
      char seps[] = ",";
      char *token = strtok(&line[0], seps);

      while(token != NULL)
      {
        vec[i] = atof(token);
        i++;
        token = strtok (NULL, ",");
        if(i == 3)
        {
          positionAllocS->Add(Vector(vec[1], vec[2], 0.0));
          i = 0;
        }
      }
    }
    file.close();
  }
  else
  {
    std::cout<<"Error in csv file"<< '\n';
  }

  MobilityHelper mobilityS;
  mobilityS.SetPositionAllocator(positionAllocS);
  mobilityS.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityS.Install(nodes);
}

void
AodvExample::CreateDevices ()
{
  wifiMac.SetType ("ns3::AdhocWifiMac");

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel",
                                "MaxRange", DoubleValue (txrange));
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate6Mbps"),
                                "RtsCtsThreshold", UintegerValue (0));
  devices = wifi.Install (wifiPhy, wifiMac, nodes);

  if (pcap)
  {
    wifiPhy.EnablePcapAll (outputFilename);
  }
}

void
AodvExample::InstallInternetStack ()
{
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.0.0.0");
  interfaces = address.Assign (devices);

  for(uint32_t i = 0; i < (size / 2); i++)
  {
    serverAddress[i] = Address (interfaces.GetAddress (i));
  }
}

void
AodvExample::InstallApplications ()
{
  uint16_t i, j, k;
  uint16_t port = 4000;
  UdpServerHelper server (port);
  ApplicationContainer apps;

  for(i = 0; i < (size / 2); i++)
  {
    apps = server.Install (nodes.Get (i));
  }
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (simTime));

  uint32_t MaxPacketSize = 1024;
  Time interPacketInterval = Seconds (0.01);
  uint32_t maxPacketCount = 3;
  double interval_start = 2.0, interval_end = interval_start + interval;

  for(k = 1; k <= (size / 2); k++)
  {
    for(i = 0; i < k; i++)
    {
      UdpClientHelper client (serverAddress[i], port);
      client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
      client.SetAttribute ("Interval", TimeValue (interPacketInterval));
      client.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
      for(j = (size / 2); j < ((size / 2) + k); j++)
      {
        apps = client.Install (nodes.Get (j));
      }
    }
    apps.Start (Seconds (interval_start));
    apps.Stop (Seconds (interval_end));
    interval_start = interval_end + 1.0;
    interval_end = interval_start + interval;
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (10.0));

  if (tracing)
  {
    wifiPhy.EnablePcapAll (outputFilename);

  }
  AnimationInterface anim("manet-28.xml"); // Génère un fichier XML pour NetAnim
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  uint32_t rxPacketsum = 0, txPacketsum = 0, txBytessum = 0, rxBytessum = 0, lostPacketssum = 0;
  double Delaysum = 0;
  uint32_t txTimeFirst = 0, rxTimeLast = 0;
  k = 0;

  for (auto i = stats.begin (); i != stats.end (); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    rxPacketsum += i->second.rxPackets;
    txPacketsum += i->second.txPackets;
    txBytessum += i->second.txBytes;
    rxBytessum += i->second.rxBytes;
    Delaysum += i->second.delaySum.GetSeconds();
    lostPacketssum += i->second.lostPackets;
    txTimeFirst += i->second.timeFirstTxPacket.GetSeconds();
    rxTimeLast += i->second.timeLastRxPacket.GetSeconds();
  }

  uint64_t timeDiff = (rxTimeLast - txTimeFirst);

  std::cout << "\n\n";
  std::cout << "  Total Packets Lost: " << lostPacketssum << "\n";
  std::cout << "  Throughput: " << ((rxBytessum * 8.0) / timeDiff)/1024<<" Kbps"<<"\n";
  std::cout << "  Packets Delivery Ratio: " << (((txPacketsum - lostPacketssum) * 100) /txPacketsum) << "%" << "\n";

  Simulator::Destroy ();
}
