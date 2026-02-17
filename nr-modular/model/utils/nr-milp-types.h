/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * MILP Resource Scheduling - Type Definitions
 * 
 * This file defines all data structures used for MILP-based optimal
 * BWP (Bandwidth Part) allocation and resource scheduling in 5G NR.
 * 
 * Key Components:
 * - UE SLA specifications (throughput, latency requirements)
 * - MILP problem formulation (inputs to solver)
 * - MILP solution representation (outputs from solver)
 * - PRB allocation structures (per-slot assignments)
 * - Scheduling modes and configuration flags
 */

#ifndef NR_MILP_TYPES_H
#define NR_MILP_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace ns3
{

// ============================================================================
// SCHEDULING MODE ENUMERATION
// ============================================================================

/**
 * \brief Scheduling mode for resource allocation
 * 
 * Controls which scheduler implementation is used:
 * - HEURISTIC: Traditional scheduler (Proportional Fair, Round Robin, etc.)
 *              This is Baseline 1 - no MILP optimization
 * 
 * - MILP_EXECUTOR: MILP-based optimal allocation with blind execution
 *                  This is Baseline 2 - theoretical optimum, state-agnostic
 * 
 * - MILP_RL: MILP guidance with RL-based adaptive execution
 *            This is the Proposed approach - practical optimum, state-aware
 *            (Phase 3 - Future implementation)
 * 
 * Usage:
 * Set in JSON config: "scheduling": {"mode": "milp"}
 * Or programmatically: config.scheduling.mode = SchedulingMode::MILP_EXECUTOR
 */
enum class SchedulingMode : uint8_t
{
    HEURISTIC = 0,      ///< Baseline 1: Traditional heuristic scheduler
    MILP_EXECUTOR = 1,  ///< Baseline 2: MILP optimal (blind executor)
    MILP_RL = 2         ///< Proposed: MILP + RL adaptive (future)
};

/**
 * \brief Convert scheduling mode enum to string
 * \param mode The scheduling mode
 * \return String representation ("heuristic", "milp", "milp_rl")
 */
std::string SchedulingModeToString(SchedulingMode mode);

/**
 * \brief Convert string to scheduling mode enum
 * \param str String representation ("heuristic", "milp", "milp_rl")
 * \return Scheduling mode enum
 */
SchedulingMode StringToSchedulingMode(const std::string& str);

// ============================================================================
// SLICE TYPE ENUMERATION
// ============================================================================

/**
 * \brief 5G Network Slice Type
 * 
 * Defines the type of network slice for each UE:
 * - eMBB: Enhanced Mobile Broadband (high throughput, moderate latency)
 * - uRLLC: Ultra-Reliable Low-Latency Communications (low latency, high reliability)
 * - mMTC: Massive Machine-Type Communications (many devices, low power)
 * 
 * Different slice types have different SLA requirements:
 * - eMBB: High throughput (50+ Mbps), latency ~10-20ms
 * - uRLLC: Low latency (<5ms), moderate throughput (1-10 Mbps)
 * - mMTC: Low power, low throughput, high device density
 */
enum class SliceType : uint8_t
{
    eMBB = 0,   ///< Enhanced Mobile Broadband
    uRLLC = 1,  ///< Ultra-Reliable Low-Latency Communications
    mMTC = 2    ///< Massive Machine-Type Communications
};

/**
 * \brief Convert slice type enum to string
 * \param type The slice type
 * \return String representation ("eMBB", "uRLLC", "mMTC")
 */
std::string SliceTypeToString(SliceType type);

/**
 * \brief Convert string to slice type enum
 * \param str String representation ("eMBB", "uRLLC", "mMTC")
 * \return Slice type enum
 */
SliceType StringToSliceType(const std::string& str);

// ============================================================================
// SLA SPECIFICATION STRUCTURE
// ============================================================================

/**
 * \brief Service Level Agreement (SLA) for a UE
 * 
 * Defines the QoS requirements for a specific UE. Each UE can belong to
 * one network slice and has specific throughput and latency requirements.
 * 
 * Fields:
 * - ueId: Unique identifier for the UE (0-indexed)
 * - sliceType: Type of network slice (eMBB, uRLLC, mMTC)
 * - throughputMbps: Minimum required throughput in Mbps
 * - latencyMs: Maximum allowed latency in milliseconds
 * - mcs: Modulation and Coding Scheme (0-28, higher = better channel)
 * - tbs: Transport Block Size in bits per PRB (depends on MCS)
 * 
 * Example (eMBB):
 *   ueId = 0
 *   sliceType = eMBB
 *   throughputMbps = 50.0
 *   latencyMs = 10.0
 *   mcs = 16
 *   tbs = 25832  (from 3GPP tables for MCS 16)
 * 
 * Example (uRLLC):
 *   ueId = 1
 *   sliceType = uRLLC
 *   throughputMbps = 10.0
 *   latencyMs = 5.0
 *   mcs = 16
 *   tbs = 25832
 * 
 * The MILP solver uses these SLAs as constraints to compute optimal
 * PRB allocations that satisfy all requirements.
 */
struct UeSla
{
    uint32_t ueId;              ///< UE identifier (0-indexed)
    SliceType sliceType;        ///< Network slice type
    double throughputMbps;      ///< Minimum throughput requirement (Mbps)
    double latencyMs;           ///< Maximum latency requirement (ms)
    uint16_t mcs;               ///< Modulation and Coding Scheme (0-28)
    uint32_t tbs;               ///< Transport Block Size (bits per PRB)
    
    /**
     * \brief Default constructor
     */
    UeSla();
    
    /**
     * \brief Parameterized constructor
     * \param ueId UE identifier
     * \param sliceType Network slice type
     * \param throughputMbps Minimum throughput (Mbps)
     * \param latencyMs Maximum latency (ms)
     * \param mcs Modulation and Coding Scheme
     * \param tbs Transport Block Size (bits per PRB)
     */
    UeSla(uint32_t ueId,
          SliceType sliceType,
          double throughputMbps,
          double latencyMs,
          uint16_t mcs,
          uint32_t tbs);
    
    /**
     * \brief Validate SLA parameters
     * \return true if SLA is valid, false otherwise
     * 
     * Checks:
     * - throughputMbps > 0
     * - latencyMs > 0
     * - mcs in range [0, 28]
     * - tbs > 0
     */
    bool IsValid() const;
    
    /**
     * \brief Print SLA to output stream (for debugging)
     * \param os Output stream
     */
    void Print(std::ostream& os) const;
};

// ============================================================================
// MILP PROBLEM STRUCTURE
// ============================================================================

/**
 * \brief MILP Problem Specification
 * 
 * This structure contains all information needed by the external MILP solver
 * to compute optimal PRB allocations. It is serialized to JSON and sent via
 * socket to the Python solver.
 * 
 * Fields:
 * - numUEs: Number of UEs to schedule
 * - bandwidth: Total available bandwidth in Hz (e.g., 100 MHz = 100e6)
 * - totalBandwidthPrbs: Total number of PRBs in bandwidth
 * - timeWindow: Optimization time window in seconds (e.g., 1.0s)
 * - numerology: Fixed numerology (μ = 1 for 30 kHz SCS)
 * - slotDuration: Duration of one slot in seconds (0.5ms for μ=1)
 * - totalSlots: Total number of slots in time window (timeWindow / slotDuration)
 * - ues: Vector of UE SLA specifications
 * 
 * Example (3 UEs, 1 second simulation):
 *   numUEs = 3
 *   bandwidth = 100e6  (100 MHz)
 *   totalBandwidthPrbs = 273  (for 100 MHz at 30 kHz SCS)
 *   timeWindow = 1.0  (1 second)
 *   numerology = 1  (30 kHz SCS, 0.5ms slots)
 *   slotDuration = 0.0005  (0.5 ms)
 *   totalSlots = 2000  (1.0s / 0.0005s = 2000 slots)
 *   ues = [UeSla0, UeSla1, UeSla2]
 * 
 * The MILP solver formulates optimization problem:
 *   Maximize: Total throughput
 *   Subject to:
 *     - Each UE meets throughput SLA
 *     - Each UE meets latency SLA
 *     - PRBs don't overlap in time-frequency
 *     - Total PRBs per slot <= totalBandwidthPrbs
 */
struct MilpProblem
{
    uint32_t numUEs;                ///< Number of UEs
    double bandwidth;               ///< Total bandwidth (Hz)
    uint32_t totalBandwidthPrbs;    ///< Total PRBs available
    double timeWindow;              ///< Optimization window (seconds)
    uint8_t numerology;             ///< Fixed numerology (μ = 1)
    double slotDuration;            ///< Slot duration (seconds)
    uint32_t totalSlots;            ///< Total slots in time window
    std::vector<UeSla> ues;         ///< UE SLA specifications
    
    /**
     * \brief Default constructor
     */
    MilpProblem();
    
    /**
     * \brief Validate problem parameters
     * \return true if problem is valid, false otherwise
     * 
     * Checks:
     * - numUEs > 0
     * - bandwidth > 0
     * - totalBandwidthPrbs > 0
     * - timeWindow > 0
     * - numerology == 1 (fixed)
     * - slotDuration > 0
     * - totalSlots > 0
     * - All UE SLAs are valid
     */
    bool IsValid() const;
    
    /**
     * \brief Print problem to output stream (for debugging)
     * \param os Output stream
     */
    void Print(std::ostream& os) const;
};

// ============================================================================
// PRB ALLOCATION STRUCTURE
// ============================================================================

/**
 * \brief Physical Resource Block (PRB) Allocation
 * 
 * Represents a single PRB allocation decision from the MILP solver.
 * Specifies which UE gets which PRBs at which time slot.
 * 
 * Fields:
 * - ueId: Which UE receives this allocation
 * - slotId: Which time slot (TTI index, 0-indexed)
 * - startPrb: Starting PRB index in frequency domain (0-indexed)
 * - numPrbs: Number of contiguous PRBs allocated
 * 
 * Example:
 *   ueId = 0
 *   slotId = 5  (6th slot, at time t = 5 * 0.5ms = 2.5ms)
 *   startPrb = 0  (starts at PRB 0)
 *   numPrbs = 10  (allocates PRBs 0-9)
 * 
 * This means: UE 0 gets PRBs [0-9] at slot 5 (t=2.5ms)
 * 
 * Constraints:
 * - PRBs must be contiguous (no gaps)
 * - startPrb + numPrbs <= totalBandwidthPrbs
 * - No two allocations can overlap in time-frequency
 */
struct PrbAllocation
{
    uint32_t ueId;      ///< UE identifier
    uint32_t slotId;    ///< Time slot index (TTI)
    uint32_t startPrb;  ///< Starting PRB index (frequency)
    uint32_t numPrbs;   ///< Number of contiguous PRBs
    
    /**
     * \brief Default constructor
     */
    PrbAllocation();
    
    /**
     * \brief Parameterized constructor
     * \param ueId UE identifier
     * \param slotId Time slot index
     * \param startPrb Starting PRB index
     * \param numPrbs Number of PRBs
     */
    PrbAllocation(uint32_t ueId,
                  uint32_t slotId,
                  uint32_t startPrb,
                  uint32_t numPrbs);
    
    /**
     * \brief Validate allocation parameters
     * \param maxPrbs Maximum number of PRBs available
     * \return true if allocation is valid, false otherwise
     * 
     * Checks:
     * - numPrbs > 0
     * - startPrb + numPrbs <= maxPrbs
     */
    bool IsValid(uint32_t maxPrbs) const;
    
    /**
     * \brief Print allocation to output stream (for debugging)
     * \param os Output stream
     */
    void Print(std::ostream& os) const;
};

// ============================================================================
// MILP SOLUTION STRUCTURE
// ============================================================================

/**
 * \brief MILP Solution from External Solver
 * 
 * Contains the complete solution returned by the MILP solver, including
 * the allocation blueprint and summary statistics.
 * 
 * Fields:
 * - status: Solution status ("optimal", "infeasible", "timeout", "error")
 * - objectiveValue: Objective function value (total throughput in Mbps)
 * - solveTimeSeconds: Time taken to solve MILP (seconds)
 * - allocations: Vector of all PRB allocations (per-slot, per-UE)
 * - summary: Per-UE summary statistics (throughput, latency, SLA met)
 * 
 * Example:
 *   status = "optimal"
 *   objectiveValue = 150.5  (total throughput achieved)
 *   solveTimeSeconds = 12.34
 *   allocations = [6000 allocations for 3 UEs over 2000 slots]
 *   summary = {
 *     0: {totalPrbs=1000, throughput=51.2, latency=9.5, slasMet=true},
 *     1: {totalPrbs=200, throughput=10.8, latency=4.8, slasMet=true},
 *     2: {totalPrbs=600, throughput=30.5, latency=18.2, slasMet=true}
 *   }
 * 
 * Status Meanings:
 * - "optimal": MILP found optimal solution
 * - "infeasible": Problem has no feasible solution (SLAs too strict)
 * - "timeout": Solver exceeded time limit
 * - "error": Solver encountered error
 * 
 * The allocations vector contains ALL per-slot allocations for the entire
 * simulation. For a 1-second simulation with 3 UEs:
 *   - 2000 slots × 3 UEs = up to 6000 allocation entries
 *   - Each entry specifies exact PRB assignment
 */
struct MilpSolution
{
    /**
     * \brief Per-UE Summary Statistics
     * 
     * Aggregated statistics for one UE showing expected performance
     * if the MILP allocation is executed perfectly.
     * 
     * Fields:
     * - totalPrbsAllocated: Total PRBs allocated to this UE
     * - expectedThroughputMbps: Expected throughput (Mbps)
     * - maxLatencyMs: Maximum latency for any transmission (ms)
     * - slasMet: Whether this UE's SLAs are satisfied
     */
    struct UeSummary
    {
        uint32_t totalPrbsAllocated;    ///< Total PRBs allocated
        double expectedThroughputMbps;  ///< Expected throughput (Mbps)
        double maxLatencyMs;            ///< Maximum latency (ms)
        bool slasMet;                   ///< SLA satisfaction flag
        
        /**
         * \brief Default constructor
         */
        UeSummary();
        
        /**
         * \brief Print summary to output stream (for debugging)
         * \param os Output stream
         */
        void Print(std::ostream& os) const;
    };
    
    std::string status;                         ///< Solution status
    double objectiveValue;                      ///< Objective value (Mbps)
    double solveTimeSeconds;                    ///< Solve time (seconds)
    std::vector<PrbAllocation> allocations;     ///< All PRB allocations
    std::map<uint32_t, UeSummary> summary;      ///< Per-UE summaries
    
    /**
     * \brief Default constructor
     */
    MilpSolution();
    
    /**
     * \brief Check if solution is optimal
     * \return true if status == "optimal"
     */
    bool IsOptimal() const;
    
    /**
     * \brief Check if solution is feasible
     * \return true if status == "optimal" or has allocations
     */
    bool IsFeasible() const;
    
    /**
     * \brief Get number of allocations
     * \return Total number of PRB allocations
     */
    uint32_t GetNumAllocations() const;
    
    /**
     * \brief Validate solution
     * \param problem The MILP problem this is a solution to
     * \return true if solution is valid for given problem
     * 
     * Checks:
     * - All allocations are valid
     * - No PRB overlaps in time-frequency
     * - All UE IDs exist in problem
     * - Slot IDs are within bounds
     */
    bool IsValid(const MilpProblem& problem) const;
    
    /**
     * \brief Print solution to output stream (for debugging)
     * \param os Output stream
     */
    void Print(std::ostream& os) const;
};

// ============================================================================
// SCHEDULING CONFIGURATION STRUCTURE
// ============================================================================

/**
 * \brief Scheduling Configuration
 * 
 * Configuration parameters for the scheduling system, including
 * MILP solver connection settings and scheduling mode.
 * 
 * Fields:
 * - mode: Scheduling mode (heuristic, milp, milp_rl)
 * - enableMilp: Flag to enable/disable MILP solver (for baseline comparison)
 * - solverAddress: IP address of MILP solver (default: "localhost")
 * - solverPort: Port number for socket communication (default: 8888)
 * - timeWindow: MILP optimization window in seconds (default: 1.0)
 * - slotDuration: Slot duration in seconds (default: 0.0005 for μ=1)
 * - connectionTimeout: Socket connection timeout in seconds (default: 10.0)
 * - solveTimeout: MILP solve timeout in seconds (default: 60.0)
 * 
 * Example (MILP enabled):
 *   mode = SchedulingMode::MILP_EXECUTOR
 *   enableMilp = true
 *   solverAddress = "localhost"
 *   solverPort = 8888
 *   timeWindow = 1.0
 *   slotDuration = 0.0005
 * 
 * Example (MILP disabled - Baseline 1):
 *   mode = SchedulingMode::HEURISTIC
 *   enableMilp = false
 *   // ... other fields ignored when enableMilp = false
 * 
 * The enableMilp flag is critical for baseline comparisons:
 * - enableMilp = false → Use traditional scheduler (Baseline 1)
 * - enableMilp = true, mode = MILP_EXECUTOR → Use MILP blind (Baseline 2)
 * - enableMilp = true, mode = MILP_RL → Use MILP + RL (Proposed)
 */
struct SchedulingConfig
{
    SchedulingMode mode;        ///< Scheduling mode
    bool enableMilp;            ///< Enable/disable MILP solver
    std::string solverAddress;  ///< MILP solver IP address
    uint16_t solverPort;        ///< MILP solver port
    double timeWindow;          ///< Optimization window (seconds)
    double slotDuration;        ///< Slot duration (seconds)
    double connectionTimeout;   ///< Socket connection timeout (seconds)
    double solveTimeout;        ///< MILP solve timeout (seconds)
    
    /**
     * \brief Default constructor
     * 
     * Sets default values:
     * - mode = HEURISTIC
     * - enableMilp = false
     * - solverAddress = "localhost"
     * - solverPort = 8888
     * - timeWindow = 1.0
     * - slotDuration = 0.0005
     * - connectionTimeout = 10.0
     * - solveTimeout = 60.0
     */
    SchedulingConfig();
    
    /**
     * \brief Validate configuration
     * \return true if configuration is valid
     * 
     * Checks:
     * - If enableMilp = true, then solverAddress is not empty
     * - solverPort > 0
     * - timeWindow > 0
     * - slotDuration > 0
     * - Timeouts > 0
     */
    bool IsValid() const;
    
    /**
     * \brief Print configuration to output stream (for debugging)
     * \param os Output stream
     */
    void Print(std::ostream& os) const;
};

} // namespace ns3

#endif /* NR_MILP_TYPES_H */
