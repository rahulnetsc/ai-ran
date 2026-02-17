# ============================================================================
# CMakeLists.txt for nr-modular module
# ============================================================================
#
# This module provides a modular architecture for NR simulations with:
# - Configuration management
# - Topology deployment
# - Channel modeling
# - Mobility management
# - Traffic generation
# - Metrics collection
# - Results output
#
# ============================================================================

# ============================================================================
# BUILD LIBRARY DEFINITION
# ============================================================================

build_lib(
    # Module name (will create libnr-modular.so)
    LIBNAME nr-modular
    
    # ========================================================================
    # SOURCE FILES (Implementation files - .cc)
    # ========================================================================
    SOURCE_FILES
        # Core managers
        model/nr-simulation-manager.cc
        model/nr-config-manager.cc
        model/nr-topology-manager.cc
        model/nr-channel-manager.cc
        model/nr-network-manager.cc
        model/nr-mobility-manager.cc
        model/nr-traffic-manager.cc
        model/nr-metrics-manager.cc
        model/nr-output-manager.cc
        model/utils/nr-milp-types.cc
        model/nr-milp-interface.cc
        model/nr-bwp-manager.cc
        model/nr-milp-executor-scheduler.cc
        # Utilities
        model/utils/nr-sim-config.cc
        
    # ========================================================================
    # HEADER FILES (Public API - .h)
    # ========================================================================
    HEADER_FILES
        # Core managers
        model/nr-simulation-manager.h
        model/nr-config-manager.h
        model/nr-topology-manager.h
        model/nr-channel-manager.h
        model/nr-network-manager.h
        model/nr-mobility-manager.h
        model/nr-traffic-manager.h
        model/nr-metrics-manager.h
        model/nr-output-manager.h
        model/utils/nr-milp-types.h
        model/nr-milp-interface.h
        model/nr-bwp-manager.h
        model/nr-milp-executor-scheduler.h
        # Utilities
        model/utils/nr-sim-config.h
        
    # ========================================================================
    # LIBRARIES TO LINK (Dependencies)
    # ========================================================================
    LIBRARIES_TO_LINK
        # NS-3 Core modules
        ${libcore}              # Core NS-3 functionality (Object, Ptr, etc.)
        ${libnetwork}           # Network devices and protocols
        ${libinternet}          # IP stacks and routing
        ${libapplications}      # Traffic applications (UDP, TCP)
        
        # Mobility and positioning
        ${libmobility}          # Mobility models (RandomWalk, etc.)
        
        # Wireless communication
        ${libspectrum}          # Spectrum and frequency modeling
        ${libpropagation}       # Path loss and propagation models
        
        # Metrics and monitoring
        ${libflow-monitor}      # Flow statistics collection
        
        # 5G NR module (from 5G-LENA)
        ${libnr}                # The 5G-LENA NR module
)

# ============================================================================
# BUILD TEST APPLICATION
# ============================================================================
# build_lib_example(
#     NAME nr-modular-traffic-test
#     SOURCE test/nr-modular-traffic-test.cc
#     LIBRARIES_TO_LINK ${lib_nr-modular}
# )

# ============================================================================
# NOTES ON FUTURE ADDITIONS
# ============================================================================
#
# When you add more files later, add them to the appropriate section:
#
# 1. For strategy implementations (when you create them):
#    SOURCE_FILES:
#      model/strategies/deployment-strategies.cc
#      model/strategies/mobility-strategies.cc
#      model/strategies/traffic-strategies.cc
#
#    HEADER_FILES:
#      model/strategies/deployment-strategies.h
#      model/strategies/mobility-strategies.h
#      model/strategies/traffic-strategies.h
#
# 2. For helper classes (when you create them):
#    SOURCE_FILES:
#      helper/nr-simulation-helper.cc
#
#    HEADER_FILES:
#      helper/nr-simulation-helper.h
#
# 3. For additional utilities:
#    SOURCE_FILES:
#      model/utils/nr-result-types.cc
#
#    HEADER_FILES:
#      model/utils/nr-result-types.h
#
# ============================================================================