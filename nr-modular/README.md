# Folder structure

```
ns-3-dev/
├── contrib/
│   └── nr-modular/                    # Your new module
│       ├── CMakeLists.txt             # Build configuration
│       ├── README.md
│       ├── doc/                       # Documentation
│       │   ├── architecture.md
│       │   └── user-guide.md
│       ├── model/                     # Core implementation
│       │   ├── nr-simulation-manager.h/cc
│       │   ├── nr-topology-manager.h/cc
│       │   ├── nr-channel-manager.h/cc
│       │   ├── nr-mobility-manager.h/cc
│       │   ├── nr-traffic-manager.h/cc
│       │   ├── nr-metrics-manager.h/cc
│       │   ├── nr-config-manager.h/cc
│       │   ├── nr-output-manager.h/cc
│       │   ├── strategies/            # Strategy implementations
│       │   │   ├── mobility-strategies.h/cc
│       │   │   ├── traffic-strategies.h/cc
│       │   │   └── deployment-strategies.h/cc
│       │   └── utils/                 # Utility classes
│       │       ├── nr-sim-config.h/cc
│       │       └── nr-result-types.h/cc
│       ├── helper/                    # NS-3 helper pattern
│       │   └── nr-simulation-helper.h/cc
│       ├── test/                      # Unit tests
│       │   ├── nr-topology-test.cc
│       │   ├── nr-mobility-test.cc
│       │   └── nr-traffic-test.cc
│       └── examples/                  # Example simulations
│           ├── nr-simple-scenario.cc
│           ├── nr-xr-simulation.cc
│           └── nr-mobility-study.cc
└── scratch/                           # For quick experiments
    └── my-nr-tests/
```