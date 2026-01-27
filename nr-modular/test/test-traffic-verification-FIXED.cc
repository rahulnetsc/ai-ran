/*
 * Copyright (c) 2026 ARTPARK
 * 
 * CORRECTED End-to-end test matching actual NrSimConfig API
 * Fixed all compilation errors
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/nr-modular-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/packet-sink.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <chrono>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TestTrafficVerificationFixed");

// ===================================================================
// TIMING HELPER
// ===================================================================
void
PrintStepTime(const std::string& stepName, 
              std::chrono::high_resolution_clock::time_point& lastStepTime)
{
    auto now = std::chrono::high_resolution_clock::now();
    auto stepDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStepTime);
    
    std::cout << "  [Completed in " << std::fixed << std::setprecision(2) 
              << (stepDuration.count() / 1000.0) << "s]\n\n";
    
    lastStepTime = now;
}

// ===================================================================
// POSITION TRACKING
// ===================================================================
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

// ===================================================================
// UE-GNB PROXIMITY ANALYSIS
// ===================================================================
void
PrintUeGnbAssociations(NodeContainer gnbNodes, NodeContainer ueNodes)
{
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           UE-gNB PROXIMITY ANALYSIS                       ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    std::map<uint32_t, uint32_t> gnbUeCount;
    
    for (uint32_t ueId = 0; ueId < ueNodes.GetN(); ++ueId)
    {
        Ptr<MobilityModel> ueMob = ueNodes.Get(ueId)->GetObject<MobilityModel>();
        if (!ueMob) continue;
        
        Vector uePos = ueMob->GetPosition();
        
        double minDist = std::numeric_limits<double>::max();
        uint32_t nearestGnb = 0;
        
        for (uint32_t gnbId = 0; gnbId < gnbNodes.GetN(); ++gnbId)
        {
            Ptr<MobilityModel> gnbMob = gnbNodes.Get(gnbId)->GetObject<MobilityModel>();
            if (!gnbMob) continue;
            
            Vector gnbPos = gnbMob->GetPosition();
            double dx = uePos.x - gnbPos.x;
            double dy = uePos.y - gnbPos.y;
            double dz = uePos.z - gnbPos.z;
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            if (dist < minDist)
            {
                minDist = dist;
                nearestGnb = gnbId;
            }
        }
        
        gnbUeCount[nearestGnb]++;
        
        std::cout << "UE " << std::setw(2) << ueId << ": ";
        std::cout << "Nearest gNB " << nearestGnb << " ";
        std::cout << "(distance: " << std::fixed << std::setprecision(1) 
                  << minDist << " m)" << std::endl;
    }
    
    std::cout << "\n--- gNB Load Distribution ---\n";
    for (uint32_t gnbId = 0; gnbId < gnbNodes.GetN(); ++gnbId)
    {
        uint32_t count = gnbUeCount[gnbId];
        std::cout << "gNB " << gnbId << ": " << count << " UEs [";
        for (uint32_t i = 0; i < count; ++i) std::cout << "█";
        std::cout << "]" << std::endl;
    }
    std::cout << std::endl;
}

// ===================================================================
// PACKET RECEPTION VERIFICATION - FIXED VERSION
// ===================================================================
void
VerifyPacketReception(Ptr<NrTrafficManager> trafficMgr, uint32_t numUes)
{
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           PACKET RECEPTION VERIFICATION (FIXED)           ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    // Get separate containers (NEW CORRECT WAY)
    ApplicationContainer dlSinks = trafficMgr->GetDlServerApps();
    ApplicationContainer ulSinks = trafficMgr->GetUlServerApps();
    
    std::cout << "Application counts:\n";
    std::cout << "  DL sinks: " << dlSinks.GetN() << " (expected: " << numUes << ")\n";
    std::cout << "  UL sinks: " << ulSinks.GetN() << " (expected: " << numUes << ")\n\n";
    
    uint32_t dlSinksWithData = 0;
    uint32_t ulSinksWithData = 0;
    uint64_t totalDlBytes = 0;
    uint64_t totalUlBytes = 0;
    
    // ===== DOWNLINK VERIFICATION =====
    std::cout << "--- Downlink Traffic (Remote → UEs) ---\n";
    for (uint32_t i = 0; i < numUes && i < dlSinks.GetN(); ++i)
    {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(dlSinks.Get(i));
        if (sink)
        {
            uint64_t bytes = sink->GetTotalRx();
            totalDlBytes += bytes;
            
            std::cout << "  UE " << std::setw(2) << i << ": ";
            std::cout << std::setw(12) << bytes << " bytes ";
            
            if (bytes > 0)
            {
                dlSinksWithData++;
                std::cout << "(" << std::fixed << std::setprecision(2) 
                          << (bytes / 1024.0 / 1024.0) << " MB) ✓";
            }
            else
            {
                std::cout << "✗ NO DATA!";
            }
            std::cout << std::endl;
        }
        else
        {
            std::cout << "  UE " << std::setw(2) << i << ": ✗ SINK CAST FAILED!\n";
        }
    }
    
    // ===== UPLINK VERIFICATION =====
    std::cout << "\n--- Uplink Traffic (UEs → Remote) ---\n";
    for (uint32_t i = 0; i < numUes && i < ulSinks.GetN(); ++i)
    {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(ulSinks.Get(i));
        if (sink)
        {
            uint64_t bytes = sink->GetTotalRx();
            totalUlBytes += bytes;
            
            std::cout << "  UE " << std::setw(2) << i << ": ";
            std::cout << std::setw(12) << bytes << " bytes ";
            
            if (bytes > 0)
            {
                ulSinksWithData++;
                std::cout << "(" << std::fixed << std::setprecision(2) 
                          << (bytes / 1024.0 / 1024.0) << " MB) ✓";
            }
            else
            {
                std::cout << "✗ NO DATA!";
            }
            std::cout << std::endl;
        }
        else
        {
            std::cout << "  UE " << std::setw(2) << i << ": ✗ SINK CAST FAILED!\n";
        }
    }
    
    // ===== SUMMARY =====
    std::cout << "\n--- Summary ---\n";
    std::cout << "DL: " << dlSinksWithData << "/" << numUes << " UEs received data ";
    std::cout << "(Total: " << std::fixed << std::setprecision(2) 
              << (totalDlBytes / 1024.0 / 1024.0) << " MB)";
    if (dlSinksWithData == numUes)
    {
        std::cout << " ✓ SUCCESS!";
    }
    else if (dlSinksWithData == 0)
    {
        std::cout << " ✗ CRITICAL: NO DOWNLINK TRAFFIC!";
    }
    else
    {
        std::cout << " ⚠️  PARTIAL TRAFFIC";
    }
    std::cout << std::endl;
    
    std::cout << "UL: " << ulSinksWithData << "/" << numUes << " UEs sent data ";
    std::cout << "(Total: " << (totalUlBytes / 1024.0 / 1024.0) << " MB)";
    if (ulSinksWithData == numUes)
    {
        std::cout << " ✓ SUCCESS!";
    }
    else if (ulSinksWithData == 0)
    {
        std::cout << " ✗ CRITICAL: NO UPLINK TRAFFIC!";
    }
    else
    {
        std::cout << " ⚠️  PARTIAL TRAFFIC";
    }
    std::cout << std::endl;
    
    // ===== DIAGNOSTIC INFO =====
    if (dlSinksWithData < numUes || ulSinksWithData < numUes)
    {
        std::cout << "\n⚠️  WARNING: Some UEs have no traffic!\n";
        std::cout << "    Possible causes:\n";
        std::cout << "    - Routing issue between remote host and PGW\n";
        std::cout << "    - IP address mismatch\n";
        std::cout << "    - Application start time too early\n";
        std::cout << "    - High packet loss due to poor channel conditions\n";
        std::cout << "    - UE not properly attached to gNB\n";
    }
    else
    {
        std::cout << "\n✓✓✓ ALL UEs HAVE BIDIRECTIONAL TRAFFIC! ✓✓✓\n";
    }
    
    std::cout << std::endl;
}

// ===================================================================
// FLOWMONITOR ANALYSIS - FIXED IP CONVERSION
// ===================================================================
void
AnalyzeFlowMonitor(Ptr<FlowMonitor> monitor, 
                  Ptr<Ipv4FlowClassifier> classifier,
                  uint32_t numUes)
{
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           FLOWMONITOR ANALYSIS                             ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    std::cout << "Total flows detected: " << stats.size() << "\n\n";
    
    // Organize flows by UE
    std::map<uint32_t, double> ueDlThroughput;
    std::map<uint32_t, double> ueUlThroughput;
    std::map<uint32_t, double> ueDlDelay;
    std::map<uint32_t, double> ueUlDelay;
    std::map<uint32_t, double> ueDlLoss;
    std::map<uint32_t, double> ueUlLoss;
    
    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        
        double duration = (it->second.timeLastRxPacket.GetSeconds() - 
                          it->second.timeFirstTxPacket.GetSeconds());
        double throughput = 0.0;
        if (duration > 0)
        {
            throughput = (it->second.rxBytes * 8.0) / duration / 1e6; // Mbps
        }
        
        double avgDelay = 0.0;
        if (it->second.rxPackets > 0)
        {
            avgDelay = (it->second.delaySum.GetMilliSeconds() / it->second.rxPackets);
        }
        
        double lossRate = 0.0;
        if (it->second.txPackets > 0)
        {
            lossRate = 100.0 * (it->second.txPackets - it->second.rxPackets) / 
                       it->second.txPackets;
        }
        
        // FIXED: Convert Ipv4Address to string properly
        std::ostringstream dstStream, srcStream;
        t.destinationAddress.Print(dstStream);
        t.sourceAddress.Print(srcStream);
        std::string dstIp = dstStream.str();
        std::string srcIp = srcStream.str();
        
        // DL: destination is 7.0.0.x (UE)
        // UL: destination is 1.0.0.2 (remote host)
        if (dstIp.substr(0, 2) == "7.")
        {
            // Downlink - extract UE ID from IP (7.0.0.2 = UE 0, 7.0.0.3 = UE 1, etc.)
            uint32_t ueId = std::stoi(dstIp.substr(dstIp.rfind('.') + 1)) - 2;
            if (ueId < numUes)
            {
                ueDlThroughput[ueId] += throughput;
                ueDlDelay[ueId] = avgDelay;
                ueDlLoss[ueId] = lossRate;
            }
        }
        else if (dstIp.substr(0, 2) == "1.")
        {
            // Uplink - extract UE ID from source IP
            if (srcIp.substr(0, 2) == "7.")
            {
                uint32_t ueId = std::stoi(srcIp.substr(srcIp.rfind('.') + 1)) - 2;
                if (ueId < numUes)
                {
                    ueUlThroughput[ueId] += throughput;
                    ueUlDelay[ueId] = avgDelay;
                    ueUlLoss[ueId] = lossRate;
                }
            }
        }
    }
    
    // Print per-UE results
    std::cout << "UE | DL (Mbps) | UL (Mbps) | DL Delay | UL Delay | DL Loss | UL Loss\n";
    std::cout << "---+-----------+-----------+----------+----------+---------+---------\n";
    
    uint32_t dlActive = 0, ulActive = 0;
    double totalDlTput = 0.0, totalUlTput = 0.0;
    
    for (uint32_t i = 0; i < numUes; ++i)
    {
        double dlTput = ueDlThroughput[i];
        double ulTput = ueUlThroughput[i];
        
        if (dlTput > 0) { dlActive++; totalDlTput += dlTput; }
        if (ulTput > 0) { ulActive++; totalUlTput += ulTput; }
        
        std::cout << std::setw(2) << i << " | ";
        std::cout << std::setw(9) << std::fixed << std::setprecision(2) << dlTput << " | ";
        std::cout << std::setw(9) << ulTput << " | ";
        std::cout << std::setw(8) << ueDlDelay[i] << "ms | ";
        std::cout << std::setw(8) << ueUlDelay[i] << "ms | ";
        std::cout << std::setw(7) << std::setprecision(2) << ueDlLoss[i] << "% | ";
        std::cout << std::setw(7) << ueUlLoss[i] << "%";
        
        if (ueDlLoss[i] > 10.0 || ueUlLoss[i] > 10.0)
        {
            std::cout << " ⚠️";
        }
        std::cout << std::endl;
    }
    
    std::cout << "\n--- Aggregate Statistics ---\n";
    std::cout << "DL: " << dlActive << "/" << numUes << " UEs, ";
    std::cout << "Total: " << std::fixed << std::setprecision(2) << totalDlTput << " Mbps, ";
    std::cout << "Avg: " << (dlActive > 0 ? totalDlTput / dlActive : 0.0) << " Mbps/UE\n";
    
    std::cout << "UL: " << ulActive << "/" << numUes << " UEs, ";
    std::cout << "Total: " << totalUlTput << " Mbps, ";
    std::cout << "Avg: " << (ulActive > 0 ? totalUlTput / ulActive : 0.0) << " Mbps/UE\n";
    
    if (dlActive < numUes || ulActive < numUes)
    {
        std::cout << "\n⚠️  WARNING: Some UEs missing traffic\n";
    }
    else
    {
        std::cout << "\n✓ All UEs have active flows\n";
    }
}

// ===================================================================
// MAIN
// ===================================================================
int
main(int argc, char* argv[])
{
    // Start execution timer
    auto startTime = std::chrono::high_resolution_clock::now();
    auto lastStepTime = startTime;
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║      TRAFFIC VERIFICATION TEST (CORRECTED)                ║\n";
    std::cout << "║      Matching actual NrSimConfig API                      ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "⏱️  Test execution started at " 
              << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) 
              << "\n\n";

    // =================================================================
    // STEP 1: Load Configuration - FIXED FIELD NAMES
    // =================================================================
    std::cout << "Step 1/10: Loading configuration...\n";
    
    Ptr<NrSimConfig> config = CreateObject<NrSimConfig>();
    config->simDuration = 120.0;
    
    // Topology - FIXED: Use correct field names
    config->topology.gnbCount = 7;          // NOT numGnbs
    config->topology.ueCount = 10;          // NOT numUes
    config->topology.uePlacementStrategy = "uniform";  // NOT placement
    
    // Channel - FIXED: Remove non-existent scenarioName
    config->channel.frequency = 3.5e9;
    config->channel.bandwidth = 20e6;
    config->channel.propagationModel = "UMa";
    
    // Mobility
    config->mobility.defaultModel = "RandomWalk";
    config->mobility.defaultSpeed = 3.0;
    
    // FIXED: Add waypoints using map interface (not push_back)
    UeWaypointConfig wp1;
    wp1.speed = 5.0;
    wp1.waypoints = {{0, 0, 1.5}, {100, 100, 1.5}, {200, 0, 1.5}, {0, 0, 1.5}};
    config->mobility.ueWaypoints[0] = wp1;  // Use map indexing, not push_back
    
    UeWaypointConfig wp2;
    wp2.speed = 3.0;
    wp2.waypoints = {{50, 50, 1.5}, {150, 150, 1.5}};
    config->mobility.ueWaypoints[1] = wp2;  // Use map indexing
    
    UeWaypointConfig wp3;
    wp3.speed = 8.0;
    wp3.waypoints = {{-100, -100, 1.5}, {100, 100, 1.5}};
    config->mobility.ueWaypoints[2] = wp3;  // Use map indexing
    
    // Traffic - bidirectional
    config->traffic.udpRateDl = 100.0;  // Mbps downlink
    config->traffic.udpRateUl = 50.0;   // Mbps uplink
    config->traffic.packetSizeDl = 1024;
    config->traffic.packetSizeUl = 512;
    
    std::cout << "✓ Configuration loaded\n";
    PrintStepTime("Step 1", lastStepTime);

    // =================================================================
    // STEP 2: Setup NR Infrastructure
    // =================================================================
    std::cout << "Step 2/10: Setting up NR infrastructure...\n";
    
    Ptr<NrNetworkManager> netMgr = CreateObject<NrNetworkManager>();
    netMgr->SetConfig(config);
    netMgr->SetupNrInfrastructure();
    
    std::cout << "✓ NR infrastructure ready\n";
    PrintStepTime("Step 2", lastStepTime);

    // =================================================================
    // STEP 3: Deploy Topology
    // =================================================================
    std::cout << "Step 3/10: Deploying topology...\n";
    
    Ptr<NrTopologyManager> topoMgr = CreateObject<NrTopologyManager>();
    topoMgr->SetConfig(config);
    topoMgr->DeployTopology();
    
    NodeContainer gnbNodes = topoMgr->GetGnbNodes();
    NodeContainer ueNodes = topoMgr->GetUeNodes();
    
    std::cout << "✓ Topology: " << gnbNodes.GetN() << " gNBs, " 
              << ueNodes.GetN() << " UEs\n";
    PrintStepTime("Step 3", lastStepTime);

    PrintUeGnbAssociations(gnbNodes, ueNodes);

    // =================================================================
    // STEP 4: Install NR Devices
    // =================================================================
    std::cout << "Step 4/10: Installing NR devices...\n";
    
    netMgr->InstallNrDevices(gnbNodes, ueNodes);
    
    std::cout << "✓ NR devices installed\n";
    PrintStepTime("Step 4", lastStepTime);

    // =================================================================
    // STEP 5: Attach UEs to gNBs
    // =================================================================
    std::cout << "Step 5/10: Attaching UEs to gNBs...\n";
    
    netMgr->AttachUesToGnbs();
    
    std::cout << "✓ UEs attached\n";
    PrintStepTime("Step 5", lastStepTime);

    // =================================================================
    // STEP 6: Assign IP Addresses
    // =================================================================
    std::cout << "Step 6/10: Assigning IP addresses...\n";
    
    netMgr->AssignIpAddresses(ueNodes);
    
    std::cout << "✓ IP addresses assigned\n";
    PrintStepTime("Step 6", lastStepTime);

    // =================================================================
    // STEP 7: Install Mobility
    // =================================================================
    std::cout << "Step 7/10: Installing mobility...\n";
    
    Ptr<NrMobilityManager> mobMgr = CreateObject<NrMobilityManager>();
    mobMgr->SetConfig(config);
    mobMgr->InstallUeMobility(ueNodes);
    
    std::cout << "✓ Mobility configured\n";
    PrintStepTime("Step 7", lastStepTime);

    // =================================================================
    // STEP 8: Install Traffic (USING FIXED TRAFFIC MANAGER)
    // =================================================================
    std::cout << "Step 8/10: Installing traffic...\n";
    
    Ptr<NrTrafficManager> trafficMgr = CreateObject<NrTrafficManager>();
    trafficMgr->SetConfig(config);
    trafficMgr->SetNetworkManager(netMgr);
    trafficMgr->InstallTraffic(gnbNodes, ueNodes);
    
    // Get containers - both new and legacy
    ApplicationContainer dlSinks = trafficMgr->GetDlServerApps();
    ApplicationContainer ulSinks = trafficMgr->GetUlServerApps();
    ApplicationContainer dlSources = trafficMgr->GetDlClientApps();
    ApplicationContainer ulSources = trafficMgr->GetUlClientApps();
    
    std::cout << "✓ Traffic installed:\n";
    std::cout << "    DL: " << dlSinks.GetN() << " sinks + " 
              << dlSources.GetN() << " sources\n";
    std::cout << "    UL: " << ulSinks.GetN() << " sinks + " 
              << ulSources.GetN() << " sources\n";
    PrintStepTime("Step 8", lastStepTime);

    // =================================================================
    // STEP 9: Enable FlowMonitor
    // =================================================================
    std::cout << "Step 9/10: Enabling FlowMonitor...\n";
    
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(
        flowHelper.GetClassifier());
    
    std::cout << "✓ FlowMonitor enabled\n";
    PrintStepTime("Step 9", lastStepTime);

    // =================================================================
    // Position Tracking
    // =================================================================
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

    // =================================================================
    // Schedule Traffic Verification (USING FIXED FUNCTION)
    // =================================================================
    Simulator::Schedule(Seconds(config->simDuration - 0.5),
                       &VerifyPacketReception,
                       trafficMgr,
                       ueNodes.GetN());

    // =================================================================
    // STEP 10: Run Simulation
    // =================================================================
    std::cout << "Step 10/10: Running simulation for " << config->simDuration << " seconds...\n";
    
    auto simStartTime = std::chrono::high_resolution_clock::now();
    
    Simulator::Stop(Seconds(config->simDuration));
    Simulator::Run();
    
    posFile.close();
    
    auto simEndTime = std::chrono::high_resolution_clock::now();
    std::chrono::milliseconds simDuration = std::chrono::duration_cast<std::chrono::milliseconds>(simEndTime - simStartTime);
    
    std::cout << "\n✓ Simulation complete";
    std::cout << "  [Simulation runtime: " << std::fixed << std::setprecision(2) 
              << (simDuration.count() / 1000.0) << "s]\n";

    // =================================================================
    // Analyze Results
    // =================================================================
    AnalyzeFlowMonitor(monitor, classifier, ueNodes.GetN());

    // =================================================================
    // Final Summary
    // =================================================================
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║              TEST COMPLETE                                ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "Output files:\n";
    std::cout << "  - output/ue_positions.csv\n\n";

    Simulator::Destroy();
    
    // =================================================================
    // STOP TIMER AND CALCULATE EXECUTION TIME
    // =================================================================
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    double totalSeconds = duration.count() / 1000.0;
    int hours = static_cast<int>(totalSeconds / 3600);
    int minutes = static_cast<int>((totalSeconds - hours * 3600) / 60);
    double seconds = totalSeconds - hours * 3600 - minutes * 60;
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║              EXECUTION TIME SUMMARY                       ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "⏱️  Total wall-clock time: ";
    if (hours > 0)
    {
        std::cout << hours << "h " << minutes << "m " 
                  << std::fixed << std::setprecision(2) << seconds << "s";
    }
    else if (minutes > 0)
    {
        std::cout << minutes << "m " 
                  << std::fixed << std::setprecision(2) << seconds << "s";
    }
    else
    {
        std::cout << std::fixed << std::setprecision(2) << seconds << " seconds";
    }
    std::cout << "\n";
    
    std::cout << "   Raw: " << duration.count() << " milliseconds\n\n";
    
    // Performance analysis
    std::cout << "Performance analysis:\n";
    std::cout << "  Simulated time:     " << config->simDuration << " seconds\n";
    std::cout << "  Real execution time: " << std::fixed << std::setprecision(2) 
              << totalSeconds << " seconds\n";
    std::cout << "  Real-time ratio:    " 
              << std::fixed << std::setprecision(2)
              << (config->simDuration / totalSeconds) << "x\n";
    std::cout << "  (Ratio > 1.0 means simulation ran faster than real-time)\n\n";
    
    // Quick reference
    std::cout << "Time breakdown:\n";
    std::cout << "  Setup (Steps 1-9): See individual step times above\n";
    std::cout << "  Simulation run:    " 
              << std::fixed << std::setprecision(2) 
              << (simDuration.count() / 1000.0) << " seconds\n";
    std::cout << "  Analysis & cleanup: " 
              << std::fixed << std::setprecision(2)
              << (totalSeconds - (simDuration.count() / 1000.0)) << " seconds\n\n";
    
    return 0;
}