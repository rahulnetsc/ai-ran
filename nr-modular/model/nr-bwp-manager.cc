/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * BWP Manager - Implementation File
 */

#include "nr-bwp-manager.h"

#include "ns3/log.h"
#include "ns3/abort.h"

#include <fstream>
#include <algorithm>
#include <numeric>
#include <iomanip>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrBwpManager");
NS_OBJECT_ENSURE_REGISTERED(NrBwpManager);

// ============================================================================
// TYPE ID
// ============================================================================

TypeId
NrBwpManager::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrBwpManager")
                            .SetParent<Object>()
                            .SetGroupName("NrModular")
                            .AddConstructor<NrBwpManager>();
    return tid;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

NrBwpManager::NrBwpManager()
    : m_solution(),
      m_slotAllocations(),
      m_ueTotalPrbs(),
      m_statistics(),
      m_hasLoadedSolution(false),
      m_totalSlots(0),
      m_totalBandwidthPrbs(0),
      m_numUes(0)
{
    NS_LOG_FUNCTION(this);
}

NrBwpManager::~NrBwpManager()
{
    NS_LOG_FUNCTION(this);
}

void
NrBwpManager::DoDispose()
{
    NS_LOG_FUNCTION(this);
    ClearSolution();
    Object::DoDispose();
}

// ============================================================================
// MILP SOLUTION LOADING
// ============================================================================

bool
NrBwpManager::LoadMilpSolution(const MilpSolution& solution)
{
    NS_LOG_FUNCTION(this);
    
    NS_LOG_INFO("Loading MILP solution...");
    
    // Clear any existing solution
    ClearSolution();
    
    // Store the solution
    m_solution = solution;
    
    // Check if solution is valid
    if (!m_solution.IsOptimal() && !m_solution.IsFeasible())
    {
        NS_LOG_ERROR("MILP solution is not optimal or feasible. Status: " 
                     << m_solution.status);
        return false;
    }
    
    // Check if solution has allocations
    if (m_solution.allocations.empty())
    {
        NS_LOG_WARN("MILP solution has no allocations");
        m_hasLoadedSolution = false;
        return false;
    }
    
    NS_LOG_INFO("Solution status: " << m_solution.status);
    NS_LOG_INFO("Number of allocations: " << m_solution.allocations.size());
    NS_LOG_INFO("Solve time: " << m_solution.solveTimeSeconds << " seconds");
    
    // Extract metadata from first allocation
    if (!m_solution.allocations.empty())
    {
        // Find max UE ID and max slot ID
        uint32_t maxUeId = 0;
        uint32_t maxSlotId = 0;
        
        for (const auto& alloc : m_solution.allocations)
        {
            maxUeId = std::max(maxUeId, alloc.ueId);
            maxSlotId = std::max(maxSlotId, alloc.slotId);
        }
        
        m_numUes = maxUeId + 1;
        m_totalSlots = maxSlotId + 1;
        
        NS_LOG_INFO("Detected " << m_numUes << " UEs");
        NS_LOG_INFO("Detected " << m_totalSlots << " slots");
    }
    
    // Build indexed structure
    NS_LOG_INFO("Building indexed lookup table...");
    BuildIndexedStructure();
    
    // Validate solution integrity
    NS_LOG_INFO("Validating solution integrity...");
    if (!ValidateSolution())
    {
        NS_LOG_ERROR("Solution validation failed");
        ClearSolution();
        return false;
    }
    
    // Compute statistics
    NS_LOG_INFO("Computing statistics...");
    ComputeStatistics();
    
    m_hasLoadedSolution = true;
    
    NS_LOG_INFO("MILP solution loaded successfully");
    NS_LOG_INFO("Active slots: " << m_statistics.numActiveSlots 
                << "/" << m_statistics.totalSlots 
                << " (" << (m_statistics.slotUtilization * 100) << "%)");
    
    return true;
}

bool
NrBwpManager::HasSolution() const
{
    return m_hasLoadedSolution;
}

void
NrBwpManager::ClearSolution()
{
    NS_LOG_FUNCTION(this);
    
    m_solution = MilpSolution();
    m_slotAllocations.clear();
    m_ueTotalPrbs.clear();
    m_statistics = Statistics();
    m_hasLoadedSolution = false;
    m_totalSlots = 0;
    m_totalBandwidthPrbs = 0;
    m_numUes = 0;
}

