/*
 * Test script for NR Simulation Manager
 * 
 * This script demonstrates how to use the NrSimulationManager
 * to run a 5G NR simulation using a JSON configuration file.
 * 
 * Usage:
 *   ./ns3 run "scratch/test-nr-simulation --configFile=config/test-waypoint-traffic-config.json"
 * 
 * Or with default path:
 *   ./ns3 run "scratch/test-nr-simulation"
 */

#include "ns3/core-module.h"
#include "ns3/nr-simulation-manager.h"
#include "ns3/nr-sim-config.h"

#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TestNrSimulation");

int
main(int argc, char* argv[])
{
    // =========================================================================
    // STEP 1: Parse command line arguments
    // =========================================================================
    std::string configFile = "config/test-waypoint-traffic-config.json";
    bool verbose = false;
    
    CommandLine cmd(__FILE__);
    cmd.AddValue("configFile", "Path to JSON configuration file", configFile);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.Parse(argc, argv);
    
    // =========================================================================
    // STEP 2: Configure logging
    // =========================================================================
    if (verbose)
    {
        LogComponentEnable("TestNrSimulation", LOG_LEVEL_ALL);
        LogComponentEnable("NrSimulationManager", LOG_LEVEL_ALL);
        LogComponentEnable("NrNetworkManager", LOG_LEVEL_ALL);
        LogComponentEnable("NrTopologyManager", LOG_LEVEL_ALL);
        LogComponentEnable("NrMobilityManager", LOG_LEVEL_ALL);
        LogComponentEnable("NrTrafficManager", LOG_LEVEL_ALL);
        LogComponentEnable("NrMetricsManager", LOG_LEVEL_ALL);
    }
    
    // =========================================================================
    // Print Banner
    // =========================================================================
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           5G NR SIMULATION TEST                            ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Configuration file: " << configFile << "\n";
    std::cout << "\n";
    
    // =========================================================================
    // STEP 3: Create the Simulation Manager
    // =========================================================================
    NS_LOG_INFO("Creating NrSimulationManager...");
    
    Ptr<NrSimulationManager> simManager = CreateObject<NrSimulationManager>();
    
    std::cout << "✓ Simulation manager created\n" << std::endl;
    
    // =========================================================================
    // STEP 4: Set Configuration File Path
    // =========================================================================
    std::cout << "Setting configuration...\n" << std::endl;
    
    simManager->SetConfigFile(configFile);
    std::cout << "✓ Configuration file path set\n" << std::endl;
    
    // =========================================================================
    // STEP 5: Initialize the Simulation
    // =========================================================================
    std::cout << "\n";
    std::cout << "┌────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ PHASE 1: INITIALIZATION                                    │\n";
    std::cout << "└────────────────────────────────────────────────────────────┘\n";
    std::cout << "\n";
    
    simManager->Initialize();
    std::cout << "\n✓ Initialization completed successfully!\n" << std::endl;
    
    // =========================================================================
    // STEP 6: Run the Simulation
    // =========================================================================
    std::cout << "\n";
    std::cout << "┌────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ PHASE 2: RUNNING SIMULATION                                │\n";
    std::cout << "└────────────────────────────────────────────────────────────┘\n";
    std::cout << "\n";
    
    simManager->Run();
    std::cout << "\n✓ Simulation completed successfully!\n" << std::endl;
    
    // =========================================================================
    // STEP 7: Finalize and Generate Results
    // =========================================================================
    std::cout << "\n";
    std::cout << "┌────────────────────────────────────────────────────────────┐\n";
    std::cout << "│ PHASE 3: FINALIZATION                                      │\n";
    std::cout << "└────────────────────────────────────────────────────────────┘\n";
    std::cout << "\n";
    
    simManager->Finalize();
    std::cout << "\n✓ Finalization completed successfully!\n" << std::endl;
    
    // =========================================================================
    // DONE
    // =========================================================================
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           SIMULATION COMPLETED SUCCESSFULLY                ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    // Get config to show basic summary
    Ptr<NrSimConfig> config = simManager->GetConfig();
    
    std::cout << "Simulation Summary:\n";
    std::cout << "  ├─ Scenario:        " << config->channel.propagationModel << "\n";
    std::cout << "  ├─ Frequency:       " << (config->channel.frequency / 1e9) << " GHz\n";
    std::cout << "  ├─ Bandwidth:       " << (config->channel.bandwidth / 1e6) << " MHz\n";
    std::cout << "  └─ Duration:        " << config->simDuration << " seconds\n";
    std::cout << "\n";
    
    return 0;
}