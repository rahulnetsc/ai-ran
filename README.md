# AI-RAN: Modular 5G NR Simulation Framework

A modular, extensible framework for simulating 5G New Radio (NR) networks in NS-3, designed for AI-driven Radio Access Network research and development.

[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![NS-3](https://img.shields.io/badge/NS--3-3.46-green.svg)](https://www.nsnam.org/)

## 🎯 Overview

AI-RAN provides a clean, manager-based architecture for complex 5G NR simulations with support for:

- 🏗️ **Modular Design**: Seven specialized managers for different aspects of simulation
- 📝 **JSON Configuration**: Easy scenario definition without code changes
- 🚶 **Advanced Mobility**: Waypoint-based paths, random walk, clustering, hotspots
- 📊 **Traffic & Metrics**: Bidirectional UDP flows with comprehensive metrics
- 🔄 **Handover Support**: Automatic tracking and statistics
- 📍 **Flexible Topology**: Grid, hexagonal, random, file-based positioning

## ✨ Key Features

### Manager-Based Architecture
```
NrSimulationManager (Central Orchestrator)
├── NrConfigManager      → JSON configuration loading/validation
├── NrTopologyManager    → Network deployment (gNB/UE positioning)
├── NrNetworkManager     → 5G NR infrastructure setup
├── NrMobilityManager    → Mobility model installation
├── NrTrafficManager     → Traffic generation and metrics
├── NrMetricsManager     → Performance monitoring
└── NrOutputManager      → Results export
```

### Simple 3-Step API
```cpp
Ptr<NrSimulationManager> sim = CreateObject<NrSimulationManager>();
sim->SetConfigFile("config.json");

sim->Initialize();  // Deploy network
sim->Run();         // Execute simulation  
sim->Finalize();    // Collect results
```

## 📋 Requirements

- **NS-3:** Version 3.46 or later ([ns-3-dev](https://gitlab.com/nsnam/ns-3-dev))
- **NR Module:** [ns3-new-radio/nr](https://gitlab.cttc.es/ns3-new-radio/nr)
- **Dependencies:** 
  - nlohmann-json (header-only, typically included)
  - C++17 compatible compiler

## 🚀 Quick Start

### Installation

```bash
# 1. Clone and setup NS-3
git clone https://gitlab.com/nsnam/ns-3-dev.git
cd ns-3-dev

# 2. Install NR module
cd contrib
git clone https://gitlab.cttc.es/ns3-new-radio/nr.git

# 3. Install AI-RAN
git clone https://github.com/rahulnetsc/ai-ran.git nr-modular
cd ..

# 4. Configure and build
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

### Run Example

```bash
# Run the example test
./ns3 run nr-downlink-test

# Or with custom config
./ns3 run "nr-downlink-test --config=contrib/nr-modular/config/my-scenario.json"
```

## 📝 Configuration Example

Create a JSON configuration file:

```json
{
  "topology": {
    "gnbCount": 3,
    "ueCount": 10,
    "areaSize": 500,
    "uePlacementStrategy": "random"
  },
  "channel": {
    "propagationModel": "UMa",
    "frequency": 3.5e9,
    "bandwidth": 20e6
  },
  "mobility": {
    "defaultModel": "RandomWalk",
    "defaultSpeed": 5.0,
    "ueWaypoints": {
      "0": {
        "speed": 10.0,
        "waypoints": [
          {"x": 100, "y": 100, "z": 1.5},
          {"x": 400, "y": 400, "z": 1.5}
        ]
      }
    }
  },
  "traffic": {
    "udpRateDl": 10e6,
    "packetSizeDl": 1024,
    "udpRateUl": 5e6,
    "packetSizeUl": 512
  },
  "simulation": {
    "duration": 30
  },
  "metrics": {
    "enableFlowMonitor": true,
    "outputFilePath": "output/results.txt"
  }
}
```

See `config/` directory for more examples.

## 💡 Usage

### Basic Simulation

```cpp
#include "ns3/nr-simulation-manager.h"

int main(int argc, char *argv[])
{
    // Enable logging (optional)
    LogComponentEnable("NrSimulationManager", LOG_LEVEL_INFO);
    
    // Create simulation manager
    Ptr<NrSimulationManager> sim = CreateObject<NrSimulationManager>();
    
    // Load configuration
    sim->SetConfigFile("path/to/config.json");
    
    // Run simulation
    sim->Initialize();
    sim->Run();
    sim->Finalize();
    
    return 0;
}
```

### Advanced: Programmatic Configuration

```cpp
// Create config object
Ptr<NrSimConfig> config = CreateObject<NrSimConfig>();

// Configure topology
config->topology.gnbCount = 7;
config->topology.ueCount = 20;
config->topology.areaSize = 1000;

// Configure mobility
config->mobility.defaultModel = "RandomWalk";
config->mobility.defaultSpeed = 5.0;

// Add waypoints for specific UE
UeWaypointConfig wpConfig;
wpConfig.speed = 10.0;
wpConfig.waypoints.push_back(Vector(100, 100, 1.5));
wpConfig.waypoints.push_back(Vector(900, 900, 1.5));
config->mobility.ueWaypoints[0] = wpConfig;

// Set configuration
sim->SetConfig(config);
```

## 📊 Output Metrics

The framework collects:

- **Throughput**: Per-UE downlink/uplink throughput
- **Packet Loss**: Loss rate per UE
- **Delay**: End-to-end packet delay (planned)
- **Handovers**: Handover count and timing
- **Attachment**: Real-time cell attachment status

Example output:
```
Total DL Throughput: 9.49 Mbps
Total UL Throughput: 5.79 Mbps
Average Packet Loss: 64.89%

Per-UE Metrics:
  UE 0: DL=2.79 Mbps, UL=0.73 Mbps, Loss=72.07%
  UE 1: DL=5.71 Mbps, UL=5.00 Mbps, Loss=42.94%
```

## 🗂️ Project Structure

```
nr-modular/
├── model/                          # Core implementation
│   ├── nr-simulation-manager.cc/h  # Central orchestrator
│   ├── nr-config-manager.cc/h      # Configuration handling
│   ├── nr-topology-manager.cc/h    # Network deployment
│   ├── nr-network-manager.cc/h     # NR infrastructure
│   ├── nr-mobility-manager.cc/h    # Mobility models
│   ├── nr-traffic-manager.cc/h     # Traffic and metrics
│   ├── nr-output-manager.cc/h      # Results output
│   └── utils/
│       └── nr-sim-config.cc/h      # Configuration structures
├── examples/                       # Example scenarios
│   └── nr-downlink-test.cc
├── config/                         # Sample configurations
│   └── test-waypoint-traffic-config.json
├── docs/                           # Documentation
│   └── nr-modular-project-summary.md
├── test/                           # Unit tests (planned)
├── output/                         # Results output directory
├── CMakeLists.txt                  # Build configuration
└── README.md
```

## 🎓 Documentation

- [Project Summary](docs/nr-modular-project-summary.md) - Comprehensive documentation
- [Configuration Guide](docs/configuration-guide.md) - JSON configuration reference (planned)
- [API Reference](docs/api-reference.md) - Class documentation (planned)

## 🔬 Use Cases

This framework is ideal for:

- **AI/ML in RAN**: Training RL agents for scheduling, handover, resource allocation
- **Mobility Studies**: Analyzing performance under different mobility patterns
- **QoS Research**: Multi-user scenarios with varying traffic demands
- **Handover Optimization**: Testing handover algorithms and parameters
- **Network Planning**: Coverage and capacity analysis
- **Algorithm Comparison**: Benchmarking different scheduling/beamforming strategies

## 🗺️ Roadmap

### Current Status (v1.0)
- ✅ Core manager architecture
- ✅ JSON configuration
- ✅ Waypoint mobility
- ✅ UDP traffic generation
- ✅ Basic metrics collection
- ✅ Handover tracking

### Planned Features
- [ ] TCP traffic support
- [ ] FlowMonitor integration
- [ ] Enhanced metrics (SINR, spectral efficiency)
- [ ] Video/FTP application models
- [ ] Machine learning integration hooks
- [ ] PCAP trace generation
- [ ] Real-time visualization
- [ ] Network slicing support

## 🤝 Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

Copyright (c) 2026 ARTPARK

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

See [LICENSE](LICENSE) file for details.

## 📚 Citation

If you use this framework in your research, please cite:

```bibtex
@software{ai_ran_2026,
  title={AI-RAN: Modular 5G NR Simulation Framework for NS-3},
  author={ARTPARK},
  year={2026},
  url={https://github.com/rahulnetsc/ai-ran}
}
```

## 👥 Contact & Support

- **GitHub Issues**: [Report bugs or request features](https://github.com/rahulnetsc/ai-ran/issues)
- **Organization**: ARTPARK
- **Maintainer**: Rahul

## 🙏 Acknowledgments

- Built on [NS-3](https://www.nsnam.org/) network simulator
- Uses the excellent [NR module](https://gitlab.cttc.es/ns3-new-radio/nr) for 5G NR PHY/MAC
- JSON parsing with [nlohmann/json](https://github.com/nlohmann/json)

## 📈 Performance Notes

- Typical simulation speed: 20-25x slower than real-time for detailed PHY
- Recommended: Start with small scenarios (3 gNBs, 10 UEs) for testing
- For large scenarios (100+ UEs), consider PHY abstraction options in NR module

---

**⭐ Star this repository if you find it useful!**

For detailed technical documentation, see [docs/nr-modular-project-summary.md](docs/nr-modular-project-summary.md)