// ============================================================================
// SLOT-BASED QUERIES
// ============================================================================

std::vector<PrbAllocation>
NrBwpManager::GetAllocationForSlot(uint32_t slotId) const
{
    NS_LOG_FUNCTION(this << slotId);
    
    if (!m_hasLoadedSolution)
    {
        NS_LOG_WARN("No MILP solution loaded");
        return {};
    }
    
    // O(1) hash map lookup
    auto it = m_slotAllocations.find(slotId);
    if (it != m_slotAllocations.end())
    {
        NS_LOG_DEBUG("Found " << it->second.size() 
                     << " allocations for slot " << slotId);
        return it->second;
    }
    
    NS_LOG_DEBUG("No allocations for slot " << slotId << " (idle slot)");
    return {};
}

bool
NrBwpManager::HasAllocationsForSlot(uint32_t slotId) const
{
    NS_LOG_FUNCTION(this << slotId);
    
    if (!m_hasLoadedSolution)
    {
        return false;
    }
    
    return m_slotAllocations.find(slotId) != m_slotAllocations.end();
}

uint32_t
NrBwpManager::GetNumUesInSlot(uint32_t slotId) const
{
    NS_LOG_FUNCTION(this << slotId);
    
    if (!m_hasLoadedSolution)
    {
        return 0;
    }
    
    auto it = m_slotAllocations.find(slotId);
    if (it != m_slotAllocations.end())
    {
        return static_cast<uint32_t>(it->second.size());
    }
    
    return 0;
}

// ============================================================================
// UE-SPECIFIC QUERIES
// ============================================================================

std::optional<PrbAllocation>
NrBwpManager::GetUeAllocationForSlot(uint32_t slotId, uint32_t ueId) const
{
    NS_LOG_FUNCTION(this << slotId << ueId);
    
    if (!m_hasLoadedSolution)
    {
        NS_LOG_WARN("No MILP solution loaded");
        return std::nullopt;
    }
    
    // Get all allocations for this slot
    auto it = m_slotAllocations.find(slotId);
    if (it == m_slotAllocations.end())
    {
        // Slot has no allocations
        return std::nullopt;
    }
    
    // Search for specific UE (typically only 1-3 UEs per slot)
    for (const auto& alloc : it->second)
    {
        if (alloc.ueId == ueId)
        {
            return alloc;
        }
    }
    
    // UE not found in this slot
    return std::nullopt;
}

std::vector<PrbAllocation>
NrBwpManager::GetUeAllocations(uint32_t ueId) const
{
    NS_LOG_FUNCTION(this << ueId);
    
    if (!m_hasLoadedSolution)
    {
        NS_LOG_WARN("No MILP solution loaded");
        return {};
    }
    
    std::vector<PrbAllocation> ueAllocations;
    
    // Iterate through all allocations (O(N))
    for (const auto& alloc : m_solution.allocations)
    {
        if (alloc.ueId == ueId)
        {
            ueAllocations.push_back(alloc);
        }
    }
    
    NS_LOG_DEBUG("UE " << ueId << " has " << ueAllocations.size() 
                 << " allocations");
    
    return ueAllocations;
}

uint32_t
NrBwpManager::GetTotalPrbsForUe(uint32_t ueId) const
{
    NS_LOG_FUNCTION(this << ueId);
    
    if (!m_hasLoadedSolution)
    {
        return 0;
    }
    
    // O(1) lookup from pre-computed map
    auto it = m_ueTotalPrbs.find(ueId);
    if (it != m_ueTotalPrbs.end())
    {
        return it->second;
    }
    
    NS_LOG_WARN("UE " << ueId << " not found in total PRBs map");
    return 0;
}

// ============================================================================
// STATISTICS AND VALIDATION
// ============================================================================

NrBwpManager::Statistics
NrBwpManager::GetStatistics() const
{
    NS_LOG_FUNCTION(this);
    return m_statistics;
}

const MilpSolution&
NrBwpManager::GetMilpSolution() const
{
    return m_solution;
}

