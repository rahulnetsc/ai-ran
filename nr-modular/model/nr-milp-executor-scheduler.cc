/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * MILP Executor Scheduler - Implementation File
 */

#include "nr-milp-executor-scheduler.h"

#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/uinteger.h"
#include "ns3/nr-mac-scheduler-ue-info-pf.h"  // For CreateUeRepresentation
#include "ns3/nr-phy-mac-common.h"             // For DciFormat, VarTtiType

#include <algorithm>
#include <functional>  // For std::bind

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrMilpExecutorScheduler");
NS_OBJECT_ENSURE_REGISTERED(NrMilpExecutorScheduler);

// ============================================================================
// TYPE ID
// ============================================================================

TypeId
NrMilpExecutorScheduler::GetTypeId()
{
    static TypeId tid = 
        TypeId("ns3::NrMilpExecutorScheduler")
            .SetParent<NrMacSchedulerTdma>()
            .SetGroupName("NrModular")
            .AddConstructor<NrMilpExecutorScheduler>();
    
    return tid;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

NrMilpExecutorScheduler::NrMilpExecutorScheduler()
    : m_bwpManager(nullptr),
      m_currentSlot(0),
      m_rbgSize(0),
      m_totalRbgs(0),
      m_initialized(false)
{
    NS_LOG_FUNCTION(this);
}

NrMilpExecutorScheduler::~NrMilpExecutorScheduler()
{
    NS_LOG_FUNCTION(this);
}

// ============================================================================
// CONFIGURATION AND INITIALIZATION
// ============================================================================

void
NrMilpExecutorScheduler::SetBwpManager(Ptr<NrBwpManager> bwpManager)
{
    NS_LOG_FUNCTION(this << bwpManager);
    
    NS_ABORT_MSG_IF(bwpManager == nullptr, "BWP Manager cannot be null");
    
    m_bwpManager = bwpManager;
    
    NS_LOG_INFO("BWP Manager set for MILP Executor Scheduler");
}

void
NrMilpExecutorScheduler::Initialize(Ptr<NrNetworkManager> networkManager)
{
    NS_LOG_FUNCTION(this << networkManager);
    
    NS_ABORT_MSG_IF(networkManager == nullptr, "Network Manager cannot be null");
    
    /*
     * CRITICAL INITIALIZATION: Cache RNTI Mappings
     * 
     * Why?
     * - Scheduler is called 2000 times/second (every 0.5ms)
     * - Needs ultra-fast O(1) lookup between ueId ↔ RNTI
     * - Network Manager provides direct access, but we cache for speed
     * 
     * When?
     * - Call AFTER UE attachment (RNTIs assigned during attachment)
     * - Call BEFORE simulation starts
     */
    
    uint32_t numUes = networkManager->GetUeDevices().GetN();
    
    NS_LOG_INFO("Initializing RNTI mapping for " << numUes << " UEs");
    
    // Clear any existing mappings
    m_ueIdToRnti.clear();
    m_rntiToUeId.clear();
    
    // Build bidirectional mapping
    for (uint32_t ueId = 0; ueId < numUes; ueId++)
    {
        // Get RNTI from Network Manager (which gets it from NrUeMac)
        uint16_t rnti = networkManager->GetUeRnti(ueId);
        
        // Store in both directions for O(1) lookup
        m_ueIdToRnti[ueId] = rnti;
        m_rntiToUeId[rnti] = ueId;
        
        NS_LOG_INFO("  UE " << ueId << " ↔ RNTI " << rnti);
    }
    
    m_initialized = true;
    
    NS_LOG_INFO("MILP Executor Scheduler initialized successfully");
}

// ============================================================================
// TRIGGER METHODS (Track Current Slot)
// ============================================================================

void
NrMilpExecutorScheduler::DoSchedDlTriggerReq(
    const NrMacSchedSapProvider::SchedDlTriggerReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    
    /*
     * CRITICAL: Extract Current Slot as Absolute Index
     *
     * MILP allocations are indexed by absolute slot number (0, 1, 2, ... N-1).
     * In SetupMilpScheduler, slots are generated as: alloc.slotId = slot (0-indexed).
     *
     * SfnSf encodes: frame number, subframe number, slot number within subframe.
     * GetEncoding() returns a PACKED BITFIELD — NOT an absolute index.
     * Using GetEncoding() directly causes unordered_map::at() crash in the parent
     * because the packed value (e.g. 984576) is not a key in m_slotAllocations.
     *
     * Correct formula for μ=1 (30 kHz SCS, 0.5 ms slots):
     *   slotsPerSubframe = 2^μ = 2
     *   slotsPerFrame    = 10 * 2 = 20
     *   absoluteSlot     = frame * 20 + subframe * 2 + slot
     *
     * Example: t=0.153s → absolute slot 306
     *   frame=15, subframe=3, slot=0 → 15*20 + 3*2 + 0 = 306 ✓
     */
    {
        uint32_t frame    = params.m_snfSf.GetFrame();
        uint32_t subframe = params.m_snfSf.GetSubframe();
        uint32_t slot     = params.m_snfSf.GetSlot();
        // μ=1 → 2 slots/subframe, 20 slots/frame (fixed in this project)
        m_currentSlot = frame * 20 + subframe * 2 + slot;
    }
    
    std::cout << "[MILP-DBG] DL Trigger: slot=" << m_currentSlot
              << " harqFeedback=" << params.m_dlHarqInfoList.size() << std::flush << std::endl;

    // Call parent implementation (handles standard scheduling flow)
    try
    {
        NrMacSchedulerTdma::DoSchedDlTriggerReq(params);
        std::cout << "[MILP-DBG] DL Trigger DONE: slot=" << m_currentSlot << std::flush << std::endl;
    }
    catch (const std::out_of_range& e)
    {
        std::cerr << "[MILP-CRASH] unordered_map::at in DL parent at slot=" << m_currentSlot
                  << " exception: " << e.what() << std::endl;
        std::cerr << "  harqList.size()=" << params.m_dlHarqInfoList.size() << std::endl;
        throw;  // re-throw so simulation still reports error
    }
    NS_LOG_DEBUG("DL Trigger for slot " << m_currentSlot);
}

void
NrMilpExecutorScheduler::DoSchedUlTriggerReq(
    const NrMacSchedSapProvider::SchedUlTriggerReqParameters& params)
{
    NS_LOG_FUNCTION(this);
    
    /*
     * Same as DL trigger — convert SfnSf to absolute slot index.
     *
     * Note: UL scheduling happens with N2 delay (N2=2 slots for μ=1).
     * params.m_snfSf refers to the UL slot being prepared, not the current DL slot.
     * We use the same absolute conversion so MILP allocations match correctly.
     */
    {
        uint32_t frame    = params.m_snfSf.GetFrame();
        uint32_t subframe = params.m_snfSf.GetSubframe();
        uint32_t slot     = params.m_snfSf.GetSlot();
        m_currentSlot = frame * 20 + subframe * 2 + slot;
    }
    
    std::cout << "[MILP-DBG] UL Trigger: slot=" << m_currentSlot << std::flush << std::endl;

    // Call parent implementation
    try
    {
        NrMacSchedulerTdma::DoSchedUlTriggerReq(params);
    }
    catch (const std::out_of_range& e)
    {
        std::cerr << "[MILP-CRASH] unordered_map::at in UL parent at slot=" << m_currentSlot
                  << " exception: " << e.what() << std::endl;
        throw;
    }
    NS_LOG_DEBUG("UL Trigger for slot " << m_currentSlot);
}

// ============================================================================
// RESOURCE ASSIGNMENT (Query MILP Allocations)
// ============================================================================

NrMacSchedulerNs3::BeamSymbolMap
NrMilpExecutorScheduler::AssignDLRBG(uint32_t symAvail, 
                                     const ActiveUeMap& activeDl) const
{
    // ── VERY FIRST LINE - if this doesn't print, crash is BEFORE AssignDLRBG ──
    std::cout << "[MILP-DBG] >>> AssignDLRBG ENTERED slot=" << m_currentSlot
              << " symAvail=" << symAvail
              << " activeDl.beams=" << activeDl.size() << std::flush << std::endl;

    NS_LOG_FUNCTION(this << symAvail << activeDl.size());
    
    /*
     * ========================================================================
     * CRITICAL METHOD: Assign DL Resources Based on MILP Solution
     * ========================================================================
     * 
     * This is THE KEY method where MILP allocations are queried and assigned!
     * 
     * Flow:
     * 1. Check if BWP Manager and initialization are ready
     * 2. Query BWP Manager for MILP allocations at current slot
     * 3. For each MILP allocation:
     *    - Map ueId → RNTI
     *    - Find UE in activeDl map
     *    - Convert PRB → RBG
     *    - Store RBG allocation in UE info
     * 4. Return symbols allocated per beam (OFDMA: all UEs use all symbols)
     * 
     * Why OFDMA style?
     * - MILP assigns different PRBs to different UEs in SAME slot
     * - All UEs transmit simultaneously on different frequencies
     * - Each UE uses ALL available symbols
     */
    
    // ========================================================================
    // Step 1: Validate State
    // ========================================================================
    
    if (!m_bwpManager || !m_initialized)
    {
        NS_LOG_WARN("BWP Manager not set or scheduler not initialized");
        BeamSymbolMap emptyMap;
        return emptyMap;
    }
    
    // Initialize RBG size and total RBGs (cached for efficiency)
    if (m_rbgSize == 0)
    {
        m_rbgSize = GetNumRbPerRbg();
        m_totalRbgs = GetBandwidthInRbg();
        
        NS_LOG_INFO("Cached RBG parameters: rbgSize=" << m_rbgSize 
                    << ", totalRbgs=" << m_totalRbgs);
    }
    
    // ========================================================================
    // Step 2: Query MILP Allocations for Current Slot
    // ========================================================================
    
    // Get MILP allocations for this slot
    auto milpAllocations = m_bwpManager->GetAllocationForSlot(m_currentSlot);
    
    NS_LOG_DEBUG("Slot " << m_currentSlot << ": Found " << milpAllocations.size() 
                 << " MILP allocations");
    
    if (milpAllocations.empty())
    {
        NS_LOG_DEBUG("No MILP allocations for this slot (idle slot)");
        BeamSymbolMap emptyMap;
        return emptyMap;
    }
    
    // ========================================================================
    // Step 3: Assign RBGs to Each UE Based on MILP Allocation
    // ========================================================================
    
    for (const auto& alloc : milpAllocations)
    {
        /*
         * For each MILP allocation:
         * 
         * alloc.ueId      = MILP UE identifier (0, 1, 2)
         * alloc.slotId    = Slot index (should match m_currentSlot)
         * alloc.startPrb  = Starting PRB index (frequency)
         * alloc.numPrbs   = Number of contiguous PRBs
         */
        
        // Map ueId → RNTI
        auto rntiIt = m_ueIdToRnti.find(alloc.ueId);
        if (rntiIt == m_ueIdToRnti.end())
        {
            NS_LOG_WARN("UE " << alloc.ueId << " not found in RNTI mapping");
            continue;
        }
        uint16_t rnti = rntiIt->second;
        
        // Find this UE in the active UEs map
        // activeDl is grouped by beam ID, but we're single-beam
        bool ueFound = false;
        for (const auto& [beamId, ueList] : activeDl)
        {
            for (const auto& [ueInfo, bufferStatus] : ueList)
            {
                if (ueInfo->m_rnti == rnti)
                {
                    /*
                     * Found the UE! Now convert MILP allocation to RBG format.
                     * 
                     * MILP uses PRBs (Physical Resource Blocks)
                     * ns-3 uses RBGs (Resource Block Groups)
                     * 
                     * Conversion:
                     * - startRbg = startPrb / rbgSize
                     * - numRbg = ceil(numPrbs / rbgSize)
                     */
                    
                    uint32_t startRbg = alloc.startPrb / m_rbgSize;
                    uint32_t numRbg = (alloc.numPrbs + m_rbgSize - 1) / m_rbgSize;  // Round up
                    
                    // Store in UE info (used by CreateDlDci)
                    // m_dlRBG is a vector of RBG indices
                    ueInfo->m_dlRBG.clear();
                    for (uint32_t rbg = startRbg; rbg < startRbg + numRbg; rbg++)
                    {
                        ueInfo->m_dlRBG.push_back(static_cast<uint16_t>(rbg));
                    }
                    
                    NS_LOG_DEBUG("  UE " << alloc.ueId << " (RNTI " << rnti << "): "
                                 << "PRBs [" << alloc.startPrb << "-" 
                                 << (alloc.startPrb + alloc.numPrbs - 1) << "] → "
                                 << "RBGs [" << startRbg << "-" << (startRbg + numRbg - 1) << "]");
                    
                    ueFound = true;
                    break;
                }
            }
            if (ueFound) break;
        }
        
        if (!ueFound)
        {
            NS_LOG_WARN("RNTI " << rnti << " not found in active UEs");
        }
    }
    
    // ========================================================================
    // Step 4: Return Symbol Allocation (OFDMA Style)
    // ========================================================================
    
    /*
     * In OFDMA, all UEs use all symbols.
     * They're separated in FREQUENCY (different RBGs), not TIME.
     * 
     * So we return: all symbols for the single beam.
     */
    
    BeamSymbolMap beamSymbols;
    for (const auto& [beamId, ueList] : activeDl)
    {
        if (!ueList.empty())
            beamSymbols[beamId] = symAvail;
    }
    std::cout << "[MILP-DBG] DL beamSymbols.size()=" << beamSymbols.size() << std::flush << std::endl;
    NS_LOG_DEBUG("Assigned " << symAvail << " symbols across " << beamSymbols.size() << " beams");
    return beamSymbols;
}

NrMacSchedulerNs3::BeamSymbolMap
NrMilpExecutorScheduler::AssignULRBG(uint32_t symAvail, 
                                     const ActiveUeMap& activeUl) const
{
    std::cout << "[MILP-DBG] >>> AssignULRBG ENTERED slot=" << m_currentSlot
              << " symAvail=" << symAvail
              << " activeUl.beams=" << activeUl.size() << std::flush << std::endl;

    NS_LOG_FUNCTION(this << symAvail << activeUl.size());
    
    /*
     * Same as DL, but for uplink.
     * 
     * The logic is identical - query MILP allocations and assign RBGs.
     */
    
    // Validate state
    if (!m_bwpManager || !m_initialized)
    {
        NS_LOG_WARN("BWP Manager not set or scheduler not initialized");
        BeamSymbolMap emptyMap;
        return emptyMap;
    }
    
    // Initialize RBG parameters if needed
    if (m_rbgSize == 0)
    {
        m_rbgSize = GetNumRbPerRbg();
        m_totalRbgs = GetBandwidthInRbg();
    }
    
    // Query MILP allocations
    auto milpAllocations = m_bwpManager->GetAllocationForSlot(m_currentSlot);
    
    NS_LOG_DEBUG("UL Slot " << m_currentSlot << ": Found " << milpAllocations.size() 
                 << " MILP allocations");
    
    if (milpAllocations.empty())
    {
        BeamSymbolMap emptyMap;
        return emptyMap;
    }
    
    // Assign RBGs to UEs
    for (const auto& alloc : milpAllocations)
    {
        auto rntiIt = m_ueIdToRnti.find(alloc.ueId);
        if (rntiIt == m_ueIdToRnti.end())
        {
            continue;
        }
        uint16_t rnti = rntiIt->second;
        
        bool ueFound = false;
        for (const auto& [beamId, ueList] : activeUl)
        {
            for (const auto& [ueInfo, bufferStatus] : ueList)
            {
                if (ueInfo->m_rnti == rnti)
                {
                    uint32_t startRbg = alloc.startPrb / m_rbgSize;
                    uint32_t numRbg = (alloc.numPrbs + m_rbgSize - 1) / m_rbgSize;
                    
                    // Store in UE info (m_ulRBG is a vector of RBG indices)
                    ueInfo->m_ulRBG.clear();
                    for (uint32_t rbg = startRbg; rbg < startRbg + numRbg; rbg++)
                    {
                        ueInfo->m_ulRBG.push_back(static_cast<uint16_t>(rbg));
                    }
                    
                    NS_LOG_DEBUG("  UL UE " << alloc.ueId << " (RNTI " << rnti << "): "
                                 << "RBGs [" << startRbg << "-" << (startRbg + numRbg - 1) << "]");
                    
                    ueFound = true;
                    break;
                }
            }
            if (ueFound) break;
        }
    }
    
    BeamSymbolMap beamSymbols;
    for (const auto& [beamId, ueList] : activeUl)
    {
        if (!ueList.empty())
            beamSymbols[beamId] = symAvail;
    }
    std::cout << "[MILP-DBG] UL beamSymbols.size()=" << beamSymbols.size() << std::flush << std::endl;
    
    return beamSymbols;
}

// ============================================================================
// BEAM SWITCHING (No-Op for Single Beam)
// ============================================================================

void
NrMilpExecutorScheduler::ChangeDlBeam(PointInFTPlane* spoint, 
                                      uint32_t symOfBeam) const
{
    NS_LOG_FUNCTION(this << spoint << symOfBeam);
    
    /*
     * Beam switching is not used in single-beam scenario.
     * This is a required override but does nothing.
     */
    
    // No-op
}

void
NrMilpExecutorScheduler::ChangeUlBeam(PointInFTPlane* spoint, 
                                      uint32_t symOfBeam) const
{
    NS_LOG_FUNCTION(this << spoint << symOfBeam);
    
    // No-op (single beam)
}

// ============================================================================
// DCI CREATION (Convert MILP Allocation to ns-3 Format)
// ============================================================================

std::shared_ptr<DciInfoElementTdma>
NrMilpExecutorScheduler::CreateDlDci(
    PointInFTPlane* spoint,
    const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
    uint32_t maxSym) const
{
    NS_LOG_FUNCTION(this << spoint << ueInfo << maxSym);
    
    /*
     * ========================================================================
     * CRITICAL METHOD: Create DCI from MILP Allocation
     * ========================================================================
     * 
     * DCI = Downlink Control Information
     * This is the message that tells the PHY layer HOW to transmit to this UE.
     * 
     * Steps:
     * 1. Map RNTI → ueId
     * 2. Query MILP allocation for this UE at current slot
     * 3. If no allocation, return nullptr (UE not scheduled)
     * 4. Convert PRB → RBG bitmask
     * 5. Assign symbols (all symbols - OFDMA)
     * 6. Set MCS (from UE info)
     * 7. Calculate TBS (Transport Block Size)
     * 8. Return DCI
     */
    
    // ========================================================================
    // Step 1: Map RNTI → ueId
    // ========================================================================
    
    uint16_t rnti = ueInfo->m_rnti;
    
    auto ueIdIt = m_rntiToUeId.find(rnti);
    if (ueIdIt == m_rntiToUeId.end())
    {
        NS_LOG_WARN("RNTI " << rnti << " not found in ueId mapping");
        return nullptr;
    }
    
    uint32_t ueId = ueIdIt->second;
    
    // ========================================================================
    // Step 2: Query MILP Allocation
    // ========================================================================
    
    auto alloc = m_bwpManager->GetUeAllocationForSlot(m_currentSlot, ueId);
    
    if (!alloc.has_value())
    {
        // No allocation for this UE in this slot
        NS_LOG_DEBUG("No MILP allocation for UE " << ueId << " at slot " << m_currentSlot);
        return nullptr;
    }
    
    NS_LOG_DEBUG("Creating DL DCI for UE " << ueId << " (RNTI " << rnti << ")");
    
    // ========================================================================
    // Step 3: Convert PRB → RBG Bitmask
    // ========================================================================
    
    /*
     * MILP allocation: PRBs [startPrb, startPrb + numPrbs - 1]
     * ns-3 needs: RBG bitmask (vector of bool)
     */
    
    std::vector<bool> rbgBitmask = ConvertPrbToRbgBitmask(
        alloc->startPrb,
        alloc->numPrbs,
        m_rbgSize,
        m_totalRbgs
    );
    
    // ========================================================================
    // Step 4: Create DCI Using Constructor
    // ========================================================================
    
    /*
     * DciInfoElementTdma must be constructed with all parameters.
     * Fields are const/read-only after construction.
     * 
     * Constructor signature:
     * DciInfoElementTdma(symStart, numSym, format, varTtiType, rbgBitmask)
     * 
     * Enum values (defined INSIDE DciInfoElementTdma struct):
     * - DciFormat: DL=0, UL=1
     * - VarTtiType: SRS=0, DATA=1, CTRL=2, MSG3=3
     * 
     * We use DATA for normal data transmission.
     */
    
    auto dci = std::make_shared<DciInfoElementTdma>(
        spoint->m_sym,                          // symStart
        maxSym,                                 // numSym (all symbols - OFDMA)
        DciInfoElementTdma::DL,                 // DciFormat::DL (qualified)
        DciInfoElementTdma::DATA,               // VarTtiType::DATA (qualified)
        rbgBitmask                              // RBG bitmask
    );
    
    // ========================================================================
    // Step 5: Set Additional DCI Fields
    // ========================================================================
    
    // RNTI (which UE)
    const_cast<uint16_t&>(dci->m_rnti) = rnti;
    
    // MCS (modulation and coding scheme)
    const_cast<uint8_t&>(dci->m_mcs) = ueInfo->m_dlMcs;
    
    // ========================================================================
    // Step 6: Calculate TBS (Transport Block Size)
    // ========================================================================
    
    /*
     * TBS = how many bytes can be transmitted
     * 
     * CalculateTbSize(mcs, rank, nprb)
     * - mcs: Modulation/Coding Scheme
     * - rank: MIMO rank (1 for SISO, 2 for 2x2 MIMO, etc.)
     * - nprb: Number of PRBs
     * 
     * m_tbSize is also const, so we need const_cast.
     */
    
    uint32_t numPrbs = alloc->numPrbs;
    uint8_t rank = 1;  // SISO (single antenna)
    
    uint32_t tbSize = m_dlAmc->CalculateTbSize(dci->m_mcs, rank, numPrbs);
    const_cast<uint32_t&>(dci->m_tbSize) = tbSize;
    
    NS_LOG_DEBUG("  DCI: RBGs=" << (alloc->numPrbs / m_rbgSize) 
                 << ", Symbols=[" << dci->m_symStart << "-" 
                 << (dci->m_symStart + dci->m_numSym - 1) << "]"
                 << ", MCS=" << static_cast<uint32_t>(dci->m_mcs)
                 << ", TBS=" << dci->m_tbSize << " bytes");
    
    // ========================================================================
    // Step 8: Update Starting Point (for next UE in TDMA flow)
    // ========================================================================
    
    /*
     * Even though we're doing OFDMA (all UEs use all symbols),
     * the base class expects us to update the starting point.
     * 
     * In OFDMA, we move in FREQUENCY, not TIME.
     */
    
    // Move cursor in frequency (RBG dimension)
    spoint->m_rbg += (alloc->numPrbs / m_rbgSize);
    
    return dci;
}

std::shared_ptr<DciInfoElementTdma>
NrMilpExecutorScheduler::CreateUlDci(
    PointInFTPlane* spoint,
    const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
    uint32_t maxSym) const
{
    NS_LOG_FUNCTION(this << spoint << ueInfo << maxSym);
    
    /*
     * Same as DL DCI creation, but for uplink.
     * Logic is identical.
     */
    
    // Map RNTI → ueId
    uint16_t rnti = ueInfo->m_rnti;
    auto ueIdIt = m_rntiToUeId.find(rnti);
    if (ueIdIt == m_rntiToUeId.end())
    {
        return nullptr;
    }
    uint32_t ueId = ueIdIt->second;
    
    // Query MILP allocation
    auto alloc = m_bwpManager->GetUeAllocationForSlot(m_currentSlot, ueId);
    if (!alloc.has_value())
    {
        return nullptr;
    }
    
    NS_LOG_DEBUG("Creating UL DCI for UE " << ueId << " (RNTI " << rnti << ")");
    
    // Convert PRB → RBG bitmask
    std::vector<bool> rbgBitmask = ConvertPrbToRbgBitmask(
        alloc->startPrb,
        alloc->numPrbs,
        m_rbgSize,
        m_totalRbgs
    );
    
    // Create DCI using constructor (properly qualified enum values)
    auto dci = std::make_shared<DciInfoElementTdma>(
        spoint->m_sym,                          // symStart
        maxSym,                                 // numSym
        DciInfoElementTdma::UL,                 // DciFormat::UL (qualified)
        DciInfoElementTdma::DATA,               // VarTtiType::DATA (qualified)
        rbgBitmask                              // RBG bitmask
    );
    
    // Set RNTI
    const_cast<uint16_t&>(dci->m_rnti) = rnti;
    
    // Set MCS
    const_cast<uint8_t&>(dci->m_mcs) = ueInfo->m_ulMcs;
    
    // Calculate TBS (m_tbSize is also const)
    uint32_t numPrbs = alloc->numPrbs;
    uint8_t rank = 1;  // SISO
    
    uint32_t tbSize = m_ulAmc->CalculateTbSize(dci->m_mcs, rank, numPrbs);
    const_cast<uint32_t&>(dci->m_tbSize) = tbSize;
    
    NS_LOG_DEBUG("  UL DCI: RBGs=" << (alloc->numPrbs / m_rbgSize)
                 << ", Symbols=[" << dci->m_symStart << "-"
                 << (dci->m_symStart + dci->m_numSym - 1) << "]"
                 << ", MCS=" << static_cast<uint32_t>(dci->m_mcs)
                 << ", TBS=" << dci->m_tbSize << " bytes");
    
    // Update cursor
    spoint->m_rbg += (alloc->numPrbs / m_rbgSize);
    
    return dci;
}

// ============================================================================
// UE REPRESENTATION
// ============================================================================

std::shared_ptr<NrMacSchedulerUeInfo>
NrMilpExecutorScheduler::CreateUeRepresentation(
    const NrMacCschedSapProvider::CschedUeConfigReqParameters& params) const
{
    NS_LOG_FUNCTION(this);
    
    /*
     * Create UE representation for scheduler.
     * We use the Proportional Fair version (same as TDMA-PF scheduler).
     * 
     * Constructor parameters:
     * 1. alpha: PF fairness coefficient (1.0 = standard PF)
     * 2. rnti: UE identifier
     * 3. beamId: Beam identifier
     * 4. fn: Function to get RBs per RBG (must use std::bind for member function)
     */
    
    return std::make_shared<NrMacSchedulerUeInfoPF>(
        1.0,                                    // alpha = 1.0 (standard PF)
        params.m_rnti,                          // RNTI
        params.m_beamId,                        // Beam ID
        std::bind(&NrMacSchedulerNs3::GetNumRbPerRbg,
                  this)                         // Bind member function to this
    );
}

// ============================================================================
// TRANSMIT POWER CONTROL
// ============================================================================

uint8_t
NrMilpExecutorScheduler::GetTpc() const
{
    /*
     * TPC = Transmit Power Control
     * 
     * MILP doesn't optimize transmit power, so we use fixed value.
     * TPC=1 means "no change" in power.
     */
    
    return 1;  // No power adjustment
}

// ============================================================================
// TDMA PURE VIRTUAL METHODS (No-ops for MILP Executor)
// ============================================================================

std::function<bool(const NrMacSchedulerNs3::UePtrAndBufferReq&,
                    const NrMacSchedulerNs3::UePtrAndBufferReq&)>
NrMilpExecutorScheduler::GetUeCompareDlFn() const
{
    /*
     * MILP executor doesn't need to sort UEs.
     * Allocation order is pre-determined by MILP solution.
     * Return a dummy comparison function.
     */
    return [](const UePtrAndBufferReq& /*lhs*/, const UePtrAndBufferReq& /*rhs*/) -> bool {
        return false;  // All UEs are equal priority
    };
}

std::function<bool(const NrMacSchedulerNs3::UePtrAndBufferReq&,
                    const NrMacSchedulerNs3::UePtrAndBufferReq&)>
NrMilpExecutorScheduler::GetUeCompareUlFn() const
{
    /*
     * MILP executor doesn't need to sort UEs.
     * Allocation order is pre-determined by MILP solution.
     * Return a dummy comparison function.
     */
    return [](const UePtrAndBufferReq& /*lhs*/, const UePtrAndBufferReq& /*rhs*/) -> bool {
        return false;  // All UEs are equal priority
    };
}

void
NrMilpExecutorScheduler::AssignedDlResources(
    const UePtrAndBufferReq& /*ue*/,
    const FTResources& /*assigned*/,
    const FTResources& /*totAssigned*/) const
{
    // No-op: MILP executor doesn't need post-assignment processing
}

void
NrMilpExecutorScheduler::AssignedUlResources(
    const UePtrAndBufferReq& /*ue*/,
    const FTResources& /*assigned*/,
    const FTResources& /*totAssigned*/) const
{
    // No-op: MILP executor doesn't need post-assignment processing
}

void
NrMilpExecutorScheduler::NotAssignedDlResources(
    const UePtrAndBufferReq& /*ue*/,
    const FTResources& /*notAssigned*/,
    const FTResources& /*totAssigned*/) const
{
    // No-op: MILP executor doesn't need to handle unassigned resources
}

void
NrMilpExecutorScheduler::NotAssignedUlResources(
    const UePtrAndBufferReq& /*ue*/,
    const FTResources& /*notAssigned*/,
    const FTResources& /*totAssigned*/) const
{
    // No-op: MILP executor doesn't need to handle unassigned resources
}

void
NrMilpExecutorScheduler::BeforeDlSched(
    const UePtrAndBufferReq& /*ue*/,
    const FTResources& /*assignableInIteration*/) const
{
    // No-op: MILP executor doesn't need pre-scheduling operations
}

void
NrMilpExecutorScheduler::BeforeUlSched(
    const UePtrAndBufferReq& /*ue*/,
    const FTResources& /*assignableInIteration*/) const
{
    // No-op: MILP executor doesn't need pre-scheduling operations
}

// ============================================================================
// HELPER METHODS
// ============================================================================

std::vector<bool>
NrMilpExecutorScheduler::ConvertPrbToRbgBitmask(uint32_t startPrb,
                                                 uint32_t numPrbs,
                                                 uint32_t rbgSize,
                                                 uint32_t totalRbgs) const
{
    NS_LOG_FUNCTION(this << startPrb << numPrbs << rbgSize << totalRbgs);
    
    /*
     * Convert PRB allocation to RBG bitmask.
     * 
     * Input:
     * - startPrb: Starting PRB index (e.g., 0)
     * - numPrbs: Number of contiguous PRBs (e.g., 10)
     * - rbgSize: PRBs per RBG (e.g., 4)
     * - totalRbgs: Total RBGs in bandwidth (e.g., 68)
     * 
     * Output:
     * - Bitmask vector where bitmask[i] = true if RBG i is allocated
     * 
     * Algorithm:
     * - startRbg = floor(startPrb / rbgSize)
     * - endPrb = startPrb + numPrbs - 1
     * - endRbg = floor(endPrb / rbgSize)
     * - Set bitmask[startRbg..endRbg] = true
     */
    
    // Initialize bitmask (all false)
    std::vector<bool> bitmask(totalRbgs, false);
    
    // Calculate RBG range
    uint32_t startRbg = startPrb / rbgSize;
    uint32_t endPrb = startPrb + numPrbs - 1;
    uint32_t endRbg = endPrb / rbgSize;
    
    // Sanity check
    if (endRbg >= totalRbgs)
    {
        NS_LOG_WARN("RBG allocation exceeds bandwidth: endRbg=" << endRbg 
                    << ", totalRbgs=" << totalRbgs);
        endRbg = totalRbgs - 1;
    }
    
    // Set allocated RBGs
    for (uint32_t rbg = startRbg; rbg <= endRbg; rbg++)
    {
        bitmask[rbg] = true;
    }
    
    NS_LOG_DEBUG("PRBs [" << startPrb << "-" << endPrb << "] → "
                 << "RBGs [" << startRbg << "-" << endRbg << "]");
    
    return bitmask;
}

uint32_t
NrMilpExecutorScheduler::GetCurrentSlot() const
{
    return m_currentSlot;
}

} // namespace ns3