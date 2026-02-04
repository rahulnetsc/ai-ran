# NR Modular Simulation Framework

A highly modular and configurable 5G NR (New Radio) simulation framework built on NS-3, designed for flexible network topology, mobility, and traffic pattern experimentation.

[![NS-3 Version](https://img.shields.io/badge/NS--3-3.x-blue.svg)](https://www.nsnam.org/)
[![License](https://img.shields.io/badge/license-GPLv2-green.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-17-orange.svg)](https://isocpp.org/)

## 🌟 Features

### Core Capabilities
- **JSON-Driven Configuration**: Define entire simulation scenarios through simple JSON files
- **Modular Architecture**: Clean separation of concerns with dedicated managers for topology, mobility, traffic, and output
- **Flexible Topology**: Multiple UE placement strategies (random, grid, hotspot, cluster)
- **Advanced Mobility**: Support for static, random walk, and waypoint-based mobility with per-UE configuration
- **Comprehensive Traffic Control**: Configurable UDP downlink/uplink traffic with flow monitoring
- **Rich Metrics**: Detailed flow statistics including throughput, delay, jitter, and packet loss
- **Real-Time Visualization**: Live telemetry dashboard with topology mapping, throughput graphs, and connection visualization

### Topology Placement Strategies
- **Random**: Uniform random distribution across simulation area
- **Grid**: Structured grid-based placement with configurable spacing
- **Hotspot**: Concentrated clusters around random hotspot locations
- **Cluster**: Multiple clusters with configurable radius and UE density
- **File-based**: Load positions from external position files

### Mobility Models
- **Static**: Fixed positions (ConstantPositionMobilityModel)
- **Random Walk**: 2D random walk with configurable bounds
- **Waypoint**: Path-based movement with automatic timing calculation

## 📋 Table of Contents

- [Architecture](#-architecture)
- [Prerequisites](#-prerequisites)
- [Installation](#-installation)
- [Configuration](#-configuration)
- [Usage](#-usage)
- [Real-Time Visualization](#-real-time-visualization)
- [File Structure](#-file-structure)
- [Example Scenarios](#-example-scenarios)
- [Output and Metrics](#-output-and-metrics)
- [Contributing](#-contributing)
- [License](#-license)

## 🏗️ Architecture

The framework follows a modular design with clear separation of concerns:

```
┌─────────────────────────────────────────┐
│      NrSimulationManager (Main)         │
│         Orchestrates workflow           │
└──────────────┬──────────────────────────┘
               │
       ┌───────┴───────┐
       │               │
┌──────▼──────┐   ┌────▼────────┐
│   Config    │   │   Output    │
│   Manager   │   │   Manager   │
└──────┬──────┘   └─────────────┘
       │
┌──────┴────────────────────────────────┐
│                                       │
│  ┌──────────────┐  ┌───────────────┐  │
│  │  Topology    │  │   Mobility    │  │
│  │   Manager    │──│    Manager    │  │
│  └──────────────┘  └───────────────┘  │
│                                       │
│  ┌──────────────┐  ┌───────────────┐  │
│  │   Network    │  │   Traffic     │  │
│  │   Manager    │  │    Manager    │  │
│  └──────────────┘  └───────────────┘  │
│                                       │
└───────────────────────────────────────┘
```

### Module Responsibilities

| Module | Purpose | Key Functions |
|--------|---------|---------------|
| **SimulationManager** | Main orchestrator | Initialize, Run, Finalize workflow |
| **ConfigManager** | Configuration handling | Load/validate/save JSON configs |
| **TopologyManager** | Node placement | Create nodes, assign positions |
| **MobilityManager** | Movement patterns | Install mobility models, configure waypoints |
| **NetworkManager** | Network stack setup | Configure NR stack, channels, propagation |
| **TrafficManager** | Traffic generation | Install applications, flow monitoring |
| **OutputManager** | Results collection | Metrics aggregation, file output |

## 🔧 Prerequisites

### Required Dependencies
- **NS-3**: Version 3.x or higher
- **5G-LENA Module**: NR module for NS-3
- **nlohmann/json**: JSON parsing library (C++11)
- **C++ Compiler**: C++17 or higher (GCC 7+, Clang 5+)

### Optional Dependencies (for Visualization)
- **Python 3.6+**: For real-time telemetry dashboard
- **Tkinter**: Python GUI framework
- **Matplotlib**: Plotting library for topology visualization

### System Requirements
- Linux (Ubuntu 20.04/22.04 recommended)
- CMake 3.10+
- Python 3.6+ (for NS-3 build system and visualization)

## 📦 Installation

### 1. Install NS-3

```bash
# Clone NS-3
git clone https://gitlab.com/nsnam/ns-3-dev.git ns-3
cd ns-3

# Checkout desired version (e.g., ns-3.40)
git checkout ns-3.40
```

### 2. Install 5G-LENA NR Module

```bash
cd contrib
git clone https://gitlab.com/cttc-lena/nr.git
cd ..
```

### 3. Install nlohmann/json Library

```bash
# Ubuntu/Debian
sudo apt-get install nlohmann-json3-dev

# Or manually download single header
cd contrib/nr-modular/model/utils
wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
```

### 4. Install NR Modular Framework

```bash
cd contrib
git clone git@github.com:rahulnetsc/ai-ran.git nr-modular
cd ..
```

### 5. Configure and Build

```bash
./ns3 configure --build-profile=optimized --enable-examples --enable-tests
./ns3 build
```

## ⚙️ Configuration

The framework uses JSON configuration files to define simulation parameters. All configurations are validated at load time.

### Configuration File Structure

```json
{
  "topology": {
    "gnbCount": 3,
    "ueCount": 10,
    "useFilePositions": false,
    "areaSize": 1000.0,
    "uePlacementStrategy": "grid",
    "gnbPositions": [...],
    "uePositions": [...]
  },
  "channel": {
    "propagationModel": "UMa",
    "frequency": 3500000000.0,
    "bandwidth": 100000000.0
  },
  "mobility": {
    "defaultModel": "RandomWalk",
    "defaultSpeed": 3.0,
    "ueWaypoints": {
      "0": {
        "speed": 5.0,
        "waypoints": [
          {"x": 100, "y": 100, "z": 1.5},
          {"x": 500, "y": 500, "z": 1.5}
        ]
      }
    }
  },
  "traffic": {
    "udpRateDl": 10.0,
    "packetSizeDl": 1024,
    "udpRateUl": 2.0,
    "packetSizeUl": 512
  },
  "simulation": {
    "duration": 60.0
  },
  "metrics": {
    "enableFlowMonitor": true,
    "outputFilePath": "output/results.txt"
  }
}
```

### Configuration Parameters

#### Topology Section
- `gnbCount`: Number of gNB (base station) nodes
- `ueCount`: Number of UE (user equipment) nodes
- `useFilePositions`: Load positions from file (bool)
- `positionFile`: Path to position file (if `useFilePositions` is true)
- `areaSize`: Simulation area size in meters (for random/grid placement)
- `uePlacementStrategy`: Placement algorithm (`random`, `grid`, `hotspot`, `cluster`)
- `gnbPositions`: Array of explicit gNB positions (optional)
- `uePositions`: Array of explicit UE positions (optional)

**Grid Strategy Parameters:**
- `gridSpacing`: Distance between grid points (meters)

**Hotspot Strategy Parameters:**
- `numHotspots`: Number of hotspot centers
- `hotspotRadius`: Radius of each hotspot (meters)

**Cluster Strategy Parameters:**
- `numClusters`: Number of clusters
- `clusterRadius`: Radius of each cluster (meters)
- `uesPerCluster`: UEs per cluster

#### Channel Section
- `propagationModel`: Propagation loss model (`UMa`, `UMi`, `RMa`)
- `frequency`: Carrier frequency in Hz (e.g., 3.5 GHz = 3500000000.0)
- `bandwidth`: Channel bandwidth in Hz (e.g., 100 MHz = 100000000.0)

#### Mobility Section
- `defaultModel`: Default mobility model (`Static`, `RandomWalk`, `Waypoint`)
- `defaultSpeed`: Default movement speed in m/s
- `ueWaypoints`: Per-UE waypoint configuration (map of UE ID to waypoint config)

**Per-UE Waypoint Config:**
```json
"ueWaypoints": {
  "0": {
    "speed": 5.0,
    "waypoints": [
      {"x": 100, "y": 100, "z": 1.5},
      {"x": 500, "y": 500, "z": 1.5},
      {"x": 900, "y": 900, "z": 1.5}
    ]
  }
}
```

#### Traffic Section
- `udpRateDl`: Downlink UDP data rate in Mbps
- `packetSizeDl`: Downlink packet size in bytes
- `udpRateUl`: Uplink UDP data rate in Mbps
- `packetSizeUl`: Uplink packet size in bytes
- `enableFlowMonitoring`: Enable flow statistics collection
- `startTime`: Traffic start time in seconds

#### Simulation Section
- `duration`: Total simulation time in seconds

#### Metrics Section
- `enableFlowMonitor`: Enable FlowMonitor for detailed statistics
- `outputFilePath`: Path to output file for results

## 🚀 Usage

### Basic Usage

1. **Create a configuration file** (see `config/test-waypoint-traffic-config.json` for example)

2. **Run the simulation**:
```bash
./ns3 run "nr-downlink-test --config=config/test-waypoint-traffic-config.json"
```

3. **Optional: Launch real-time visualizer**:
```bash
# In a separate terminal
python3 monitor_telemetry_gui.py
```

4. **Check results**:
```bash
cat output/waypoint_traffic_results.txt
```

### Command Line Arguments

```bash
./ns3 run "nr-downlink-test --config=<path-to-config.json>"
```

- `--config`: Path to JSON configuration file (default: `config/test-waypoint-traffic-config.json`)

### Creating Custom Scenarios

#### Example 1: Simple Random Deployment

```json
{
  "topology": {
    "gnbCount": 1,
    "ueCount": 20,
    "uePlacementStrategy": "random",
    "areaSize": 500.0
  },
  "mobility": {
    "defaultModel": "RandomWalk",
    "defaultSpeed": 3.0
  },
  "traffic": {
    "udpRateDl": 5.0,
    "packetSizeDl": 512
  },
  "simulation": {
    "duration": 30.0
  }
}
```

#### Example 2: Grid Deployment with Waypoints

```json
{
  "topology": {
    "gnbCount": 3,
    "ueCount": 9,
    "uePlacementStrategy": "grid",
    "gridSpacing": 200.0,
    "areaSize": 1000.0
  },
  "mobility": {
    "defaultModel": "Static",
    "ueWaypoints": {
      "0": {
        "speed": 10.0,
        "waypoints": [
          {"x": 100, "y": 100, "z": 1.5},
          {"x": 900, "y": 900, "z": 1.5}
        ]
      }
    }
  }
}
```

#### Example 3: Hotspot Scenario

```json
{
  "topology": {
    "gnbCount": 4,
    "ueCount": 50,
    "uePlacementStrategy": "hotspot",
    "numHotspots": 3,
    "hotspotRadius": 100.0,
    "areaSize": 1000.0
  },
  "traffic": {
    "udpRateDl": 20.0,
    "packetSizeDl": 1024,
    "udpRateUl": 5.0,
    "packetSizeUl": 512
  }
}
```

## 📺 Real-Time Visualization

The framework includes an advanced telemetry dashboard for real-time monitoring of your NR simulations.

### Dashboard Features

![Dashboard Overview](docs/images/dashboard.png)

The telemetry GUI provides:

- **Live Topology Map**: Real-time visualization of gNB and UE positions
- **Connection Lines**: Color-coded links showing active connections (green = good, amber = weak, red = dead)
- **Per-UE Metrics Table**: Individual throughput, packet loss, distance, and RSRP for each UE
- **Global Statistics**: Network-wide DL/UL throughput, average loss, scheduler type
- **Raw JSON Inspector**: Debug view with live telemetry data and copy-to-clipboard
- **Progress Bar**: Simulation progress tracking

### Installation

Install Python dependencies:

```bash
# Ubuntu/Debian
sudo apt-get install python3-tk python3-matplotlib

# Or via pip
pip3 install matplotlib
```

### Running the Visualizer

1. **Start the visualizer** (in a separate terminal):
```bash
cd /path/to/ns-3
python3 monitor_telemetry_gui.py
```

2. **Run your simulation** (with telemetry enabled):
```bash
./ns3 run "nr-downlink-test --config=config/test-waypoint-traffic-config.json"
```

The dashboard will automatically connect and display live data on UDP port 5555.

### Dashboard Components

#### 1. Header Bar
- **Simulation Time**: Current simulation timestamp
- **Progress Bar**: Visual indicator of simulation completion

#### 2. Global Statistics Cards
- **Total DL Throughput**: Aggregate downlink throughput (Mbps)
- **Total UL Throughput**: Aggregate uplink throughput (Mbps)
- **Avg Network Loss**: Overall packet loss percentage
- **Scheduler**: Active MAC scheduler type
- **Network Config**: Number of gNBs and UEs

#### 3. Live Topology Map (Left Panel)
- **gNB Locations**: Red triangles with Cell ID labels
- **UE Positions**: Circles color-coded by health:
  - 🟢 Green: Good performance (≥1 Mbps)
  - 🟡 Amber: Weak performance (<1 Mbps)
  - 🔴 Red: Dead/no throughput
- **Connection Lines**: Dotted lines showing UE-gNB associations
- **Interactive Legend**: Performance threshold indicators

#### 4. Per-UE Performance Table (Right Panel)
Displays individual UE metrics:
- **ID**: UE identifier
- **IMSI**: International Mobile Subscriber Identity
- **Cell**: Serving cell ID
- **DL Mbps**: Downlink throughput
- **UL Mbps**: Uplink throughput
- **Loss %**: Packet loss ratio
- **Dist**: Distance to serving gNB (meters)
- **RSRP**: Reference Signal Received Power (dBm)

#### 5. Raw JSON Inspector (Bottom Right)
- Live JSON telemetry stream
- Pause/resume capability
- Copy to clipboard functionality
- Syntax-highlighted display

### Controls

- **Pause Live View**: Freeze the dashboard to inspect current state
- **Copy JSON**: Copy raw telemetry data to clipboard for analysis

### Telemetry Data Format

The simulator sends JSON telemetry via UDP containing:

```json
{
  "timestamp": {
    "simulation_time": 45.5,
    "wall_clock_time": "2026-02-04T10:30:00"
  },
  "simulation": {
    "progress_percent": 75.8,
    "duration_total": 60.0
  },
  "config": {
    "gnb_count": 3,
    "ue_count": 10,
    "area_size": 1000.0
  },
  "topology": {
    "gnbs": [
      {
        "id": 0,
        "cell_id": 0,
        "position": {"x": 500.0, "y": 150.0, "z": 25.0},
        "scheduler": {"type": "ns3::NrMacSchedulerTdmaRR"}
      }
    ],
    "ues": [
      {
        "id": 0,
        "imsi": 1,
        "position": {"x": 150.0, "y": 150.0, "z": 1.5},
        "network": {
          "cell_id": 0,
          "gnb_id": 0,
          "distance_to_gnb": 350.0
        },
        "radio": {
          "rsrp_dbm": -75.3,
          "available": true
        },
        "traffic": {
          "dl": {
            "throughput_mbps": 8.5,
            "loss_percent": 2.1
          },
          "ul": {
            "throughput_mbps": 1.2,
            "loss_percent": 0.5
          }
        }
      }
    ]
  },
  "traffic_summary": {
    "total_dl_throughput_mbps": 45.2,
    "total_ul_throughput_mbps": 8.7,
    "avg_packet_loss_percent": 3.2
  }
}
```

### Tips for Visualization

1. **Large Scenarios**: For 100+ UEs, increase refresh interval in the code (line: `self.after(500, ...)`)
2. **Network Issues**: Check firewall rules allow UDP on port 5555
3. **Performance**: Close unused applications to ensure smooth rendering
4. **Multiple Runs**: Restart the visualizer between simulation runs for clean state

## 📁 File Structure

```
ns-3/
├── contrib/
│   └── nr-modular/
│       ├── model/
│       │   ├── nr-simulation-manager.cc      # Main orchestrator
│       │   ├── nr-config-manager.cc          # Configuration loader
│       │   ├── nr-topology-manager.cc        # Node placement
│       │   ├── nr-mobility-manager.cc        # Mobility models
│       │   ├── nr-network-manager.cc         # Network setup
│       │   ├── nr-traffic-manager.cc         # Traffic generation
│       │   ├── nr-output-manager.cc          # Metrics output
│       │   └── utils/
│       │       └── nr-sim-config.cc          # Config data structures
│       ├── helper/                           # Helper classes (optional)
│       └── examples/                         # Example scenarios
├── scratch/
│   └── nr-downlink-test.cc                   # Main test program
├── monitor_telemetry_gui.py                  # Real-time visualization dashboard
└── config/
    ├── test-waypoint-traffic-config.json     # Example config
    └── new.json                              # Simple config
```

### Key Files

| File | Location | Purpose |
|------|----------|---------|
| `nr-downlink-test.cc` | `scratch/` | Main simulation entry point |
| `monitor_telemetry_gui.py` | Root directory | Real-time telemetry dashboard |
| `nr-simulation-manager.cc` | `contrib/nr-modular/model/` | Simulation orchestrator |
| `nr-config-manager.cc` | `contrib/nr-modular/model/` | JSON configuration handler |
| `nr-topology-manager.cc` | `contrib/nr-modular/model/` | Topology and placement |
| `nr-mobility-manager.cc` | `contrib/nr-modular/model/` | Mobility configuration |
| `nr-network-manager.cc` | `contrib/nr-modular/model/` | NR stack setup |
| `nr-traffic-manager.cc` | `contrib/nr-modular/model/` | Traffic and flow monitoring |
| `nr-output-manager.cc` | `contrib/nr-modular/model/` | Results and metrics |
| `nr-sim-config.cc` | `contrib/nr-modular/model/utils/` | Configuration data structures |

## 📊 Example Scenarios

### Scenario 1: Urban Macro (UMa) Deployment

**Use Case**: Urban area with high-rise buildings and moderate user density

```json
{
  "topology": {
    "gnbCount": 7,
    "ueCount": 100,
    "uePlacementStrategy": "hotspot",
    "numHotspots": 5,
    "hotspotRadius": 150.0,
    "areaSize": 2000.0
  },
  "channel": {
    "propagationModel": "UMa",
    "frequency": 3500000000.0,
    "bandwidth": 100000000.0
  },
  "mobility": {
    "defaultModel": "RandomWalk",
    "defaultSpeed": 1.5
  },
  "traffic": {
    "udpRateDl": 50.0,
    "packetSizeDl": 1400,
    "udpRateUl": 10.0,
    "packetSizeUl": 512
  },
  "simulation": {
    "duration": 120.0
  }
}
```

### Scenario 2: Highway Mobility

**Use Case**: Highway scenario with high-speed mobility

```json
{
  "topology": {
    "gnbCount": 10,
    "ueCount": 30,
    "uePlacementStrategy": "grid",
    "gridSpacing": 500.0,
    "areaSize": 5000.0
  },
  "channel": {
    "propagationModel": "RMa",
    "frequency": 700000000.0,
    "bandwidth": 20000000.0
  },
  "mobility": {
    "defaultModel": "RandomWalk",
    "defaultSpeed": 30.0
  },
  "traffic": {
    "udpRateDl": 10.0,
    "packetSizeDl": 1024
  },
  "simulation": {
    "duration": 60.0
  }
}
```

### Scenario 3: Dense Indoor (UMi)

**Use Case**: Dense indoor environment with many users

```json
{
  "topology": {
    "gnbCount": 4,
    "ueCount": 50,
    "uePlacementStrategy": "cluster",
    "numClusters": 4,
    "clusterRadius": 50.0,
    "uesPerCluster": 12,
    "areaSize": 500.0
  },
  "channel": {
    "propagationModel": "UMi",
    "frequency": 28000000000.0,
    "bandwidth": 400000000.0
  },
  "mobility": {
    "defaultModel": "Static"
  },
  "traffic": {
    "udpRateDl": 100.0,
    "packetSizeDl": 1500,
    "udpRateUl": 20.0,
    "packetSizeUl": 512
  },
  "simulation": {
    "duration": 90.0
  }
}
```

## 📈 Output and Metrics

### Flow Monitor Statistics

The framework collects comprehensive per-flow statistics:

- **Throughput**: Rx bytes, Tx bytes, data rate (Mbps)
- **Delay**: Mean delay, delay standard deviation
- **Jitter**: Mean jitter
- **Packet Loss**: Lost packets, packet loss ratio
- **Timing**: First Tx time, Last Rx time

### Sample Output

```
========================================
FLOW MONITOR STATISTICS
========================================

Flow 1 (192.168.1.1:49153 -> 7.0.0.2:9)
  Tx Packets:   5000
  Rx Packets:   4850
  Lost Packets: 150
  Packet Loss Ratio: 3.00%
  Mean Delay:   12.5 ms
  Mean Jitter:  2.3 ms
  Throughput:   45.2 Mbps
  Duration:     60.0 s
----------------------------------------

Flow 2 (192.168.1.1:49154 -> 7.0.0.3:9)
  Tx Packets:   5000
  Rx Packets:   4920
  Lost Packets: 80
  Packet Loss Ratio: 1.60%
  Mean Delay:   8.7 ms
  Mean Jitter:  1.8 ms
  Throughput:   48.5 Mbps
  Duration:     60.0 s
----------------------------------------

SUMMARY:
  Total Flows:        10
  Total Tx Packets:   50000
  Total Rx Packets:   48500
  Overall Loss Ratio: 3.00%
  Average Throughput: 450.0 Mbps
========================================
```

### Output Files

Results are saved to the path specified in `metrics.outputFilePath`:

```
output/
└── waypoint_traffic_results.txt    # Flow statistics
```

## 🔍 Debugging and Logging

Enable detailed logging for specific components:

```cpp
// In nr-downlink-test.cc
LogComponentEnable("NrSimulationManager", LOG_LEVEL_INFO);
LogComponentEnable("NrTopologyManager", LOG_LEVEL_DEBUG);
LogComponentEnable("NrTrafficManager", LOG_LEVEL_ALL);
```

Available log levels:
- `LOG_LEVEL_ERROR`: Error messages only
- `LOG_LEVEL_WARN`: Warnings and errors
- `LOG_LEVEL_INFO`: General information
- `LOG_LEVEL_DEBUG`: Detailed debug info
- `LOG_LEVEL_ALL`: All messages including function calls

## 🤝 Contributing

Contributions are welcome! Please follow these guidelines:

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/new-mobility-model`
3. **Make your changes** with clear commit messages
4. **Test thoroughly** with multiple scenarios
5. **Submit a pull request** with description of changes

### Code Style
- Follow NS-3 coding style guidelines
- Use meaningful variable and function names
- Add comments for complex logic
- Include log messages at appropriate levels

### Testing
- Test with various configuration combinations
- Verify backward compatibility
- Check for memory leaks with valgrind
- Run NS-3 test suite: `./test.py`

## 📝 License

This project is licensed under the GNU General Public License v2.0 - see the [LICENSE](LICENSE) file for details.

Copyright (c) 2026 ARTPARK

## 🙏 Acknowledgments

- **NS-3 Team**: For the excellent network simulator
- **5G-LENA Team**: For the comprehensive NR module
- **nlohmann**: For the JSON parsing library

## 📧 Contact

For questions, issues, or suggestions:
- Open an issue on GitHub
- Contact: [your-email@example.com]

## 🗺️ Roadmap

### Planned Features
- [ ] Additional mobility models (Gauss-Markov, Manhattan Grid)
- [ ] Support for beamforming configuration
- [ ] Multi-cell handover scenarios
- [x] Real-time visualization tools ✅
- [ ] Performance optimization for large-scale scenarios
- [ ] Integration with machine learning frameworks
- [ ] Support for D2D (Device-to-Device) communication
- [ ] Advanced scheduler configuration options
- [ ] Export telemetry to time-series databases (InfluxDB, Prometheus)
- [ ] Web-based dashboard alternative to Python GUI

### Version History
- **v1.0.0** (2026-01): Initial release with core modular framework
  - JSON-based configuration
  - Multiple topology strategies
  - Waypoint mobility support
  - Flow monitoring and statistics
  - Real-time telemetry dashboard

---

**Built with ❤️ for 5G NR research and development**