bool
NrBwpManager::ValidateSolution() const
{
    NS_LOG_FUNCTION(this);
    
    if (m_solution.allocations.empty())
    {
        NS_LOG_ERROR("Solution has no allocations");
        return false;
    }
    
    // Validate no PRB overlaps
    if (!ValidateNoPrbOverlaps())
    {
        NS_LOG_ERROR("PRB overlap detected");
        return false;
    }
    
    // Validate all allocations are within bounds
    for (const auto& alloc : m_solution.allocations)
    {
        // Check UE ID
        if (alloc.ueId >= m_numUes)
        {
            NS_LOG_ERROR("Invalid UE ID: " << alloc.ueId 
                         << " (max: " << (m_numUes - 1) << ")");
            return false;
        }
        
        // Check slot ID
        if (alloc.slotId >= m_totalSlots)
        {
            NS_LOG_ERROR("Invalid slot ID: " << alloc.slotId 
                         << " (max: " << (m_totalSlots - 1) << ")");
            return false;
        }
        
        // Check PRB allocation (if we know bandwidth)
        if (m_totalBandwidthPrbs > 0)
        {
            if (!alloc.IsValid(m_totalBandwidthPrbs))
            {
                NS_LOG_ERROR("Invalid PRB allocation for UE " << alloc.ueId 
                             << " at slot " << alloc.slotId);
                return false;
            }
        }
    }
    
    NS_LOG_INFO("Solution validation passed");
    return true;
}

// ============================================================================
// DEBUGGING AND VISUALIZATION
// ============================================================================

void
NrBwpManager::PrintSummary(std::ostream& os) const
{
    os << "\n========================================" << std::endl;
    os << "BWP MANAGER - MILP SOLUTION SUMMARY" << std::endl;
    os << "========================================" << std::endl;
    
    if (!m_hasLoadedSolution)
    {
        os << "No solution loaded" << std::endl;
        return;
    }
    
    os << "Solution Status: " << m_solution.status << std::endl;
    os << "Objective Value: " << m_solution.objectiveValue << " Mbps" << std::endl;
    os << "Solve Time: " << m_solution.solveTimeSeconds << " seconds" << std::endl;
    os << std::endl;
    
    os << "Resource Allocation:" << std::endl;
    os << "  Total slots: " << m_statistics.totalSlots << std::endl;
    os << "  Active slots: " << m_statistics.numActiveSlots 
       << " (" << (m_statistics.slotUtilization * 100) << "%)" << std::endl;
    os << "  Idle slots: " << m_statistics.numIdleSlots << std::endl;
    os << "  Total allocations: " << m_statistics.totalAllocations << std::endl;
    os << "  Total PRBs allocated: " << m_statistics.totalPrbsAllocated << std::endl;
    os << std::endl;
    
    os << "PRB Usage:" << std::endl;
    os << "  Max PRBs per slot: " << m_statistics.maxPrbsPerSlot << std::endl;
    os << "  Min PRBs per slot: " << m_statistics.minPrbsPerSlot << std::endl;
    os << "  Avg PRBs per active slot: " << m_statistics.avgPrbsPerActiveSlot << std::endl;
    os << std::endl;
    
    os << "UE Distribution:" << std::endl;
    os << "  Max UEs per slot: " << m_statistics.maxUesPerSlot << std::endl;
    os << "  Avg UEs per active slot: " << m_statistics.avgUesPerActiveSlot << std::endl;
    os << std::endl;
    
    os << "Per-UE PRB Allocation:" << std::endl;
    for (const auto& [ueId, totalPrbs] : m_statistics.prbsPerUe)
    {
        os << "  UE " << ueId << ": " << totalPrbs << " PRBs";
        
        // Print expected throughput if available
        auto it = m_solution.summary.find(ueId);
        if (it != m_solution.summary.end())
        {
            os << " (Expected: " << it->second.expectedThroughputMbps << " Mbps, "
               << "Latency: " << it->second.maxLatencyMs << " ms, "
               << "SLA: " << (it->second.slasMet ? "MET" : "VIOLATED") << ")";
        }
        os << std::endl;
    }
    
    os << "========================================\n" << std::endl;
}

