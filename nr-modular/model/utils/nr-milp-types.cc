/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * MILP Resource Scheduling - Type Definitions Implementation
 */

#include "nr-milp-types.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>

namespace ns3
{

// ============================================================================
// SCHEDULING MODE CONVERSIONS
// ============================================================================

std::string
SchedulingModeToString(SchedulingMode mode)
{
    switch (mode)
    {
        case SchedulingMode::HEURISTIC:
            return "heuristic";
        case SchedulingMode::MILP_EXECUTOR:
            return "milp";
        case SchedulingMode::MILP_RL:
            return "milp_rl";
        default:
            return "unknown";
    }
}

SchedulingMode
StringToSchedulingMode(const std::string& str)
{
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "heuristic")
    {
        return SchedulingMode::HEURISTIC;
    }
    else if (lower == "milp" || lower == "milp_executor")
    {
        return SchedulingMode::MILP_EXECUTOR;
    }
    else if (lower == "milp_rl")
    {
        return SchedulingMode::MILP_RL;
    }
    else
    {
        // Default to heuristic if unknown
        std::cerr << "Warning: Unknown scheduling mode '" << str 
                  << "', defaulting to HEURISTIC" << std::endl;
        return SchedulingMode::HEURISTIC;
    }
}

// ============================================================================
// SLICE TYPE CONVERSIONS
// ============================================================================

std::string
SliceTypeToString(SliceType type)
{
    switch (type)
    {
        case SliceType::eMBB:
            return "eMBB";
        case SliceType::uRLLC:
            return "uRLLC";
        case SliceType::mMTC:
            return "mMTC";
        default:
            return "unknown";
    }
}

SliceType
StringToSliceType(const std::string& str)
{
    if (str == "eMBB" || str == "embb" || str == "EMBB")
    {
        return SliceType::eMBB;
    }
    else if (str == "uRLLC" || str == "urllc" || str == "URLLC")
    {
        return SliceType::uRLLC;
    }
    else if (str == "mMTC" || str == "mmtc" || str == "MMTC")
    {
        return SliceType::mMTC;
    }
    else
    {
        // Default to eMBB if unknown
        std::cerr << "Warning: Unknown slice type '" << str 
                  << "', defaulting to eMBB" << std::endl;
        return SliceType::eMBB;
    }
}

// ============================================================================
// UeSla IMPLEMENTATION
// ============================================================================

UeSla::UeSla()
    : ueId(0),
      sliceType(SliceType::eMBB),
      throughputMbps(0.0),
      latencyMs(0.0),
      mcs(0),
      tbs(0)
{
}

UeSla::UeSla(uint32_t ueId,
             SliceType sliceType,
             double throughputMbps,
             double latencyMs,
             uint16_t mcs,
             uint32_t tbs)
    : ueId(ueId),
      sliceType(sliceType),
      throughputMbps(throughputMbps),
      latencyMs(latencyMs),
      mcs(mcs),
      tbs(tbs)
{
}

bool
UeSla::IsValid() const
{
    // Check throughput is positive
    if (throughputMbps <= 0.0)
    {
        std::cerr << "Invalid UeSla: throughputMbps must be > 0, got " 
                  << throughputMbps << std::endl;
        return false;
    }
    
    // Check latency is positive
    if (latencyMs <= 0.0)
    {
        std::cerr << "Invalid UeSla: latencyMs must be > 0, got " 
                  << latencyMs << std::endl;
        return false;
    }
    
    // Check MCS is in valid range [0, 28] (3GPP spec)
    if (mcs > 28)
    {
        std::cerr << "Invalid UeSla: mcs must be in [0, 28], got " 
                  << mcs << std::endl;
        return false;
    }
    
    // Check TBS is positive
    if (tbs == 0)
    {
        std::cerr << "Invalid UeSla: tbs must be > 0, got " 
                  << tbs << std::endl;
        return false;
    }
    
    // Additional validation: Check if SLA is realistic
    // For eMBB: typically throughput > 10 Mbps, latency < 100ms
    // For uRLLC: typically latency < 10ms, throughput 1-10 Mbps
    if (sliceType == SliceType::uRLLC && latencyMs > 20.0)
    {
        std::cerr << "Warning: uRLLC slice with latency > 20ms: " 
                  << latencyMs << "ms" << std::endl;
    }
    
    if (sliceType == SliceType::eMBB && throughputMbps < 1.0)
    {
        std::cerr << "Warning: eMBB slice with very low throughput: " 
                  << throughputMbps << " Mbps" << std::endl;
    }
    
    return true;
}

