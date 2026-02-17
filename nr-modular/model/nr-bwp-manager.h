/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * BWP Manager - Header File
 * 
 * Responsibilities:
 * - Store MILP allocation blueprint in efficient indexed structure
 * - Provide O(1) slot-based lookup for MAC scheduler
 * - Track allocation progress and statistics
 * - Validate MILP solution integrity
 * - Support baseline comparison metrics
 * 
 * Design Pattern:
 * This class acts as a "smart lookup table" that pre-processes the MILP
 * solution once (O(N) complexity) to enable fast runtime queries (O(1)).
 * 
 * Performance:
 * - Load solution: O(N) where N = total allocations (~6000)
 * - Query per slot: O(1) average (hash map lookup)
 * - Query per UE: O(k) where k = UEs per slot (~1-3)
 * 
 * Usage:
 * 1. Create BWP Manager
 * 2. Load MILP solution → builds indexed structure
 * 3. During simulation: Query allocation at each slot
 * 4. After simulation: Get statistics for comparison
 */

#ifndef NR_BWP_MANAGER_H
#define NR_BWP_MANAGER_H

#include "ns3/object.h"
#include "utils/nr-milp-types.h"

#include <unordered_map>
#include <vector>
#include <optional>
#include <cstdint>

namespace ns3
{

/**
 * \ingroup nr-modular
 * 
 * \brief Bandwidth Part Manager for MILP-based Resource Scheduling
 * 
 * This class manages the optimal BWP allocation blueprint computed by the
 * external MILP solver. It stores allocations in an efficient indexed
 * structure to enable fast O(1) queries during runtime scheduling.
 * 
 * Architecture:
 * - MILP solver computes optimal allocation (offline, pre-simulation)
 * - BWP Manager loads and indexes the solution
 * - MAC Scheduler queries allocations at each TTI (every 0.5ms)
 * - BWP Manager provides instant lookup (no linear search)
 * 
 * Data Structure:
 * - Primary index: slotId → vector<PrbAllocation>
 * - For slot 5: returns all UE allocations at that slot
 * - Avoids O(6000) linear search, achieves O(1) lookup
 * 
 * Example Usage:
 * \code
 *   // 1. Create and load solution
 *   Ptr<NrBwpManager> bwpMgr = CreateObject<NrBwpManager>();
 *   bwpMgr->LoadMilpSolution(milpSolution);
 *   
 *   // 2. Query at each slot (called by MAC scheduler)
 *   uint32_t currentSlot = 5;
 *   auto allocations = bwpMgr->GetAllocationForSlot(currentSlot);
 *   // Returns: [{UE0, slot5, PRB0-9}, {UE1, slot5, PRB10-14}, ...]
 *   
 *   // 3. Execute allocations
 *   for (auto& alloc : allocations) {
 *       AssignPrbs(alloc.ueId, alloc.startPrb, alloc.numPrbs);
 *   }
 *   
 *   // 4. Get statistics after simulation
 *   auto stats = bwpMgr->GetStatistics();
 * \endcode
 */
class NrBwpManager : public Object
{
  public:
    /**
     * \brief Get the TypeId for this class
     * \return The TypeId
     */
    static TypeId GetTypeId();
    
    /**
     * \brief Constructor
     */
    NrBwpManager();
    
    /**
     * \brief Destructor
     */
    virtual ~NrBwpManager();
    
    // ========================================================================
    // MILP SOLUTION LOADING
    // ========================================================================
    
    /**
     * \brief Load MILP solution and build indexed lookup table
     * \param solution The MILP solution from external solver
     * \return true if solution loaded successfully, false otherwise
     * 
     * This method performs ONE-TIME pre-processing:
     * 1. Validates solution integrity (no overlaps, valid IDs)
     * 2. Builds indexed structure: slotId → allocations
     * 3. Computes statistics (total PRBs per UE, utilization)
     * 
     * Complexity: O(N) where N = total allocations
     * 
     * After this call, all query methods become available.
     * 
     * Example:
     * \code
     *   MilpSolution solution = milpInterface->SolveProblem(...);
     *   bool success = bwpManager->LoadMilpSolution(solution);
     *   if (!success) {
     *       NS_FATAL_ERROR("Failed to load MILP solution");
     *   }
     * \endcode
     */
    bool LoadMilpSolution(const MilpSolution& solution);
    
    /**
     * \brief Check if MILP solution has been loaded
     * \return true if solution loaded, false otherwise
     */
    bool HasSolution() const;
    
    /**
     * \brief Clear loaded solution and reset state
     * 
     * Useful for running multiple simulations with different solutions
     */
    void ClearSolution();
    
    // ========================================================================
    // SLOT-BASED QUERIES (Primary Interface for MAC Scheduler)
    // ========================================================================
    
