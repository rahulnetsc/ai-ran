/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * MILP Interface - Header File
 * 
 * Responsibilities:
 * - Connect to external Python MILP solver via TCP socket
 * - Serialize MilpProblem to JSON format
 * - Send JSON problem via socket
 * - Receive JSON solution via socket
 * - Deserialize JSON to MilpSolution
 * - Handle connection errors, timeouts, and retries
 * 
 * Architecture:
 * This class acts as a bridge between ns-3 (C++) and external MILP solver (Python).
 * It uses TCP sockets for communication and JSON for data exchange.
 * 
 * Communication Protocol:
 * 1. ns-3 opens TCP connection to Python solver (e.g., localhost:8888)
 * 2. ns-3 sends JSON problem (MilpProblem → JSON string)
 * 3. ns-3 waits for response (blocking or with timeout)
 * 4. Python solver sends JSON solution
 * 5. ns-3 parses JSON and returns MilpSolution
 * 6. Connection closed (or kept alive for multiple problems)
 * 
 * JSON Format (Example):
 * 
 * Request (MilpProblem):
 * {
 *   "numUEs": 3,
 *   "bandwidth": 100000000.0,
 *   "totalBandwidthPrbs": 273,
 *   "timeWindow": 1.0,
 *   "numerology": 1,
 *   "slotDuration": 0.0005,
 *   "totalSlots": 2000,
 *   "ues": [
 *     {"ueId": 0, "sliceType": "eMBB", "throughputMbps": 50.0, ...},
 *     ...
 *   ]
 * }
 * 
 * Response (MilpSolution):
 * {
 *   "status": "optimal",
 *   "objectiveValue": 150.5,
 *   "solveTimeSeconds": 12.34,
 *   "allocations": [
 *     {"ueId": 0, "slotId": 0, "startPrb": 0, "numPrbs": 10},
 *     ...
 *   ],
 *   "summary": {
 *     "0": {"totalPrbsAllocated": 1000, "expectedThroughputMbps": 51.2, ...},
 *     ...
 *   }
 * }
 */

#ifndef NR_MILP_INTERFACE_H
#define NR_MILP_INTERFACE_H

#include "ns3/object.h"
#include "utils/nr-milp-types.h"

#include <string>
#include <cstdint>

namespace ns3
{

/**
 * \ingroup nr-modular
 * 
 * \brief Interface to External MILP Solver via TCP Socket
 * 
 * This class provides communication with an external MILP solver (Python)
 * running as a separate process. It handles:
 * - TCP socket connection management
 * - JSON serialization/deserialization
 * - Error handling and timeouts
 * - Connection retry logic
 * 
 * Usage Example:
 * \code
 *   // 1. Create interface
 *   Ptr<NrMilpInterface> milpInterface = CreateObject<NrMilpInterface>();
 *   
 *   // 2. Configure solver connection
 *   milpInterface->SetSolverAddress("localhost");
 *   milpInterface->SetSolverPort(8888);
 *   milpInterface->SetConnectionTimeout(10.0);  // 10 seconds
 *   milpInterface->SetSolveTimeout(60.0);       // 60 seconds
 *   
 *   // 3. Connect to solver
 *   if (!milpInterface->Connect()) {
 *       NS_FATAL_ERROR("Failed to connect to MILP solver");
 *   }
 *   
 *   // 4. Build problem
 *   MilpProblem problem;
 *   problem.numUEs = 3;
 *   problem.bandwidth = 100e6;
 *   // ... set other fields ...
 *   
 *   // 5. Solve (blocking call)
 *   MilpSolution solution = milpInterface->SolveProblem(problem);
 *   
 *   // 6. Check result
 *   if (solution.IsOptimal()) {
 *       std::cout << "Optimal solution found!" << std::endl;
 *   }
 *   
 *   // 7. Disconnect (optional, done automatically in destructor)
 *   milpInterface->Disconnect();
 * \endcode
 */
class NrMilpInterface : public Object
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
    NrMilpInterface();
    
    /**
     * \brief Destructor
     */
    virtual ~NrMilpInterface();
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * \brief Set solver IP address or hostname
     * \param address IP address (e.g., "127.0.0.1", "localhost", "192.168.1.100")
     * 
     * Default: "localhost"
     */
    void SetSolverAddress(const std::string& address);
    
    /**
     * \brief Set solver TCP port
     * \param port Port number (e.g., 8888)
     * 
     * Default: 8888
     */
    void SetSolverPort(uint16_t port);
    
    /**
     * \brief Set connection timeout
     * \param timeout Timeout in seconds
     * 
     * How long to wait for initial connection to solver.
     * Default: 10.0 seconds
     */
    void SetConnectionTimeout(double timeout);
    
    /**
     * \brief Set solve timeout
     * \param timeout Timeout in seconds
     * 
     * How long to wait for MILP solver to return solution.
     * If solver exceeds this time, returns timeout error.
     * Default: 60.0 seconds
     */
    void SetSolveTimeout(double timeout);
    
