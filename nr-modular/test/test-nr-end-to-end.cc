#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

// Include your custom managers
#include "ns3/nr-modular-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NrEndToEndTest");

int main(int argc, char *argv[])
{
    // 1. Setup default parameters
    uint32_t nUes = 5;
    double simTime = 10.0;
    std::string animFile = "nr-simulation.xml";

    CommandLine cmd;
    cmd.AddValue("nUes", "Number of UEs", nUes);
    cmd.AddValue("simTime", "Simulation time", simTime);
    cmd.Parse(argc, argv);

    // 2. Initialize Configuration Manager
    // (Assuming you have a way to create a Ptr<NrSimConfig>)
    Ptr<NrSimConfig> config = CreateObject<NrSimConfig>();
    config->simDuration = simTime;
    config->traffic.udpRateDl = 2.0; // 2 Mbps
    config->traffic.udpRateUl = 1.0; // 1 Mbps

    // 3. Deploy Topology (Nodes and Mobility)
    Ptr<NrTopologyManager> topologyManager = CreateObject<NrTopologyManager>();
    topologyManager->SetConfig(config);
    topologyManager->DeployTopology(); 

    // 4. Setup NR Network (Core, gNBs, UEs)
    Ptr<NrNetworkManager> networkManager = CreateObject<NrNetworkManager>();
    networkManager->SetConfig(config);
    networkManager->SetupNrInfrastructure();
    networkManager->InstallNrDevices(topologyManager->GetGnbNodes(), topologyManager->GetUeNodes());

    // 5. Install Traffic Applications
    Ptr<NrTrafficManager> trafficManager = CreateObject<NrTrafficManager>();
    trafficManager->SetConfig(config);
    trafficManager->SetNetworkManager(networkManager);
    
    // IMPORTANT: Start traffic at 2.0s to allow UE attachment to complete
    trafficManager->InstallTraffic(topologyManager->GetGnbNodes(), topologyManager->GetUeNodes());

    // 6. Enable FlowMonitor for analysis
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // 7. Run Simulation
    NS_LOG_INFO("Starting Simulation...");
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // 8. Process Results
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n--- Final Results ---" << std::endl;
    for (auto const& [flowId, flowStats] : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        double throughput = (flowStats.rxBytes * 8.0) / 
                            (flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds()) / 1024 / 1024;
        
        std::cout << "Flow " << flowId << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n"
                  << "  Tx Bytes: " << flowStats.txBytes << "\n"
                  << "  Rx Bytes: " << flowStats.rxBytes << "\n"
                  << "  Throughput: " << throughput << " Mbps\n"
                  << "  Loss Rate: " << (1.0 - (double)flowStats.rxPackets / flowStats.txPackets) * 100 << "%\n";
    }

    Simulator::Destroy();
    return 0;
}