void
UeSla::Print(std::ostream& os) const
{
    os << "UeSla {" << std::endl;
    os << "  ueId: " << ueId << std::endl;
    os << "  sliceType: " << SliceTypeToString(sliceType) << std::endl;
    os << "  throughputMbps: " << throughputMbps << std::endl;
    os << "  latencyMs: " << latencyMs << std::endl;
    os << "  mcs: " << mcs << std::endl;
    os << "  tbs: " << tbs << " bits/PRB" << std::endl;
    os << "}";
}

// ============================================================================
// MilpProblem IMPLEMENTATION
// ============================================================================

MilpProblem::MilpProblem()
    : numUEs(0),
      bandwidth(0.0),
      totalBandwidthPrbs(0),
      timeWindow(0.0),
      numerology(1),
      slotDuration(0.0),
      totalSlots(0),
      ues()
{
}

bool
MilpProblem::IsValid() const
{
    // Check number of UEs
    if (numUEs == 0)
    {
        std::cerr << "Invalid MilpProblem: numUEs must be > 0" << std::endl;
        return false;
    }
    
    // Check bandwidth
    if (bandwidth <= 0.0)
    {
        std::cerr << "Invalid MilpProblem: bandwidth must be > 0, got " 
                  << bandwidth << std::endl;
        return false;
    }
    
    // Check total PRBs
    if (totalBandwidthPrbs == 0)
    {
        std::cerr << "Invalid MilpProblem: totalBandwidthPrbs must be > 0" 
                  << std::endl;
        return false;
    }
    
    // Check time window
    if (timeWindow <= 0.0)
    {
        std::cerr << "Invalid MilpProblem: timeWindow must be > 0, got " 
                  << timeWindow << std::endl;
        return false;
    }
    
    // Check numerology is fixed at 1
    if (numerology != 1)
    {
        std::cerr << "Invalid MilpProblem: numerology must be 1 (fixed), got " 
                  << static_cast<int>(numerology) << std::endl;
        return false;
    }
    
    // Check slot duration
    if (slotDuration <= 0.0)
    {
        std::cerr << "Invalid MilpProblem: slotDuration must be > 0, got " 
                  << slotDuration << std::endl;
        return false;
    }
    
    // Check total slots
    if (totalSlots == 0)
    {
        std::cerr << "Invalid MilpProblem: totalSlots must be > 0" << std::endl;
        return false;
    }
    
    // Verify totalSlots matches timeWindow / slotDuration
    uint32_t expectedSlots = static_cast<uint32_t>(timeWindow / slotDuration);
    if (totalSlots != expectedSlots)
    {
        std::cerr << "Warning: totalSlots (" << totalSlots 
                  << ") doesn't match timeWindow/slotDuration (" 
                  << expectedSlots << ")" << std::endl;
    }
    
    // Check UE vector size matches numUEs
    if (ues.size() != numUEs)
    {
        std::cerr << "Invalid MilpProblem: ues.size() (" << ues.size() 
                  << ") != numUEs (" << numUEs << ")" << std::endl;
        return false;
    }
    
    // Validate each UE SLA
    for (size_t i = 0; i < ues.size(); ++i)
    {
        if (!ues[i].IsValid())
        {
            std::cerr << "Invalid MilpProblem: UE " << i << " has invalid SLA" 
                      << std::endl;
            return false;
        }
        
        // Check UE ID matches index
        if (ues[i].ueId != i)
        {
            std::cerr << "Warning: UE at index " << i << " has ueId " 
                      << ues[i].ueId << std::endl;
        }
    }
    
    return true;
}