    /**
     * \brief Enable/disable automatic reconnection
     * \param enable If true, automatically retry connection on failure
     * 
     * If enabled and connection fails, will retry up to maxRetries times.
     * Default: true
     */
    void SetAutoReconnect(bool enable);
    
    /**
     * \brief Set maximum connection retry attempts
     * \param maxRetries Maximum number of retries (0 = no retries)
     * 
     * Only used if auto-reconnect is enabled.
     * Default: 3
     */
    void SetMaxRetries(uint32_t maxRetries);
    
    /**
     * \brief Get solver address
     * \return Configured solver address
     */
    std::string GetSolverAddress() const;
    
    /**
     * \brief Get solver port
     * \return Configured solver port
     */
    uint16_t GetSolverPort() const;
    
    // ========================================================================
    // CONNECTION MANAGEMENT
    // ========================================================================
    
    /**
     * \brief Connect to MILP solver
     * \return true if connection successful, false otherwise
     * 
     * Opens TCP socket connection to solver.
     * Blocks until connection succeeds or timeout.
     * 
     * This is called automatically by SolveProblem() if not connected.
     * Can be called manually to test connection before solving.
     * 
     * Example:
     * \code
     *   if (!milpInterface->Connect()) {
     *       NS_LOG_ERROR("Solver not available, check if Python script is running");
     *       // Fall back to heuristic scheduler
     *   }
     * \endcode
     */
    bool Connect();
    
    /**
     * \brief Disconnect from MILP solver
     * 
     * Closes TCP socket.
     * Called automatically in destructor.
     */
    void Disconnect();
    
    /**
     * \brief Check if connected to solver
     * \return true if socket is connected, false otherwise
     */
    bool IsConnected() const;
    
    /**
     * \brief Test connection by sending ping message
     * \return true if solver responds to ping, false otherwise
     * 
     * Sends a simple test message to verify solver is responsive.
     * Useful for health checking before submitting heavy MILP problem.
     * 
     * Example:
     * \code
     *   if (milpInterface->IsConnected() && milpInterface->Ping()) {
     *       // Solver is ready
     *   } else {
     *       // Solver not responding, reconnect
     *       milpInterface->Connect();
     *   }
     * \endcode
     */
    bool Ping();
    
    // ========================================================================
    // MILP SOLVING (Primary Interface)
    // ========================================================================
    
    /**
     * \brief Solve MILP problem (blocking call)
     * \param problem The MILP problem to solve
     * \return MILP solution (check status field for success/failure)
     * 
     * This is the PRIMARY method for solving MILP problems.
     * 
     * Workflow:
     * 1. Validates problem structure
     * 2. Connects to solver if not connected
     * 3. Serializes problem to JSON
     * 4. Sends JSON via socket
     * 5. Waits for response (blocks up to solveTimeout)
     * 6. Deserializes JSON to MilpSolution
     * 7. Returns solution
     * 
     * Return value:
     * - solution.status == "optimal" → Success
     * - solution.status == "infeasible" → No solution exists
     * - solution.status == "timeout" → Solver exceeded time limit
     * - solution.status == "error" → Communication or parsing error
     * 
     * Example:
     * \code
     *   MilpSolution sol = milpInterface->SolveProblem(problem);
     *   
     *   if (sol.status == "optimal") {
     *       bwpManager->LoadMilpSolution(sol);
     *   } else if (sol.status == "infeasible") {
     *       NS_LOG_WARN("SLAs too strict, cannot be satisfied");
     *       // Relax constraints or use heuristic
     *   } else if (sol.status == "timeout") {
     *       NS_LOG_WARN("Solver timeout, using best feasible solution");
     *       // Check if partial solution is available
     *   } else {
     *       NS_FATAL_ERROR("MILP solver error: " << sol.status);
     *   }
     * \endcode
     */
    MilpSolution SolveProblem(const MilpProblem& problem);
    
    /**
     * \brief Solve problem with custom timeout
     * \param problem The MILP problem to solve
     * \param customTimeout Timeout in seconds (overrides default)
     * \return MILP solution
     * 
     * Same as SolveProblem() but with custom timeout for this call only.
     * Useful for adaptive timeout based on problem size.
     */
    MilpSolution SolveProblem(const MilpProblem& problem, double customTimeout);
    
    // ========================================================================
    // JSON SERIALIZATION (Public for Testing)
    // ========================================================================
    
    /**
     * \brief Serialize MILP problem to JSON string
     * \param problem The MILP problem
     * \return JSON string representation
     * 
     * Made public for unit testing and debugging.
     * Normally called internally by SolveProblem().
     * 
     * Example output:
     * \code
     *   std::string json = milpInterface->SerializeProblem(problem);
     *   std::cout << "Problem JSON:\n" << json << std::endl;
     *   // Can save to file for offline testing
     * \endcode
     */
    std::string SerializeProblem(const MilpProblem& problem);
    
