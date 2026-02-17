/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * MILP Executor Scheduler - Header File
 * 
 * ============================================================================
 * CRITICAL COMPONENT: This scheduler executes MILP-optimized allocations
 * ============================================================================
 * 
 * Purpose:
 * This scheduler implements "Baseline 2" - the MILP Blind Executor.
 * It takes pre-computed optimal allocations from the MILP solver and
 * executes them WITHOUT considering runtime channel conditions.
 * 
 * Why "Blind Executor"?
 * - IGNORES current CQI (Channel Quality Indicator)
 * - IGNORES buffer status
 * - IGNORES HARQ feedback
 * - Simply executes the pre-computed MILP allocation plan
 * 
 * This serves as the optimal baseline for comparison with:
 * - Baseline 1: Traditional heuristics (PF/RR)
 * - Proposed: MILP + RL adaptation
 * 
 * Architecture:
 * 
 *   ┌──────────────────────────────────────────────────────┐
 *   │ MILP Solver (Python - Offline)                       │
 *   │ - Solves optimization problem once                   │
 *   │ - Returns 6000 allocations (2000 slots × 3 UEs)     │
 *   └────────────────┬─────────────────────────────────────┘
 *                    ↓
 *   ┌──────────────────────────────────────────────────────┐
 *   │ BWP Manager (C++ - Pre-simulation)                   │
 *   │ - Stores allocations in O(1) indexed structure       │
 *   │ - Indexed by slotId for fast lookup                  │
 *   └────────────────┬─────────────────────────────────────┘
 *                    ↓
 *   ┌──────────────────────────────────────────────────────┐
 *   │ MILP Executor Scheduler (C++ - Runtime)              │
 *   │ THIS CLASS - Called every 0.5ms                      │
 *   │                                                       │
 *   │ For each slot:                                        │
 *   │   1. Query BWP Manager: "What to allocate?"          │
 *   │   2. Convert MILP format → ns-3 format               │
 *   │   3. Create DCIs for PHY layer                       │
 *   │   4. Send to UEs (blind execution)                   │
 *   └──────────────────────────────────────────────────────┘
 * 
 * Data Flow (Every Slot):
 * 
 *   Current Slot = 5
 *        ↓
 *   Query BWP Manager
 *        ↓
 *   MILP Allocations for Slot 5:
 *   - UE 0: PRBs [0-99]
 *   - UE 1: PRBs [100-172]
 *   - UE 2: PRBs [173-272]
 *        ↓
 *   Convert PRB → RBG
 *   (PRB = Physical Resource Block, RBG = Resource Block Group)
 *        ↓
 *   Create DCIs (Downlink Control Information)
 *   - DCI for UE 0: RBGs [0-24], Symbols [1-12], MCS 16
 *   - DCI for UE 1: RBGs [25-42], Symbols [1-12], MCS 16
 *   - DCI for UE 2: RBGs [43-67], Symbols [1-12], MCS 16
 *        ↓
 *   Send to PHY → Transmit to UEs
 */

#ifndef NR_MILP_EXECUTOR_SCHEDULER_H
#define NR_MILP_EXECUTOR_SCHEDULER_H

#include "ns3/nr-mac-scheduler-tdma.h"
#include "nr-bwp-manager.h"
#include "nr-network-manager.h"

#include <unordered_map>

namespace ns3
{

/**
 * \ingroup nr-modular
 * 
 * \brief MILP Blind Executor Scheduler (Baseline 2)
 * 
 * This scheduler executes pre-computed MILP allocations without runtime
 * adaptation. It serves as the "optimal" baseline for comparison.
 * 
 * Key Characteristics:
 * - NO CQI consideration (blind to channel quality)
 * - NO buffer awareness (ignores queue status)
 * - NO HARQ retransmissions (only fresh data)
 * - FIXED MCS (from UE SLA configuration)
 * - OFDMA-style allocation (multiple UEs per slot)
 * 
 * Inheritance:
 * - Inherits from NrMacSchedulerTdma (simpler than OFDMA base)
 * - Overrides pure virtual methods to implement MILP execution
 * - Adapts TDMA methods to support OFDMA-style allocation
 * 
 * Usage Example:
 * \code
 *   // 1. Create scheduler
 *   Ptr<NrMilpExecutorScheduler> scheduler = 
 *       CreateObject<NrMilpExecutorScheduler>();
 *   
 *   // 2. Set BWP Manager (contains MILP allocations)
 *   scheduler->SetBwpManager(bwpManager);
 *   
 *   // 3. Initialize RNTI mapping
 *   scheduler->Initialize(networkManager);
 *   
 *   // 4. Install on gNB
 *   nrHelper->InstallMacScheduler(gnbDevices, scheduler);
 *   
 *   // 5. Scheduler runs automatically during simulation
 *   //    Called every 0.5ms to schedule slot resources
 * \endcode
 */
class NrMilpExecutorScheduler : public NrMacSchedulerTdma
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
    NrMilpExecutorScheduler();
    
