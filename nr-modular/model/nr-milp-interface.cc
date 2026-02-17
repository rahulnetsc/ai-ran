/*
 * Copyright (c) 2026 ARTPARK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * MILP Interface - Implementation File
 */

#include "nr-milp-interface.h"

#include "ns3/log.h"
#include "ns3/abort.h"

#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrMilpInterface");
NS_OBJECT_ENSURE_REGISTERED(NrMilpInterface);

// ============================================================================
// TYPE ID
// ============================================================================

TypeId
NrMilpInterface::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrMilpInterface")
                            .SetParent<Object>()
                            .SetGroupName("NrModular")
                            .AddConstructor<NrMilpInterface>();
    return tid;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

NrMilpInterface::NrMilpInterface()
    : m_solverAddress("localhost"),
      m_solverPort(8888),
      m_connectionTimeout(10.0),
      m_solveTimeout(60.0),
      m_autoReconnect(true),
      m_maxRetries(3),
      m_socketFd(-1),
      m_isConnected(false),
      m_reconnectAttempts(0),
      m_statistics()
{
    NS_LOG_FUNCTION(this);
}

NrMilpInterface::~NrMilpInterface()
{
    NS_LOG_FUNCTION(this);
    Disconnect();
}

void
NrMilpInterface::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Disconnect();
    Object::DoDispose();
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void
NrMilpInterface::SetSolverAddress(const std::string& address)
{
    NS_LOG_FUNCTION(this << address);
    m_solverAddress = address;
}

void
NrMilpInterface::SetSolverPort(uint16_t port)
{
    NS_LOG_FUNCTION(this << port);
    m_solverPort = port;
}

void
NrMilpInterface::SetConnectionTimeout(double timeout)
{
    NS_LOG_FUNCTION(this << timeout);
    m_connectionTimeout = timeout;
}

void
NrMilpInterface::SetSolveTimeout(double timeout)
{
    NS_LOG_FUNCTION(this << timeout);
    m_solveTimeout = timeout;
}

void
NrMilpInterface::SetAutoReconnect(bool enable)
{
    NS_LOG_FUNCTION(this << enable);
    m_autoReconnect = enable;
}

void
NrMilpInterface::SetMaxRetries(uint32_t maxRetries)
{
    NS_LOG_FUNCTION(this << maxRetries);
    m_maxRetries = maxRetries;
}

std::string
NrMilpInterface::GetSolverAddress() const
{
    return m_solverAddress;
}

uint16_t
NrMilpInterface::GetSolverPort() const
{
    return m_solverPort;
}

// ============================================================================
// CONNECTION MANAGEMENT
// ============================================================================

