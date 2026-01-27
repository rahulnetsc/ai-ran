/*
 * Copyright (c) 2026 ARTPARK
 * 
 * End-to-end test for waypoint mobility with UDP traffic
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/nr-modular-module.h"

#include <iostream>
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TestWaypointEndToEnd");

// Position tracking for waypoint verification
void
TrackPositions(NodeContainer ueNodes, uint32_t numWaypointUes, std::ofstream& posFile)
{
    for (uint32_t i = 0; i < numWaypointUes && i < ueNodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mob = ueNodes.Get(i)->GetObject<MobilityModel>();
        if (mob)
        {
            Vector pos = mob->GetPosition();
            posFile << Simulator::Now().GetSeconds() << ","
                    << i << ","
                    << pos.x << ","
                    << pos.y << ","
                    << pos.z << std::endl;
        }
    }
}

int
main(int argc, char* argv[])
{
    std::string configFile = "input/test-waypoints-config.json";
    
    CommandLine cmd;
    cmd.AddValue("config", "Path to configuration JSON file", configFile);
    cmd.Parse(argc, argv);

    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║      END-TO-END WAYPOINT MOBILITY + TRAFFIC TEST               ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";

    // =================================================================
    // STEP 1: Load Configuration
    // =================================================================
    std::cout << "Step 1: Loading configuration from " << configFile << "...\n";
    
    Ptr<NrConfigManager> configMgr = CreateObject<NrConfigManager>();
    Ptr<NrSimConfig> config = configMgr->LoadFromFile(configFile);
    configMgr->ValidateOrAbort(config);
    
    std::cout << "✓ Configuration loaded and validated\n\n";
    
    // Print configuration
    config->Print(std::cout);
    std::cout << std::endl;

    // =================================================================
    // STEP 2: Deploy Topology
    // =================================================================
    std::cout << "Step 2: Deploying network topology...\n";
    
    Ptr<NrTopologyManager> topoMgr = CreateObject<NrTopologyManager>();
    topoMgr->SetConfig(config);
    topoMgr->DeployTopology();
    
    NodeContainer gnbNodes = topoMgr->GetGnbNodes();
    NodeContainer ueNodes = topoMgr->GetUeNodes();
    
    std::cout << "✓ Topology deployed: " << gnbNodes.GetN() << " gNBs, " 
              << ueNodes.GetN() << " UEs\n\n";

    // =================================================================
    // STEP 3: Install Mobility
    // =================================================================
    std::cout << "Step 3: Installing mobility models...\n";
    
    Ptr<NrMobilityManager> mobMgr = CreateObject<NrMobilityManager>();
    mobMgr->SetConfig(config);
    mobMgr->InstallUeMobility(ueNodes);
    
    std::cout << "✓ Mobility models installed\n\n";

    // =================================================================
    // STEP 4: Install Traffic
    // =================================================================
    std::cout << "Step 4: Installing UDP traffic...\n";
    
    Ptr<NrTrafficManager> trafficMgr = CreateObject<NrTrafficManager>();
    trafficMgr->SetConfig(config);
    trafficMgr->InstallTraffic(gnbNodes, ueNodes);
    
    std::cout << "✓ Traffic installed\n\n";

    // =================================================================
    // STEP 5: Setup Position Tracking
    // =================================================================
    std::cout << "Step 5: Setting up position tracking...\n";
    
    std::ofstream posFile("output/ue_positions.csv");
    posFile << "time,ue_id,x,y,z\n";
    
    uint32_t numWaypointUes = config->mobility.ueWaypoints.size();
    
    for (double t = 0.0; t <= config->simDuration; t += 1.0)
    {
        Simulator::Schedule(Seconds(t),
                          &TrackPositions,
                          ueNodes,
                          numWaypointUes,
                          std::ref(posFile));
    }
    
    std::cout << "✓ Position tracking scheduled (every 1s)\n\n";

    // =================================================================
    // STEP 6: Run Simulation
    // =================================================================
    std::cout << "Step 6: Running simulation for " << config->simDuration << " seconds...\n";
    std::cout << "(Tracking first " << numWaypointUes << " UEs with waypoints)\n\n";
    
    Simulator::Stop(Seconds(config->simDuration));
    Simulator::Run();
    
    posFile.close();
    
    std::cout << "\n✓ Simulation complete\n\n";

    // =================================================================
    // STEP 7: Verify Waypoints
    // =================================================================
    std::cout << "Step 7: Verifying waypoints were reached...\n\n";
    
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              WAYPOINT VERIFICATION                             ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    
    const double TOLERANCE = 5.0; // meters
    uint32_t passCount = 0;
    
    for (const auto& [ueId, wpConfig] : config->mobility.ueWaypoints)
    {
        if (ueId >= ueNodes.GetN())
            continue;
            
        Ptr<MobilityModel> mob = ueNodes.Get(ueId)->GetObject<MobilityModel>();
        if (!mob)
            continue;
            
        Vector finalPos = mob->GetPosition();
        Vector targetPos = wpConfig.waypoints.back();
        
        double dx = finalPos.x - targetPos.x;
        double dy = finalPos.y - targetPos.y;
        double dz = finalPos.z - targetPos.z;
        double distance = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        bool passed = (distance < TOLERANCE);
        if (passed) passCount++;
        
        std::cout << "UE " << ueId << ":\n";
        std::cout << "  Waypoints: " << wpConfig.waypoints.size() << "\n";
        std::cout << "  Speed: " << wpConfig.speed << " m/s\n";
        std::cout << "  Expected final position: (" << targetPos.x << ", " 
                  << targetPos.y << ", " << targetPos.z << ")\n";
        std::cout << "  Actual final position: (" << finalPos.x << ", " 
                  << finalPos.y << ", " << finalPos.z << ")\n";
        std::cout << "  Distance from target: " << distance << " m\n";
        std::cout << "  Status: " << (passed ? "✓ PASS" : "✗ FAIL");
        std::cout << (passed ? " (reached waypoint)" : " (did not reach waypoint)") << "\n\n";
    }

    // =================================================================
    // STEP 8: Print Summary
    // =================================================================
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              TEST SUMMARY                                      ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "Total UEs: " << ueNodes.GetN() << "\n";
    std::cout << "  Waypoint mobility: " << numWaypointUes << " UEs\n";
    std::cout << "  Default mobility: " << (ueNodes.GetN() - numWaypointUes) << " UEs\n\n";
    
    std::cout << "Traffic:\n";
    std::cout << "  DL: " << config->traffic.udpRateDl << " Mbps per UE\n";
    std::cout << "  UL: " << config->traffic.udpRateUl << " Mbps per UE\n\n";
    
    std::cout << "Waypoint verification:\n";
    std::cout << "  Passed: " << passCount << "/" << numWaypointUes << "\n\n";
    
    std::cout << "Simulation duration: " << config->simDuration << " seconds\n\n";

    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              TEST COMPLETE                                     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "Output files:\n";
    std::cout << "  - output/ue_positions.csv (position tracking)\n";
    if (config->enableFlowMonitor)
    {
        std::cout << "  - " << config->outputFilePath << " (metrics)\n";
    }
    std::cout << std::endl;

    // Cleanup
    Simulator::Destroy();
    
    return 0;
}