    /**
     * \brief Destructor
     */
    virtual ~NrMilpExecutorScheduler() override;
    
    // ========================================================================
    // CONFIGURATION AND INITIALIZATION
    // ========================================================================
    
    /**
     * \brief Set the BWP Manager containing MILP allocations
     * \param bwpManager Pointer to BWP Manager
     * 
     * Must be called BEFORE simulation starts.
     * The BWP Manager must already have the MILP solution loaded.
     * 
     * Example:
     * \code
     *   scheduler->SetBwpManager(bwpManager);
     * \endcode
     */
    void SetBwpManager(Ptr<NrBwpManager> bwpManager);
    
    /**
     * \brief Initialize RNTI mapping from Network Manager
     * \param networkManager Pointer to Network Manager
     * 
     * This method caches the mapping between:
     * - ueId (0, 1, 2) ↔ RNTI (assigned during attachment)
     * 
     * Must be called AFTER UE attachment (when RNTIs are assigned).
     * 
     * Why cache?
     * - Scheduler is called every 0.5ms (2000 times/second)
     * - Needs ultra-fast O(1) lookup
     * - RNTI rarely changes (only on handover)
     * 
     * Example:
     * \code
     *   // After UE attachment:
     *   scheduler->Initialize(networkManager);
     * \endcode
     */
    void Initialize(Ptr<NrNetworkManager> networkManager);
    
    // ========================================================================
    // OVERRIDE: Pure Virtual Methods from NrMacSchedulerNs3
    // ========================================================================
    
  protected:
    
    /**
     * \brief Assign DL RBGs to UEs based on MILP allocation
     * \param symAvail Number of available symbols
     * \param activeDl Map of active DL UEs per beam
     * \return Map of symbols allocated per beam
     * 
     * CRITICAL METHOD - Called every DL slot!
     * 
     * This is where we query the BWP Manager for MILP allocations
     * and assign RBGs to UEs.
     * 
     * Flow:
     * 1. Get current slot index
     * 2. Query BWP Manager: GetAllocationForSlot(currentSlot)
     * 3. For each MILP allocation:
     *    a. Map ueId → RNTI
     *    b. Convert PRB → RBG
     *    c. Store in UE info (m_dlRBG, m_dlRbgStart)
     * 4. Return all symbols for single beam (OFDMA style)
     * 
     * Why OFDMA style?
     * - MILP allocates multiple UEs in same slot
     * - Each UE gets different frequency (PRBs)
     * - All UEs use all symbols (frequency division)
     * 
     * Inherited from NrMacSchedulerNs3.
     */
    BeamSymbolMap AssignDLRBG(uint32_t symAvail, 
                              const ActiveUeMap& activeDl) const override;
    
    /**
     * \brief Assign UL RBGs to UEs based on MILP allocation
     * \param symAvail Number of available symbols
     * \param activeUl Map of active UL UEs per beam
     * \return Map of symbols allocated per beam
     * 
     * Same as AssignDLRBG() but for uplink.
     * 
     * Inherited from NrMacSchedulerNs3.
     */
    BeamSymbolMap AssignULRBG(uint32_t symAvail, 
                              const ActiveUeMap& activeUl) const override;
    
    /**
     * \brief Beam switching hook for DL (not used in single-beam scenario)
     * \param spoint Starting point in frequency-time plane
     * \param symOfBeam Number of symbols used by this beam
     * 
     * In single-beam scenario, this is a no-op.
     * Override required by base class.
     * 
     * Inherited from NrMacSchedulerNs3.
     */
    void ChangeDlBeam(PointInFTPlane* spoint, uint32_t symOfBeam) const override;
    
    /**
     * \brief Beam switching hook for UL (not used in single-beam scenario)
     * \param spoint Starting point in frequency-time plane
     * \param symOfBeam Number of symbols used by this beam
     * 
     * In single-beam scenario, this is a no-op.
     * Override required by base class.
     * 
     * Inherited from NrMacSchedulerNs3.
     */
    void ChangeUlBeam(PointInFTPlane* spoint, uint32_t symOfBeam) const override;
    