    /**
     * \brief Deserialize JSON string to MILP solution
     * \param json JSON string from solver
     * \return Parsed MILP solution
     * 
     * Made public for unit testing and debugging.
     * Normally called internally by SolveProblem().
     * 
     * Example:
     * \code
     *   std::string json = ReadFromFile("solution.json");
     *   MilpSolution sol = milpInterface->DeserializeSolution(json);
     * \endcode
     */
    MilpSolution DeserializeSolution(const std::string& json);
    
    // ========================================================================
    // STATISTICS AND DIAGNOSTICS
    // ========================================================================
    
    /**
     * \brief Communication statistics structure
     * 
     * Tracks socket communication performance and errors.
     */
    struct Statistics
    {
        uint32_t totalProblemsSubmitted;    ///< Total MILP problems sent
        uint32_t totalSolutionsReceived;    ///< Total solutions received
        uint32_t totalErrors;               ///< Total errors encountered
        uint32_t totalTimeouts;             ///< Total timeout events
        uint32_t totalReconnections;        ///< Total reconnection attempts
        
        double totalSolveTime;              ///< Sum of all solve times (seconds)
        double avgSolveTime;                ///< Average solve time (seconds)
        double maxSolveTime;                ///< Maximum solve time (seconds)
        double minSolveTime;                ///< Minimum solve time (seconds)
        
        uint64_t totalBytesSent;            ///< Total bytes sent
        uint64_t totalBytesReceived;        ///< Total bytes received
        
        /**
         * \brief Print statistics to output stream
         * \param os Output stream
         */
        void Print(std::ostream& os) const;
    };
    
    /**
     * \brief Get communication statistics
     * \return Statistics structure
     * 
     * Useful for performance analysis and debugging.
     */
    Statistics GetStatistics() const;
    
    /**
     * \brief Reset statistics counters
     * 
     * Resets all statistics to zero.
     * Useful for measuring performance of specific test runs.
     */
    void ResetStatistics();
    
    /**
     * \brief Print connection and solver information
     * \param os Output stream
     * 
     * Prints current configuration and connection status.
     */
    void PrintInfo(std::ostream& os) const;
    
  protected:
    /**
     * \brief Destructor implementation
     */
    void DoDispose() override;
    
  private:
    // ========================================================================
    // PRIVATE SOCKET COMMUNICATION METHODS
    // ========================================================================
    
    /**
     * \brief Send data via socket
     * \param data Data to send
     * \return true if send successful, false otherwise
     * 
     * Handles partial writes and errors.
     */
    bool SendData(const std::string& data);
    
    /**
     * \brief Receive data from socket
     * \param data Output buffer for received data
     * \param timeout Timeout in seconds
     * \return true if receive successful, false on timeout or error
     * 
     * Blocks until:
     * - Complete message received (detects end-of-message marker)
     * - Timeout occurs
     * - Error occurs
     */
    bool ReceiveData(std::string& data, double timeout);
    
    /**
     * \brief Wait for socket to be ready for reading
     * \param timeout Timeout in seconds
     * \return true if data available, false on timeout
     * 
     * Uses select() or poll() for timeout handling.
     */
    bool WaitForData(double timeout);
    
    /**
     * \brief Close socket connection
     */
    void CloseSocket();
    
    // ========================================================================
    // PRIVATE HELPER METHODS
    // ========================================================================
    
    /**
     * \brief Validate MILP problem before sending
     * \param problem Problem to validate
     * \return true if valid, false otherwise
     * 
     * Checks problem structure and constraints.
     */
    bool ValidateProblem(const MilpProblem& problem) const;
    
    /**
     * \brief Update statistics after solve
     * \param success Whether solve was successful
     * \param solveTime Time taken to solve (seconds)
     * \param bytesSent Bytes sent
     * \param bytesReceived Bytes received
     */
    void UpdateStatistics(bool success, double solveTime, 
                          uint64_t bytesSent, uint64_t bytesReceived);
    
    // ========================================================================
    // MEMBER VARIABLES - CONFIGURATION
    // ========================================================================
    
    std::string m_solverAddress;        ///< Solver IP address/hostname
    uint16_t m_solverPort;              ///< Solver TCP port
    double m_connectionTimeout;         ///< Connection timeout (seconds)
    double m_solveTimeout;              ///< Solve timeout (seconds)
    bool m_autoReconnect;               ///< Auto-reconnect on failure
    uint32_t m_maxRetries;              ///< Max connection retry attempts
    
    // ========================================================================
    // MEMBER VARIABLES - CONNECTION STATE
    // ========================================================================
    
    int m_socketFd;                     ///< Socket file descriptor (-1 if not connected)
    bool m_isConnected;                 ///< Connection status flag
    uint32_t m_reconnectAttempts;       ///< Current reconnection attempt counter
    
    // ========================================================================
    // MEMBER VARIABLES - STATISTICS
    // ========================================================================
    
    Statistics m_statistics;            ///< Communication statistics
};

} // namespace ns3

#endif /* NR_MILP_INTERFACE_H */