bool
NrMilpInterface::Connect()
{
    NS_LOG_FUNCTION(this);
    
    if (m_isConnected)
    {
        NS_LOG_INFO("Already connected to solver");
        return true;
    }
    
    NS_LOG_INFO("Connecting to MILP solver at " << m_solverAddress 
                << ":" << m_solverPort);
    
    uint32_t attempts = 0;
    bool connected = false;
    
    do
    {
        // Create socket
        m_socketFd = socket(AF_INET, SOCK_STREAM, 0);
        if (m_socketFd < 0)
        {
            NS_LOG_ERROR("Failed to create socket: " << strerror(errno));
            attempts++;
            sleep(1);
            continue;
        }
        
        // Set socket timeout for connect
        struct timeval tv;
        tv.tv_sec = static_cast<long>(m_connectionTimeout);
        tv.tv_usec = static_cast<long>((m_connectionTimeout - tv.tv_sec) * 1e6);
        setsockopt(m_socketFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(m_socketFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        
        // Resolve hostname
        struct hostent* server = gethostbyname(m_solverAddress.c_str());
        if (server == nullptr)
        {
            NS_LOG_ERROR("Cannot resolve hostname: " << m_solverAddress);
            close(m_socketFd);
            m_socketFd = -1;
            attempts++;
            sleep(1);
            continue;
        }
        
        // Setup server address
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
        serverAddr.sin_port = htons(m_solverPort);
        
        // Connect
        if (connect(m_socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
        {
            NS_LOG_WARN("Connection attempt " << (attempts + 1) 
                        << " failed: " << strerror(errno));
            close(m_socketFd);
            m_socketFd = -1;
            attempts++;
            
            if (m_autoReconnect && attempts <= m_maxRetries)
            {
                NS_LOG_INFO("Retrying in 1 second...");
                sleep(1);
            }
        }
        else
        {
            connected = true;
            m_isConnected = true;
            m_reconnectAttempts = 0;
            NS_LOG_INFO("Successfully connected to MILP solver");
        }
        
    } while (!connected && m_autoReconnect && attempts <= m_maxRetries);
    
    if (!connected)
    {
        NS_LOG_ERROR("Failed to connect to MILP solver after " 
                     << attempts << " attempts");
        m_statistics.totalReconnections += attempts;
        return false;
    }
    
    m_statistics.totalReconnections += attempts;
    return true;
}

void
NrMilpInterface::Disconnect()
{
    NS_LOG_FUNCTION(this);
    
    if (m_isConnected)
    {
        CloseSocket();
        NS_LOG_INFO("Disconnected from MILP solver");
    }
}

bool
NrMilpInterface::IsConnected() const
{
    return m_isConnected;
}

bool
NrMilpInterface::Ping()
{
    NS_LOG_FUNCTION(this);
    
    if (!m_isConnected)
    {
        NS_LOG_WARN("Not connected to solver");
        return false;
    }
    
    try
    {
        // Send ping message
        json pingMsg;
        pingMsg["type"] = "ping";
        std::string pingStr = pingMsg.dump();
        
        if (!SendData(pingStr))
        {
            NS_LOG_ERROR("Failed to send ping");
            return false;
        }
        
        // Wait for pong
        std::string response;
        if (!ReceiveData(response, 5.0))  // 5 second timeout
        {
            NS_LOG_ERROR("Ping timeout");
            return false;
        }
        
        // Parse response
        json responseJson = json::parse(response);
        if (responseJson["type"] == "pong")
        {
            NS_LOG_DEBUG("Ping successful");
            return true;
        }
    }
    catch (const std::exception& e)
    {
        NS_LOG_ERROR("Ping failed: " << e.what());
    }
    
    return false;
}

// ============================================================================
// MILP SOLVING
// ============================================================================

MilpSolution
NrMilpInterface::SolveProblem(const MilpProblem& problem)
{
    return SolveProblem(problem, m_solveTimeout);
}

MilpSolution
NrMilpInterface::SolveProblem(const MilpProblem& problem, double customTimeout)
{
    NS_LOG_FUNCTION(this);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Validate problem
    if (!ValidateProblem(problem))
    {
        NS_LOG_ERROR("Invalid MILP problem");
        MilpSolution errorSol;
        errorSol.status = "error";
        m_statistics.totalErrors++;
        return errorSol;
    }
    
    // Connect if not connected
    if (!m_isConnected)
    {
        if (!Connect())
        {
            NS_LOG_ERROR("Cannot connect to solver");
            MilpSolution errorSol;
            errorSol.status = "error";
            m_statistics.totalErrors++;
            return errorSol;
        }
    }
    
    // Serialize problem to JSON
    std::string jsonRequest;
    try
    {
        jsonRequest = SerializeProblem(problem);
        NS_LOG_INFO("Serialized problem: " << jsonRequest.length() << " bytes");
    }
    catch (const std::exception& e)
    {
        NS_LOG_ERROR("Failed to serialize problem: " << e.what());
        MilpSolution errorSol;
        errorSol.status = "error";
        m_statistics.totalErrors++;
        return errorSol;
    }
    
    // Send problem
    if (!SendData(jsonRequest))
    {
        NS_LOG_ERROR("Failed to send problem to solver");
        MilpSolution errorSol;
        errorSol.status = "error";
        m_statistics.totalErrors++;
        return errorSol;
    }
    
    NS_LOG_INFO("Problem sent to solver, waiting for solution (timeout: " 
                << customTimeout << "s)...");
    
    m_statistics.totalProblemsSubmitted++;
    
    // Receive solution
    std::string jsonResponse;
    if (!ReceiveData(jsonResponse, customTimeout))
    {
        NS_LOG_ERROR("Timeout or error receiving solution");
        MilpSolution timeoutSol;
        timeoutSol.status = "timeout";
        m_statistics.totalTimeouts++;
        return timeoutSol;
    }
    
    NS_LOG_INFO("Received solution: " << jsonResponse.length() << " bytes");
    
    // Deserialize solution
    MilpSolution solution;
    try
    {
        solution = DeserializeSolution(jsonResponse);
        NS_LOG_INFO("Solution status: " << solution.status);
        NS_LOG_INFO("Objective value: " << solution.objectiveValue);
        NS_LOG_INFO("Solver time: " << solution.solveTimeSeconds << " seconds");
        NS_LOG_INFO("Allocations: " << solution.allocations.size());
    }
    catch (const std::exception& e)
    {
        NS_LOG_ERROR("Failed to deserialize solution: " << e.what());
        MilpSolution errorSol;
        errorSol.status = "error";
        m_statistics.totalErrors++;
        return errorSol;
    }
    
    // Update statistics
    auto endTime = std::chrono::high_resolution_clock::now();
    double elapsedTime = std::chrono::duration<double>(endTime - startTime).count();
    
    UpdateStatistics(solution.IsOptimal() || solution.IsFeasible(),
                     elapsedTime,
                     jsonRequest.length(),
                     jsonResponse.length());
    
    m_statistics.totalSolutionsReceived++;
    
    return solution;
}

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

std::string
NrMilpInterface::SerializeProblem(const MilpProblem& problem)
{
    NS_LOG_FUNCTION(this);
    
    json j;
    
    // Basic parameters
    j["numUEs"] = problem.numUEs;
    j["bandwidth"] = problem.bandwidth;
    j["totalBandwidthPrbs"] = problem.totalBandwidthPrbs;
    j["timeWindow"] = problem.timeWindow;
    j["numerology"] = problem.numerology;
    j["slotDuration"] = problem.slotDuration;
    j["totalSlots"] = problem.totalSlots;
    
    // UE SLAs
    j["ues"] = json::array();
    for (const auto& ue : problem.ues)
    {
        json ueJson;
        ueJson["ueId"] = ue.ueId;
        ueJson["sliceType"] = SliceTypeToString(ue.sliceType);
        ueJson["throughputMbps"] = ue.throughputMbps;
        ueJson["latencyMs"] = ue.latencyMs;
        ueJson["mcs"] = ue.mcs;
        ueJson["tbs"] = ue.tbs;
        j["ues"].push_back(ueJson);
    }
    
    return j.dump();  // Convert to string
}

MilpSolution
NrMilpInterface::DeserializeSolution(const std::string& jsonStr)
{
    NS_LOG_FUNCTION(this);
    
    MilpSolution solution;
    
    try
    {
        json j = json::parse(jsonStr);
        
        // Basic fields
        solution.status = j.value("status", "unknown");
        solution.objectiveValue = j.value("objectiveValue", 0.0);
        solution.solveTimeSeconds = j.value("solveTimeSeconds", 0.0);
        
        // Allocations
        if (j.contains("allocations") && j["allocations"].is_array())
        {
            for (const auto& allocJson : j["allocations"])
            {
                PrbAllocation alloc;
                alloc.ueId = allocJson.value("ueId", 0);
                alloc.slotId = allocJson.value("slotId", 0);
                alloc.startPrb = allocJson.value("startPrb", 0);
                alloc.numPrbs = allocJson.value("numPrbs", 0);
                solution.allocations.push_back(alloc);
            }
        }
        
        // Summary (per-UE statistics)
        if (j.contains("summary") && j["summary"].is_object())
        {
            for (auto& [ueIdStr, summaryJson] : j["summary"].items())
            {
                uint32_t ueId = std::stoul(ueIdStr);
                
                MilpSolution::UeSummary summary;
                summary.totalPrbsAllocated = summaryJson.value("totalPrbsAllocated", 0);
                summary.expectedThroughputMbps = summaryJson.value("expectedThroughputMbps", 0.0);
                summary.maxLatencyMs = summaryJson.value("maxLatencyMs", 0.0);
                summary.slasMet = summaryJson.value("slasMet", false);
                
                solution.summary[ueId] = summary;
            }
        }
    }
    catch (const json::exception& e)
    {
        NS_LOG_ERROR("JSON parsing error: " << e.what());
        solution.status = "error";
    }
    
    return solution;
}

// ============================================================================
// STATISTICS
// ============================================================================

NrMilpInterface::Statistics
NrMilpInterface::GetStatistics() const
{
    return m_statistics;
}

void
NrMilpInterface::ResetStatistics()
{
    NS_LOG_FUNCTION(this);
    m_statistics = Statistics();
}

void
NrMilpInterface::PrintInfo(std::ostream& os) const
{
    os << "MILP Interface Configuration:" << std::endl;
    os << "  Solver Address: " << m_solverAddress << std::endl;
    os << "  Solver Port: " << m_solverPort << std::endl;
    os << "  Connection Timeout: " << m_connectionTimeout << " s" << std::endl;
    os << "  Solve Timeout: " << m_solveTimeout << " s" << std::endl;
    os << "  Auto-Reconnect: " << (m_autoReconnect ? "Yes" : "No") << std::endl;
    os << "  Max Retries: " << m_maxRetries << std::endl;
    os << "  Connected: " << (m_isConnected ? "Yes" : "No") << std::endl;
}

// ============================================================================
// PRIVATE SOCKET COMMUNICATION
// ============================================================================

bool
NrMilpInterface::SendData(const std::string& data)
{
    NS_LOG_FUNCTION(this << data.length());
    
    if (!m_isConnected || m_socketFd < 0)
    {
        NS_LOG_ERROR("Socket not connected");
        return false;
    }
    
    // Send length prefix (4 bytes, network byte order)
    uint32_t dataLen = htonl(static_cast<uint32_t>(data.length()));
    ssize_t bytesSent = send(m_socketFd, &dataLen, sizeof(dataLen), 0);
    if (bytesSent != sizeof(dataLen))
    {
        NS_LOG_ERROR("Failed to send length prefix");
        return false;
    }
    
    // Send actual data
    size_t totalSent = 0;
    while (totalSent < data.length())
    {
        ssize_t n = send(m_socketFd, 
                        data.c_str() + totalSent, 
                        data.length() - totalSent, 
                        0);
        
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;  // Interrupted, retry
            }
            NS_LOG_ERROR("Send error: " << strerror(errno));
            return false;
        }
        else if (n == 0)
        {
            NS_LOG_ERROR("Connection closed by peer");
            m_isConnected = false;
            return false;
        }
        
        totalSent += n;
    }
    
    NS_LOG_DEBUG("Sent " << totalSent << " bytes");
    return true;
}

bool
NrMilpInterface::ReceiveData(std::string& data, double timeout)
{
    NS_LOG_FUNCTION(this << timeout);
    
    if (!m_isConnected || m_socketFd < 0)
    {
        NS_LOG_ERROR("Socket not connected");
        return false;
    }
    
    data.clear();
    
    // First, receive length prefix (4 bytes)
    if (!WaitForData(timeout))
    {
        NS_LOG_ERROR("Timeout waiting for length prefix");
        return false;
    }
    
    uint32_t dataLen;
    ssize_t bytesRead = recv(m_socketFd, &dataLen, sizeof(dataLen), 0);
    if (bytesRead != sizeof(dataLen))
    {
        NS_LOG_ERROR("Failed to receive length prefix");
        return false;
    }
    
    dataLen = ntohl(dataLen);
    NS_LOG_DEBUG("Expecting " << dataLen << " bytes");
    
    if (dataLen == 0 || dataLen > 100 * 1024 * 1024)  // Sanity check: max 100MB
    {
        NS_LOG_ERROR("Invalid data length: " << dataLen);
        return false;
    }
    
    // Receive actual data
    data.reserve(dataLen);
    size_t totalReceived = 0;
    char buffer[4096];
    
    while (totalReceived < dataLen)
    {
        if (!WaitForData(timeout))
        {
            NS_LOG_ERROR("Timeout receiving data");
            return false;
        }
        
        size_t toRead = std::min(sizeof(buffer), dataLen - totalReceived);
        ssize_t n = recv(m_socketFd, buffer, toRead, 0);
        
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;  // Interrupted, retry
            }
            NS_LOG_ERROR("Receive error: " << strerror(errno));
            return false;
        }
        else if (n == 0)
        {
            NS_LOG_ERROR("Connection closed by peer");
            m_isConnected = false;
            return false;
        }
        
        data.append(buffer, n);
        totalReceived += n;
    }
    
    NS_LOG_DEBUG("Received " << totalReceived << " bytes");
    return true;
}