    /**
     * \brief Create DL DCI from MILP allocation
     * \param spoint Starting point in frequency-time plane
     * \param ueInfo UE information structure
     * \param maxSym Maximum symbols available
     * \return DCI for this UE, or nullptr if no allocation
     * 
     * CRITICAL METHOD - Creates the actual allocation message!
     * 
     * This converts MILP allocation → DCI (Downlink Control Information)
     * that tells the PHY layer exactly how to transmit.
     * 
     * Flow:
     * 1. Map RNTI → ueId
     * 2. Query MILP allocation for this UE at current slot
     * 3. If no allocation, return nullptr
     * 4. Convert PRB → RBG and create bitmask
     * 5. Assign symbols (all symbols - OFDMA)
     * 6. Set MCS (fixed from SLA)
     * 7. Calculate TBS (Transport Block Size)
     * 8. Return DCI
     * 
     * DCI Structure:
     * - m_rnti: Which UE
     * - m_rbgBitmask: Which RBGs (frequency)
     * - m_symStart, m_numSym: Which symbols (time)
     * - m_mcs: Modulation/Coding Scheme
     * - m_tbs: How many bytes can be transmitted
     * 
     * Inherited from NrMacSchedulerNs3.
     */
    std::shared_ptr<DciInfoElementTdma> CreateDlDci(
        PointInFTPlane* spoint,
        const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
        uint32_t maxSym) const override;
    
    /**
     * \brief Create UL DCI from MILP allocation
     * \param spoint Starting point in frequency-time plane
     * \param ueInfo UE information structure
     * \param maxSym Maximum symbols available
     * \return DCI for this UE, or nullptr if no allocation
     * 
     * Same as CreateDlDci() but for uplink.
     * 
     * Inherited from NrMacSchedulerNs3.
     */
    std::shared_ptr<DciInfoElementTdma> CreateUlDci(
        PointInFTPlane* spoint,
        const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
        uint32_t maxSym) const override;
    
    /**
     * \brief Create UE representation (called during UE registration)
     * \param params UE configuration parameters
     * \return Pointer to UE information structure
     * 
     * Creates the UE representation used by scheduler.
     * We use the TDMA version (inherited from NrMacSchedulerTdmaPF).
     * 
     * Inherited from NrMacSchedulerNs3.
     */
    std::shared_ptr<NrMacSchedulerUeInfo> CreateUeRepresentation(
        const NrMacCschedSapProvider::CschedUeConfigReqParameters& params) const override;
    
    /**
     * \brief Get Transmit Power Control command
     * \return TPC value (always 1 for MILP executor)
     * 
     * We use fixed TPC since MILP doesn't optimize power.
     * 
     * Inherited from NrMacSchedulerNs3.
     */
    uint8_t GetTpc() const override;
    
    // ========================================================================
    // OVERRIDE: Pure Virtual Methods from NrMacSchedulerTdma
    // ========================================================================
    
    /**
     * \brief Get UE comparison function for DL scheduling
     * \return Comparison function (not used in MILP executor)
     * 
     * MILP executor doesn't need to sort UEs - allocation is pre-computed.
     * Return a dummy function.
     */
    std::function<bool(const UePtrAndBufferReq&, const UePtrAndBufferReq&)>
    GetUeCompareDlFn() const override;
    
    /**
     * \brief Get UE comparison function for UL scheduling
     * \return Comparison function (not used in MILP executor)
     * 
     * MILP executor doesn't need to sort UEs - allocation is pre-computed.
     * Return a dummy function.
     */
    std::function<bool(const UePtrAndBufferReq&, const UePtrAndBufferReq&)>
    GetUeCompareUlFn() const override;
    
    /**
     * \brief Called after DL resources are assigned to a UE
     * \param ue UE and buffer info
     * \param assigned Assigned resources
     * \param totAssigned Total assigned resources
     * 
     * Hook for post-assignment operations. No-op for MILP executor.
     */
    void AssignedDlResources(const UePtrAndBufferReq& ue,
                             const FTResources& assigned,
                             const FTResources& totAssigned) const override;
    
    /**
     * \brief Called after UL resources are assigned to a UE
     * \param ue UE and buffer info
     * \param assigned Assigned resources
     * \param totAssigned Total assigned resources
     * 
     * Hook for post-assignment operations. No-op for MILP executor.
     */
    void AssignedUlResources(const UePtrAndBufferReq& ue,
                             const FTResources& assigned,
                             const FTResources& totAssigned) const override;
    
    /**
     * \brief Called when DL resources are NOT assigned to a UE
     * \param ue UE and buffer info
     * \param notAssigned Resources that were not assigned
     * \param totAssigned Total assigned resources
     * 
     * Hook for handling unassigned resources. No-op for MILP executor.
     */
    void NotAssignedDlResources(const UePtrAndBufferReq& ue,
                                 const FTResources& notAssigned,
                                 const FTResources& totAssigned) const override;
    