void
NrBwpManager::PrintSlotAllocations(uint32_t slotId, std::ostream& os) const
{
    os << "Slot " << slotId << " allocations:" << std::endl;
    
    if (!m_hasLoadedSolution)
    {
        os << "  No solution loaded" << std::endl;
        return;
    }
    
    auto it = m_slotAllocations.find(slotId);
    if (it == m_slotAllocations.end())
    {
        os << "  No allocations (idle slot)" << std::endl;
        return;
    }
    
    uint32_t totalPrbsInSlot = 0;
    
    for (const auto& alloc : it->second)
    {
        os << "  UE " << alloc.ueId << ": PRBs [" 
           << alloc.startPrb << "-" << (alloc.startPrb + alloc.numPrbs - 1) 
           << "] (" << alloc.numPrbs << " PRBs)" << std::endl;
        
        totalPrbsInSlot += alloc.numPrbs;
    }
    
    os << "Total: " << totalPrbsInSlot << " PRBs used";
    
    if (m_totalBandwidthPrbs > 0)
    {
        double utilization = (static_cast<double>(totalPrbsInSlot) / m_totalBandwidthPrbs) * 100.0;
        os << " (" << std::fixed << std::setprecision(1) << utilization << "%)";
    }
    
    os << std::endl;
}

bool
NrBwpManager::ExportToCsv(const std::string& filename) const
{
    NS_LOG_FUNCTION(this << filename);
    
    if (!m_hasLoadedSolution)
    {
        NS_LOG_ERROR("No solution loaded, cannot export");
        return false;
    }
    
    std::ofstream file(filename);
    if (!file.is_open())
    {
        NS_LOG_ERROR("Failed to open file: " << filename);
        return false;
    }
    
    // Write header
    file << "slotId,ueId,startPrb,numPrbs" << std::endl;
    
    // Write allocations (sorted by slot ID for readability)
    std::vector<PrbAllocation> sortedAllocations = m_solution.allocations;
    std::sort(sortedAllocations.begin(), sortedAllocations.end(),
              [](const PrbAllocation& a, const PrbAllocation& b) {
                  if (a.slotId != b.slotId) return a.slotId < b.slotId;
                  return a.ueId < b.ueId;
              });
    
    for (const auto& alloc : sortedAllocations)
    {
        file << alloc.slotId << ","
             << alloc.ueId << ","
             << alloc.startPrb << ","
             << alloc.numPrbs << std::endl;
    }
    
    file.close();
    
    NS_LOG_INFO("Exported " << sortedAllocations.size() 
                << " allocations to " << filename);
    
    return true;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

void
NrBwpManager::BuildIndexedStructure()
{
    NS_LOG_FUNCTION(this);
    
    // Clear existing structure
    m_slotAllocations.clear();
    m_ueTotalPrbs.clear();
    
    // Group allocations by slot ID
    for (const auto& alloc : m_solution.allocations)
    {
        m_slotAllocations[alloc.slotId].push_back(alloc);
    }
    
    // Compute total PRBs per UE
    for (const auto& alloc : m_solution.allocations)
    {
        m_ueTotalPrbs[alloc.ueId] += alloc.numPrbs;
    }
    
    NS_LOG_INFO("Built indexed structure:");
    NS_LOG_INFO("  Active slots: " << m_slotAllocations.size());
    NS_LOG_INFO("  UEs with allocations: " << m_ueTotalPrbs.size());
}

void
NrBwpManager::ComputeStatistics()
{
    NS_LOG_FUNCTION(this);
    
    // Initialize
    m_statistics = Statistics();
    m_statistics.totalSlots = m_totalSlots;
    m_statistics.numActiveSlots = static_cast<uint32_t>(m_slotAllocations.size());
    m_statistics.numIdleSlots = m_totalSlots - m_statistics.numActiveSlots;
    m_statistics.slotUtilization = static_cast<double>(m_statistics.numActiveSlots) / 
                                    static_cast<double>(m_totalSlots);
    
    m_statistics.totalAllocations = static_cast<uint32_t>(m_solution.allocations.size());
    m_statistics.totalPrbsAllocated = 0;
    
    m_statistics.maxPrbsPerSlot = 0;
    m_statistics.minPrbsPerSlot = UINT32_MAX;
    
    m_statistics.maxUesPerSlot = 0;
    
    uint32_t totalPrbsInActiveSlots = 0;
    uint32_t totalUesInActiveSlots = 0;
    
    // Per-slot statistics
    for (const auto& [slotId, allocations] : m_slotAllocations)
    {
        uint32_t prbsInSlot = 0;
        
        for (const auto& alloc : allocations)
        {
            prbsInSlot += alloc.numPrbs;
            m_statistics.totalPrbsAllocated += alloc.numPrbs;
        }
        
        m_statistics.maxPrbsPerSlot = std::max(m_statistics.maxPrbsPerSlot, prbsInSlot);
        m_statistics.minPrbsPerSlot = std::min(m_statistics.minPrbsPerSlot, prbsInSlot);
        
        totalPrbsInActiveSlots += prbsInSlot;
        
        uint32_t uesInSlot = static_cast<uint32_t>(allocations.size());
        m_statistics.maxUesPerSlot = std::max(m_statistics.maxUesPerSlot, uesInSlot);
        totalUesInActiveSlots += uesInSlot;
    }
    
    // Averages
    if (m_statistics.numActiveSlots > 0)
    {
        m_statistics.avgPrbsPerActiveSlot = 
            static_cast<double>(totalPrbsInActiveSlots) / 
            static_cast<double>(m_statistics.numActiveSlots);
        
        m_statistics.avgUesPerActiveSlot = 
            static_cast<double>(totalUesInActiveSlots) / 
            static_cast<double>(m_statistics.numActiveSlots);
    }
    
    // Per-UE statistics
    m_statistics.prbsPerUe = m_ueTotalPrbs;
    
    NS_LOG_INFO("Statistics computed:");
    NS_LOG_INFO("  Total PRBs allocated: " << m_statistics.totalPrbsAllocated);
    NS_LOG_INFO("  Avg PRBs per active slot: " << m_statistics.avgPrbsPerActiveSlot);
    NS_LOG_INFO("  Avg UEs per active slot: " << m_statistics.avgUesPerActiveSlot);
}

bool
NrBwpManager::ValidateNoPrbOverlaps() const
{
    NS_LOG_FUNCTION(this);
    
    // Check each slot for PRB overlaps
    for (const auto& [slotId, allocations] : m_slotAllocations)
    {
        // Check all pairs of allocations in this slot
        for (size_t i = 0; i < allocations.size(); ++i)
        {
            for (size_t j = i + 1; j < allocations.size(); ++j)
            {
                const auto& a1 = allocations[i];
                const auto& a2 = allocations[j];
                
                // Compute end PRB indices
                uint32_t a1_end = a1.startPrb + a1.numPrbs;
                uint32_t a2_end = a2.startPrb + a2.numPrbs;
                
                // Check for overlap: [a1_start, a1_end) overlaps [a2_start, a2_end)
                bool overlap = (a1.startPrb < a2_end) && (a2.startPrb < a1_end);
                
                if (overlap)
                {
                    NS_LOG_ERROR("PRB overlap detected at slot " << slotId);
                    NS_LOG_ERROR("  Allocation 1: UE " << a1.ueId 
                                 << ", PRBs [" << a1.startPrb << "-" << (a1_end - 1) << "]");
                    NS_LOG_ERROR("  Allocation 2: UE " << a2.ueId 
                                 << ", PRBs [" << a2.startPrb << "-" << (a2_end - 1) << "]");
                    return false;
                }
            }
        }
    }
    
    NS_LOG_DEBUG("No PRB overlaps detected");
    return true;
}

// ============================================================================
// STATISTICS PRINT
// ============================================================================

void
NrBwpManager::Statistics::Print(std::ostream& os) const
{
    os << "Statistics {" << std::endl;
    os << "  Total slots: " << totalSlots << std::endl;
    os << "  Active slots: " << numActiveSlots 
       << " (" << (slotUtilization * 100) << "%)" << std::endl;
    os << "  Idle slots: " << numIdleSlots << std::endl;
    os << "  Total allocations: " << totalAllocations << std::endl;
    os << "  Total PRBs: " << totalPrbsAllocated << std::endl;
    os << "  Max PRBs/slot: " << maxPrbsPerSlot << std::endl;
    os << "  Min PRBs/slot: " << minPrbsPerSlot << std::endl;
    os << "  Avg PRBs/active slot: " << avgPrbsPerActiveSlot << std::endl;
    os << "  Max UEs/slot: " << maxUesPerSlot << std::endl;
    os << "  Avg UEs/active slot: " << avgUesPerActiveSlot << std::endl;
    os << "  Per-UE PRBs: {";
    for (const auto& [ueId, prbs] : prbsPerUe)
    {
        os << ueId << ":" << prbs << ", ";
    }
    os << "}" << std::endl;
    os << "}";
}

} // namespace ns3