void
MilpProblem::Print(std::ostream& os) const
{
    os << "MilpProblem {" << std::endl;
    os << "  numUEs: " << numUEs << std::endl;
    os << "  bandwidth: " << (bandwidth / 1e6) << " MHz" << std::endl;
    os << "  totalBandwidthPrbs: " << totalBandwidthPrbs << std::endl;
    os << "  timeWindow: " << timeWindow << " s" << std::endl;
    os << "  numerology: " << static_cast<int>(numerology) << std::endl;
    os << "  slotDuration: " << (slotDuration * 1000) << " ms" << std::endl;
    os << "  totalSlots: " << totalSlots << std::endl;
    os << "  UE SLAs:" << std::endl;
    for (const auto& ue : ues)
    {
        os << "    ";
        ue.Print(os);
        os << std::endl;
    }
    os << "}";
}

// ============================================================================
// PrbAllocation IMPLEMENTATION
// ============================================================================

PrbAllocation::PrbAllocation()
    : ueId(0),
      slotId(0),
      startPrb(0),
      numPrbs(0)
{
}

PrbAllocation::PrbAllocation(uint32_t ueId,
                             uint32_t slotId,
                             uint32_t startPrb,
                             uint32_t numPrbs)
    : ueId(ueId),
      slotId(slotId),
      startPrb(startPrb),
      numPrbs(numPrbs)
{
}

bool
PrbAllocation::IsValid(uint32_t maxPrbs) const
{
    // Check number of PRBs is positive
    if (numPrbs == 0)
    {
        std::cerr << "Invalid PrbAllocation: numPrbs must be > 0" << std::endl;
        return false;
    }
    
    // Check allocation doesn't exceed available PRBs
    if (startPrb + numPrbs > maxPrbs)
    {
        std::cerr << "Invalid PrbAllocation: startPrb (" << startPrb 
                  << ") + numPrbs (" << numPrbs 
                  << ") > maxPrbs (" << maxPrbs << ")" << std::endl;
        return false;
    }
    
    return true;
}

void
PrbAllocation::Print(std::ostream& os) const
{
    os << "PrbAlloc{UE=" << ueId 
       << ", slot=" << slotId 
       << ", PRBs=[" << startPrb << "-" << (startPrb + numPrbs - 1) << "]"
       << ", count=" << numPrbs << "}";
}

// ============================================================================
// MilpSolution::UeSummary IMPLEMENTATION
// ============================================================================

MilpSolution::UeSummary::UeSummary()
    : totalPrbsAllocated(0),
      expectedThroughputMbps(0.0),
      maxLatencyMs(0.0),
      slasMet(false)
{
}

void
MilpSolution::UeSummary::Print(std::ostream& os) const
{
    os << "UeSummary {" << std::endl;
    os << "    totalPrbsAllocated: " << totalPrbsAllocated << std::endl;
    os << "    expectedThroughputMbps: " << expectedThroughputMbps << std::endl;
    os << "    maxLatencyMs: " << maxLatencyMs << std::endl;
    os << "    slasMet: " << (slasMet ? "YES" : "NO") << std::endl;
    os << "  }";
}

// ============================================================================
// MilpSolution IMPLEMENTATION
// ============================================================================

MilpSolution::MilpSolution()
    : status("unknown"),
      objectiveValue(0.0),
      solveTimeSeconds(0.0),
      allocations(),
      summary()
{
}

bool
MilpSolution::IsOptimal() const
{
    return status == "optimal";
}

bool
MilpSolution::IsFeasible() const
{
    return (status == "optimal") || (!allocations.empty());
}