    /**
     * \brief Called when UL resources are NOT assigned to a UE
     * \param ue UE and buffer info
     * \param notAssigned Resources that were not assigned
     * \param totAssigned Total assigned resources
     * 
     * Hook for handling unassigned resources. No-op for MILP executor.
     */
    void NotAssignedUlResources(const UePtrAndBufferReq& ue,
                                 const FTResources& notAssigned,
                                 const FTResources& totAssigned) const override;
    
    /**
     * \brief Called before DL scheduling for a UE
     * \param ue UE and buffer info
     * \param assignableInIteration Resources assignable in this iteration
     * 
     * Hook for pre-scheduling operations. No-op for MILP executor.
     */
    void BeforeDlSched(const UePtrAndBufferReq& ue,
                       const FTResources& assignableInIteration) const override;
    
    /**
     * \brief Called before UL scheduling for a UE
     * \param ue UE and buffer info
     * \param assignableInIteration Resources assignable in this iteration
     * 
     * Hook for pre-scheduling operations. No-op for MILP executor.
     */
    void BeforeUlSched(const UePtrAndBufferReq& ue,
                       const FTResources& assignableInIteration) const override;
    
    // ========================================================================
    // OVERRIDE: Trigger Methods (Track Current Slot)
    // ========================================================================
    
    /**
     * \brief DL scheduling trigger (called every DL slot)
     * \param params Trigger parameters including slot information
     * 
     * This method is called by MAC every DL slot (every 0.5ms).
     * We override it to extract and store the current slot index.
     * 
     * Why track slot?
     * - MILP allocations are indexed by slotId
     * - Need to know which slot we're scheduling for
     * - Query BWP Manager with current slot
     * 
     * Inherited from NrMacScheduler.
     */
    void DoSchedDlTriggerReq(
        const NrMacSchedSapProvider::SchedDlTriggerReqParameters& params) override;
    
    /**
     * \brief UL scheduling trigger (called every UL slot)
     * \param params Trigger parameters including slot information
     * 
     * Same as DoSchedDlTriggerReq() but for uplink.
     * 
     * Inherited from NrMacScheduler.
     */
    void DoSchedUlTriggerReq(
        const NrMacSchedSapProvider::SchedUlTriggerReqParameters& params) override;
    
  private:
    // ========================================================================
    // HELPER METHODS
    // ========================================================================
    
    /**
     * \brief Convert PRB allocation to RBG bitmask
     * \param startPrb Starting PRB index
     * \param numPrbs Number of contiguous PRBs
     * \param rbgSize Number of PRBs per RBG
     * \param totalRbgs Total number of RBGs in bandwidth
     * \return Bitmask where true = RBG allocated
     * 
     * MILP works in PRBs, ns-3 works in RBGs.
     * This method converts between the two.
     * 
     * Example:
     * - Input: startPrb=0, numPrbs=10, rbgSize=4
     * - Output: RBGs [0, 1, 2] = true (covering PRBs 0-11)
     * 
     * Rounding up to cover all PRBs.
     */
    std::vector<bool> ConvertPrbToRbgBitmask(uint32_t startPrb,
                                              uint32_t numPrbs,
                                              uint32_t rbgSize,
                                              uint32_t totalRbgs) const;
    
    /**
     * \brief Get current slot index
     * \return Slot index (0-based)
     * 
     * Extracted from trigger parameters and cached.
     */
    uint32_t GetCurrentSlot() const;
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    /**
     * \brief BWP Manager (contains MILP allocations)
     * 
     * Queried every slot to get allocations.
     */
    Ptr<NrBwpManager> m_bwpManager;
    
    /**
     * \brief Map: ueId → RNTI (cached for fast access)
     * 
     * Initialized once during Initialize().
     * Used for O(1) lookup during scheduling.
     */
    mutable std::unordered_map<uint32_t, uint16_t> m_ueIdToRnti;
    
    /**
     * \brief Map: RNTI → ueId (reverse mapping)
     * 
     * Needed to convert ns-3 RNTI back to MILP ueId.
     */
    mutable std::unordered_map<uint16_t, uint32_t> m_rntiToUeId;
    
    /**
     * \brief Current slot being scheduled
     * 
     * Updated every trigger (DoSchedDlTriggerReq, DoSchedUlTriggerReq).
     * Used to query BWP Manager for correct slot allocations.
     */
    mutable uint32_t m_currentSlot;
    
    /**
     * \brief RBG size (PRBs per RBG)
     * 
     * Cached during first access for efficiency.
     */
    mutable uint32_t m_rbgSize;
    
    /**
     * \brief Total number of RBGs in bandwidth
     * 
     * Cached during first access for efficiency.
     */
    mutable uint32_t m_totalRbgs;
    
    /**
     * \brief Flag indicating if scheduler is initialized
     */
    bool m_initialized;
};

} // namespace ns3

#endif /* NR_MILP_EXECUTOR_SCHEDULER_H */