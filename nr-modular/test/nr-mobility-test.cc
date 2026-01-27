// #include "ns3/core-module.h"
// #include "ns3/nr-modular-module.h" // Includes NrSimulationManager and others
// #include <iostream>
// #include <chrono>

// using namespace ns3;

// NS_LOG_COMPONENT_DEFINE("NrModularManagerTest");

// int main(int argc, char* argv[])
// {
//     // Default configuration path
//     std::string configFile = "config/test-waypoint-traffic-config.json";
    
//     CommandLine cmd(__FILE__);
//     cmd.AddValue("config", "Path to configuration JSON file", configFile);
//     cmd.Parse(argc, argv);

//     std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
//     std::cout << "║      NR MODULAR SIMULATION MANAGER TEST                   ║\n";
//     std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";

//     auto startTime = std::chrono::high_resolution_clock::now();

//     // 1. Create the Manager
//     // This automatically creates all sub-managers (Traffic, Topology, etc.)
//     Ptr<NrSimulationManager> simManager = CreateObject<NrSimulationManager>();

//     // 2. Set Config and Initialize
//     // This executes Steps 1-10: Loading JSON, Setting up EPC, and Assigning IPs
//     simManager->SetConfigFile(configFile);
//     simManager->Initialize();

//     // 3. Run Simulation
//     // This installs traffic, enables metrics, and calls Simulator::Run()
//     simManager->Run();

//     // 4. Finalize
//     // This collects metrics, writes results, and calls Simulator::Destroy()
//     simManager->Finalize();

//     // 5. Execution Time Summary (Retained from your original script)
//     auto endTime = std::chrono::high_resolution_clock::now();
//     auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
//     std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
//     std::cout << "║              EXECUTION TIME SUMMARY                       ║\n";
//     std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
//     std::cout << "⏱️  Total wall-clock time: " << (duration.count() / 1000.0) << " seconds\n\n";

//     return 0;
// }