uint32_t
MilpSolution::GetNumAllocations() const
{
    return static_cast<uint32_t>(allocations.size());
}

bool
MilpSolution::IsValid(const MilpProblem& problem) const
{
    // Check status
    if (status != "optimal" && status != "feasible" && 
        status != "infeasible" && status != "timeout" && status != "error")
    {
        std::cerr << "Warning: Unknown solution status: " << status << std::endl;
    }
    
    // If infeasible or error, allocations should be empty
    if (status == "infeasible" || status == "error")
    {
        if (!allocations.empty())
        {
            std::cerr << "Warning: Solution status is '" << status 
                      << "' but has " << allocations.size() 
                      << " allocations" << std::endl;
        }
        return true;  // No further validation needed
    }
    
    // Validate each allocation
    for (const auto& alloc : allocations)
    {
        // Check allocation is valid
        if (!alloc.IsValid(problem.totalBandwidthPrbs))
        {
            return false;
        }
        
        // Check UE ID exists in problem
        if (alloc.ueId >= problem.numUEs)
        {
            std::cerr << "Invalid MilpSolution: allocation has ueId " 
                      << alloc.ueId << " but problem only has " 
                      << problem.numUEs << " UEs" << std::endl;
            return false;
        }
        
        // Check slot ID is within bounds
        if (alloc.slotId >= problem.totalSlots)
        {
            std::cerr << "Invalid MilpSolution: allocation has slotId " 
                      << alloc.slotId << " but problem only has " 
                      << problem.totalSlots << " slots" << std::endl;
            return false;
        }
    }
    
    // Check for PRB overlaps within each slot
    // Group allocations by slot
    std::map<uint32_t, std::vector<PrbAllocation>> slotAllocations;
    for (const auto& alloc : allocations)
    {
        slotAllocations[alloc.slotId].push_back(alloc);
    }
    
    // Check each slot for overlaps
    for (const auto& [slotId, allocs] : slotAllocations)
    {
        for (size_t i = 0; i < allocs.size(); ++i)
        {
            for (size_t j = i + 1; j < allocs.size(); ++j)
            {
                const auto& a1 = allocs[i];
                const auto& a2 = allocs[j];
                
                // Check if PRB ranges overlap
                uint32_t a1_end = a1.startPrb + a1.numPrbs;
                uint32_t a2_end = a2.startPrb + a2.numPrbs;
                
                bool overlap = (a1.startPrb < a2_end) && (a2.startPrb < a1_end);
                
                if (overlap)
                {
                    std::cerr << "Invalid MilpSolution: PRB overlap at slot " 
                              << slotId << std::endl;
                    std::cerr << "  Allocation 1: ";
                    a1.Print(std::cerr);
                    std::cerr << std::endl;
                    std::cerr << "  Allocation 2: ";
                    a2.Print(std::cerr);
                    std::cerr << std::endl;
                    return false;
                }
            }
        }
    }
    
    // Validate summary entries
    for (const auto& [ueId, sum] : summary)
    {
        if (ueId >= problem.numUEs)
        {
            std::cerr << "Invalid MilpSolution: summary has ueId " 
                      << ueId << " but problem only has " 
                      << problem.numUEs << " UEs" << std::endl;
            return false;
        }
    }
    
    return true;
}

void
MilpSolution::Print(std::ostream& os) const
{
    os << "MilpSolution {" << std::endl;
    os << "  status: " << status << std::endl;
    os << "  objectiveValue: " << objectiveValue << " Mbps" << std::endl;
    os << "  solveTimeSeconds: " << solveTimeSeconds << " s" << std::endl;
    os << "  numAllocations: " << allocations.size() << std::endl;
    
    if (!summary.empty())
    {
        os << "  Per-UE Summary:" << std::endl;
        for (const auto& [ueId, sum] : summary)
        {
            os << "  UE " << ueId << ": ";
            sum.Print(os);
            os << std::endl;
        }
    }
    
    // Print first few allocations as examples
    if (!allocations.empty())
    {
        os << "  Sample allocations (first 5):" << std::endl;
        size_t numToPrint = std::min(allocations.size(), size_t(5));
        for (size_t i = 0; i < numToPrint; ++i)
        {
            os << "    ";
            allocations[i].Print(os);
            os << std::endl;
        }
        if (allocations.size() > 5)
        {
            os << "    ... (" << (allocations.size() - 5) << " more)" << std::endl;
        }
    }
    
    os << "}";
}