bool
NrMilpInterface::WaitForData(double timeout)
{
    NS_LOG_FUNCTION(this << timeout);
    
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(m_socketFd, &readSet);
    
    struct timeval tv;
    tv.tv_sec = static_cast<long>(timeout);
    tv.tv_usec = static_cast<long>((timeout - tv.tv_sec) * 1e6);
    
    int result = select(m_socketFd + 1, &readSet, nullptr, nullptr, &tv);
    
    if (result < 0)
    {
        NS_LOG_ERROR("Select error: " << strerror(errno));
        return false;
    }
    else if (result == 0)
    {
        NS_LOG_DEBUG("Select timeout");
        return false;
    }
    
    return true;  // Data available
}

void
NrMilpInterface::CloseSocket()
{
    NS_LOG_FUNCTION(this);
    
    if (m_socketFd >= 0)
    {
        close(m_socketFd);
        m_socketFd = -1;
    }
    
    m_isConnected = false;
}

// ============================================================================
// PRIVATE HELPER METHODS
// ============================================================================

bool
NrMilpInterface::ValidateProblem(const MilpProblem& problem) const
{
    NS_LOG_FUNCTION(this);
    
    if (!problem.IsValid())
    {
        NS_LOG_ERROR("Problem validation failed");
        return false;
    }
    
    return true;
}

