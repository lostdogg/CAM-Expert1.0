#include "AuditLog.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <random>

// --------------------------------------------------------------------------
// AuditEntry::toLogLine
// --------------------------------------------------------------------------
std::string AuditEntry::toLogLine() const {
    // Convert timestamp to ISO 8601 local time string
    std::time_t t = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
       << " | " << sessionId
       << " | " << action
       << " | " << strategy
       << " | " << material
       << " | ";

    switch (outcome) {
        case AuditOutcome::Proposed: ss << "PROPOSED"; break;
        case AuditOutcome::Accepted: ss << "ACCEPTED"; break;
        case AuditOutcome::Rejected: ss << "REJECTED"; break;
        case AuditOutcome::Error:    ss << "ERROR";    break;
    }

    if (!errorDetail.empty())
        ss << " | ERR: " << errorDetail;

    // Append parameter rationale on next line (indented)
    if (!rationale.empty()) {
        ss << "\n    >> ";
        for (char c : rationale)
            ss << (c == '\n' ? "\n    >> " : std::string(1, c));
    }

    return ss.str();
}

// --------------------------------------------------------------------------
// AuditLog ctor / dtor
// --------------------------------------------------------------------------
AuditLog::AuditLog(const std::string& filePath)
    : m_filePath(filePath)
    , m_sessionId(AuditLog::generateSessionId())
{
    if (!m_filePath.empty()) {
        m_file.open(m_filePath, std::ios::app);
        if (m_file.is_open()) {
            std::ostringstream header;
            header << "=== CAM Expert Copilot Session " << m_sessionId << " ===";
            writeLine(header.str());
        }
    }
}

AuditLog::~AuditLog() {
    flush();
}

// --------------------------------------------------------------------------
// static
std::string AuditLog::generateSessionId() {
    std::mt19937_64 rng(static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    std::ostringstream ss;
    ss << std::hex << std::uppercase << dist(rng) << dist(rng);
    return ss.str();
}

// --------------------------------------------------------------------------
std::size_t AuditLog::record(AuditEntry entry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    entry.sessionId = m_sessionId;
    if (entry.timestamp == std::chrono::system_clock::time_point{})
        entry.timestamp = std::chrono::system_clock::now();

    m_entries.push_back(entry);
    std::size_t idx = m_entries.size() - 1;

    writeLine(m_entries[idx].toLogLine());
    return idx;
}

// --------------------------------------------------------------------------
void AuditLog::updateOutcome(std::size_t entryIndex,
                              AuditOutcome outcome,
                              const std::string& errorDetail)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (entryIndex >= m_entries.size()) return;

    m_entries[entryIndex].outcome     = outcome;
    m_entries[entryIndex].errorDetail = errorDetail;

    // Write an update line referencing the original entry
    std::ostringstream ss;
    ss << "  [UPDATE #" << entryIndex << "] ";
    switch (outcome) {
        case AuditOutcome::Accepted: ss << "ACCEPTED"; break;
        case AuditOutcome::Rejected: ss << "REJECTED"; break;
        case AuditOutcome::Error:    ss << "ERROR: " << errorDetail; break;
        default: break;
    }
    writeLine(ss.str());
}

// --------------------------------------------------------------------------
const AuditEntry* AuditLog::last() const {
    if (m_entries.empty()) return nullptr;
    return &m_entries.back();
}

// --------------------------------------------------------------------------
void AuditLog::flush() {
    if (m_file.is_open())
        m_file.flush();
}

// --------------------------------------------------------------------------
void AuditLog::writeLine(const std::string& line) {
    if (m_file.is_open())
        m_file << line << "\n";
}