    /**
     * \brief Get all PRB allocations for a specific slot
     * \param slotId Slot index (0-indexed, e.g., 0-1999 for 1-second sim)
     * \return Vector of allocations for this slot (all active UEs)
     * 
     * This is the PRIMARY method called by MAC scheduler at each TTI.
     * 
     * Returns allocations for ALL UEs active in this slot.
     * - If 3 UEs are active: returns vector of size 3
     * - If no UEs active (idle slot): returns empty vector
     * 
     * Complexity: O(1) average (hash map lookup)
     * 
     * Example:
     * \code
     *   uint32_t currentSlot = GetCurrentSlot();
     *   auto allocations = bwpManager->GetAllocationForSlot(currentSlot);
     *   
     *   // Might return:
     *   // [{ueId=0, startPrb=0, numPrbs=10},
     *   //  {ueId=1, startPrb=10, numPrbs=5},
     *   //  {ueId=2, startPrb=15, numPrbs=8}]
     * \endcode
     */
    std::vector<PrbAllocation> GetAllocationForSlot(uint32_t slotId) const;
    
    /**
     * \brief Check if a slot has any allocations
     * \param slotId Slot index
     * \return true if slot has allocations, false if idle
     * 
     * Useful for skipping empty slots without fetching allocations.
     * 
     * Complexity: O(1)
     * 
     * Example:
     * \code
     *   if (bwpManager->HasAllocationsForSlot(currentSlot)) {
     *       auto allocations = bwpManager->GetAllocationForSlot(currentSlot);
     *       // Process allocations
     *   } else {
     *       // Idle slot, skip scheduling
     *   }
     * \endcode
     */
    bool HasAllocationsForSlot(uint32_t slotId) const;
    
    /**
     * \brief Get number of UEs allocated in a specific slot
     * \param slotId Slot index
     * \return Number of UEs with allocations in this slot
     * 
     * Complexity: O(1)
     */
    uint32_t GetNumUesInSlot(uint32_t slotId) const;
    
    // ========================================================================
    // UE-SPECIFIC QUERIES
    // ========================================================================
    
    /**
     * \brief Get allocation for specific UE at specific slot
     * \param slotId Slot index
     * \param ueId UE identifier
     * \return PrbAllocation if UE has allocation at this slot, nullopt otherwise
     * 
     * Useful when scheduler needs allocation for one specific UE.
     * 
     * Complexity: O(k) where k = number of UEs in this slot (typically 1-3)
     * 
     * Example:
     * \code
     *   auto alloc = bwpManager->GetUeAllocationForSlot(currentSlot, 0);
     *   if (alloc.has_value()) {
     *       // UE 0 has allocation
     *       AssignPrbs(0, alloc->startPrb, alloc->numPrbs);
     *   } else {
     *       // UE 0 not scheduled in this slot
     *   }
     * \endcode
     */
    std::optional<PrbAllocation> GetUeAllocationForSlot(uint32_t slotId, 
                                                         uint32_t ueId) const;
    
    /**
     * \brief Get all allocations for a specific UE across all slots
     * \param ueId UE identifier
     * \return Vector of all allocations for this UE
     * 
     * Useful for analyzing per-UE allocation patterns.
     * 
     * Complexity: O(N) where N = total allocations for this UE
     * 
     * Example:
     * \code
     *   auto ueAllocations = bwpManager->GetUeAllocations(0);
     *   // Returns all slots where UE 0 was allocated resources
     *   // e.g., [{slot=0, ...}, {slot=5, ...}, {slot=10, ...}]
     * \endcode
     */
    std::vector<PrbAllocation> GetUeAllocations(uint32_t ueId) const;
    
    /**
     * \brief Get total PRBs allocated to a specific UE
     * \param ueId UE identifier
     * \return Total number of PRBs allocated across all slots
     * 
     * Complexity: O(1) (pre-computed during load)
     * 
     * Example:
     * \code
     *   uint32_t totalPrbs = bwpManager->GetTotalPrbsForUe(0);
     *   // Returns: 1000 (UE 0 got 1000 PRBs total)
     * \endcode
     */
    uint32_t GetTotalPrbsForUe(uint32_t ueId) const;
    
    // ========================================================================
    // STATISTICS AND VALIDATION
    // ========================================================================
    
    /**
     * \brief Statistics structure for BWP Manager
     * 
     * Contains aggregated statistics about the MILP allocation.
     * Useful for baseline comparison and performance analysis.
     */
    struct Statistics
    {
        uint32_t totalSlots;              ///< Total number of slots
        uint32_t numActiveSlots;          ///< Slots with allocations
        uint32_t numIdleSlots;            ///< Slots without allocations
        double slotUtilization;           ///< Active slots / Total slots
        
        uint32_t totalAllocations;        ///< Total allocation count
        uint32_t totalPrbsAllocated;      ///< Sum of all PRBs allocated
        
        uint32_t maxPrbsPerSlot;          ///< Max PRBs used in any slot
        uint32_t minPrbsPerSlot;          ///< Min PRBs used (excluding idle)
        double avgPrbsPerActiveSlot;      ///< Avg PRBs per active slot
        
        uint32_t maxUesPerSlot;           ///< Max UEs in any slot
        double avgUesPerActiveSlot;       ///< Avg UEs per active slot
        
        std::unordered_map<uint32_t, uint32_t> prbsPerUe;  ///< Total PRBs per UE
        