void
NrMilpInterface::UpdateStatistics(bool success, double solveTime, 
                                   uint64_t bytesSent, uint64_t bytesReceived)
{
    NS_LOG_FUNCTION(this << success << solveTime);
    
    if (!success)
    {
        m_statistics.totalErrors++;
    }
    
    m_statistics.totalSolveTime += solveTime;
    
    // Update min/max
    if (m_statistics.totalSolutionsReceived == 0)
    {
        m_statistics.minSolveTime = solveTime;
        m_statistics.maxSolveTime = solveTime;
    }
    else
    {
        m_statistics.minSolveTime = std::min(m_statistics.minSolveTime, solveTime);
        m_statistics.maxSolveTime = std::max(m_statistics.maxSolveTime, solveTime);
    }
    
    // Update average (will be computed on next successful solve)
    if (m_statistics.totalSolutionsReceived > 0)
    {
        m_statistics.avgSolveTime = m_statistics.totalSolveTime / 
                                     m_statistics.totalSolutionsReceived;
    }
    
    m_statistics.totalBytesSent += bytesSent;
    m_statistics.totalBytesReceived += bytesReceived;
}

// ============================================================================
// STATISTICS PRINT
// ============================================================================

void
NrMilpInterface::Statistics::Print(std::ostream& os) const
{
    os << "MILP Interface Statistics:" << std::endl;
    os << "  Problems Submitted: " << totalProblemsSubmitted << std::endl;
    os << "  Solutions Received: " << totalSolutionsReceived << std::endl;
    os << "  Errors: " << totalErrors << std::endl;
    os << "  Timeouts: " << totalTimeouts << std::endl;
    os << "  Reconnections: " << totalReconnections << std::endl;
    
    if (totalSolutionsReceived > 0)
    {
        os << "  Solve Time:" << std::endl;
        os << "    Average: " << avgSolveTime << " s" << std::endl;
        os << "    Min: " << minSolveTime << " s" << std::endl;
        os << "    Max: " << maxSolveTime << " s" << std::endl;
        os << "    Total: " << totalSolveTime << " s" << std::endl;
    }
    
    os << "  Network Traffic:" << std::endl;
    os << "    Bytes Sent: " << totalBytesSent 
       << " (" << (totalBytesSent / 1024.0) << " KB)" << std::endl;
    os << "    Bytes Received: " << totalBytesReceived 
       << " (" << (totalBytesReceived / 1024.0) << " KB)" << std::endl;
}

} // namespace ns3