// ============================================================================
// SchedulingConfig IMPLEMENTATION
// ============================================================================

SchedulingConfig::SchedulingConfig()
    : mode(SchedulingMode::HEURISTIC),
      enableMilp(false),
      solverAddress("localhost"),
      solverPort(8888),
      timeWindow(1.0),
      slotDuration(0.0005),  // 0.5ms for numerology 1
      connectionTimeout(10.0),
      solveTimeout(60.0)
{
}

bool
SchedulingConfig::IsValid() const
{
    // If MILP is enabled, validate MILP-specific parameters
    if (enableMilp)
    {
        // Check solver address is not empty
        if (solverAddress.empty())
        {
            std::cerr << "Invalid SchedulingConfig: solverAddress is empty "
                      << "but MILP is enabled" << std::endl;
            return false;
        }
        
        // Check solver port is valid
        if (solverPort == 0)
        {
            std::cerr << "Invalid SchedulingConfig: solverPort must be > 0" 
                      << std::endl;
            return false;
        }
        
        // Check mode is MILP-based
        if (mode == SchedulingMode::HEURISTIC)
        {
            std::cerr << "Warning: enableMilp=true but mode=HEURISTIC" 
                      << std::endl;
        }
    }
    
    // Validate time window
    if (timeWindow <= 0.0)
    {
        std::cerr << "Invalid SchedulingConfig: timeWindow must be > 0, got " 
                  << timeWindow << std::endl;
        return false;
    }
    
    // Validate slot duration
    if (slotDuration <= 0.0)
    {
        std::cerr << "Invalid SchedulingConfig: slotDuration must be > 0, got " 
                  << slotDuration << std::endl;
        return false;
    }
    
    // Check slot duration is correct for numerology 1 (0.5ms)
    const double expected_slot_duration = 0.0005;  // 0.5ms for μ=1
    if (std::abs(slotDuration - expected_slot_duration) > 1e-6)
    {
        std::cerr << "Warning: slotDuration (" << slotDuration 
                  << ") doesn't match expected value for numerology 1 (" 
                  << expected_slot_duration << ")" << std::endl;
    }
    
    // Validate timeouts
    if (connectionTimeout <= 0.0)
    {
        std::cerr << "Invalid SchedulingConfig: connectionTimeout must be > 0" 
                  << std::endl;
        return false;
    }
    
    if (solveTimeout <= 0.0)
    {
        std::cerr << "Invalid SchedulingConfig: solveTimeout must be > 0" 
                  << std::endl;
        return false;
    }
    
    return true;
}

void
SchedulingConfig::Print(std::ostream& os) const
{
    os << "SchedulingConfig {" << std::endl;
    os << "  mode: " << SchedulingModeToString(mode) << std::endl;
    os << "  enableMilp: " << (enableMilp ? "YES" : "NO") << std::endl;
    
    if (enableMilp)
    {
        os << "  solverAddress: " << solverAddress << std::endl;
        os << "  solverPort: " << solverPort << std::endl;
        os << "  timeWindow: " << timeWindow << " s" << std::endl;
        os << "  slotDuration: " << (slotDuration * 1000) << " ms" << std::endl;
        os << "  connectionTimeout: " << connectionTimeout << " s" << std::endl;
        os << "  solveTimeout: " << solveTimeout << " s" << std::endl;
    }
    
    os << "}";
}

} // namespace ns3