        /**
         * \brief Print statistics to output stream
         * \param os Output stream
         */
        void Print(std::ostream& os) const;
    };
    
    /**
     * \brief Get allocation statistics
     * \return Statistics structure
     * 
     * Provides comprehensive statistics for analysis and comparison.
     * 
     * Example:
     * \code
     *   auto stats = bwpManager->GetStatistics();
     *   stats.Print(std::cout);
     *   
     *   // Output:
     *   // Total slots: 2000
     *   // Active slots: 1800 (90%)
     *   // Total allocations: 5400
     *   // Avg PRBs per active slot: 246.5
     *   // ...
     * \endcode
     */
    Statistics GetStatistics() const;
    
    /**
     * \brief Get the original MILP solution
     * \return Reference to stored MILP solution
     * 
     * Useful for accessing solver metadata (status, solve time, etc.)
     */
    const MilpSolution& GetMilpSolution() const;
    
    /**
     * \brief Validate solution integrity
     * \return true if solution is valid, false otherwise
     * 
     * Performs comprehensive checks:
     * - No PRB overlaps within each slot
     * - All allocations within bounds
     * - Indexed structure matches raw allocations
     * 
     * Called automatically during LoadMilpSolution().
     */
    bool ValidateSolution() const;
    
    // ========================================================================
    // DEBUGGING AND VISUALIZATION
    // ========================================================================
    
    /**
     * \brief Print allocation summary to output stream
     * \param os Output stream
     * 
     * Prints human-readable summary of loaded solution.
     */
    void PrintSummary(std::ostream& os) const;
    
    /**
     * \brief Print allocations for a specific slot
     * \param slotId Slot index
     * \param os Output stream
     * 
     * Useful for debugging specific slots.
     * 
     * Example output:
     * \code
     *   Slot 5 allocations:
     *     UE 0: PRBs [0-9] (10 PRBs)
     *     UE 1: PRBs [10-14] (5 PRBs)
     *     UE 2: PRBs [15-22] (8 PRBs)
     *   Total: 23 PRBs used
     * \endcode
     */
    void PrintSlotAllocations(uint32_t slotId, std::ostream& os) const;
    
    /**
     * \brief Export allocations to CSV file
     * \param filename Output CSV filename
     * \return true if export successful
     * 
     * Format: slotId,ueId,startPrb,numPrbs
     * 
     * Useful for visualization in Excel/Python.
     */
    bool ExportToCsv(const std::string& filename) const;
    
  protected:
    /**
     * \brief Destructor implementation
     */
    void DoDispose() override;
    
  private:
    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================
    
    /**
     * \brief Build indexed lookup table from raw allocations
     * 
     * Groups allocations by slot ID for O(1) lookup.
     * Called during LoadMilpSolution().
     */
    void BuildIndexedStructure();
    
    /**
     * \brief Compute statistics from allocations
     * 
     * Pre-computes all statistics for fast GetStatistics() queries.
     * Called during LoadMilpSolution().
     */
    void ComputeStatistics();
    
    /**
     * \brief Validate no PRB overlaps within each slot
     * \return true if no overlaps detected
     */
    bool ValidateNoPrbOverlaps() const;
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /**
     * \brief Original MILP solution (unprocessed)
     * 
     * Stored for reference and metadata access.
     */
    MilpSolution m_solution;
    
    /**
     * \brief Indexed allocation structure (PRIMARY DATA STRUCTURE)
     * 
     * Key: slotId (uint32_t)
     * Value: vector of all allocations for that slot
     * 
     * Example:
     * {
     *   0: [{ueId=0, startPrb=0, numPrbs=10}, {ueId=1, startPrb=10, numPrbs=5}],
     *   1: [{ueId=0, startPrb=0, numPrbs=10}, {ueId=2, startPrb=10, numPrbs=8}],
     *   5: [{ueId=1, startPrb=0, numPrbs=5}],
     *   ...
     * }
     * 
     * Enables O(1) lookup: m_slotAllocations[slotId]
     */
    std::unordered_map<uint32_t, std::vector<PrbAllocation>> m_slotAllocations;
    
    /**
     * \brief Per-UE total PRBs (pre-computed)
     * 
     * Key: ueId
     * Value: total PRBs allocated to this UE
     * 
     * Enables O(1) query for GetTotalPrbsForUe()
     */
    std::unordered_map<uint32_t, uint32_t> m_ueTotalPrbs;
    
    /**
     * \brief Cached statistics
     * 
     * Pre-computed during LoadMilpSolution() for fast access.
     */
    Statistics m_statistics;
    
    /**
     * \brief Flag indicating if solution has been loaded
     */
    bool m_hasLoadedSolution;
    
    /**
     * \brief Total number of slots (from problem)
     */
    uint32_t m_totalSlots;
    
    /**
     * \brief Total bandwidth in PRBs (from problem)
     */
    uint32_t m_totalBandwidthPrbs;
    
    /**
     * \brief Number of UEs (from problem)
     */
    uint32_t m_numUes;
};

} // namespace ns3

#endif /* NR_BWP_MANAGER_H */
