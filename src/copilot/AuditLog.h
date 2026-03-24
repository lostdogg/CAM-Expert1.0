#pragma once
#ifndef AUDIT_LOG_H
#define AUDIT_LOG_H

#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <mutex>

// --------------------------------------------------------------------------
// AuditLog – Security & Legal Guardrails
//
// Every Copilot suggestion—and its outcome—is written to an append-only log
// file.  If a tool crash occurs because of an AI suggestion, the developer
// can inspect exactly which logic branch was triggered, what parameters were
// proposed, and whether the operator accepted or rejected the suggestion.
//
// Design decisions:
//   • Thread-safe: a mutex guards the write path (refresh can arrive from any
//     thread in a future async implementation).
//   • Geometry-anonymous: raw CAD coordinates are NOT logged; only human-
//     readable metadata (feature type, strategy name, material class) is
//     written.  This ensures sensitive IP stays local even if the log file
//     is transmitted for support purposes.
//   • Append-only file: the log file is opened in append mode so no entry
//     can be silently overwritten.
// --------------------------------------------------------------------------

enum class AuditOutcome {
    Proposed,   // suggestion shown to the user, awaiting response
    Accepted,   // user clicked "Apply" – toolpath was generated
    Rejected,   // user dismissed the suggestion
    Error       // CopilotEngine reported an internal error
};

struct AuditEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string  sessionId;     // unique per application launch
    std::string  command;       // the original user command (anonymised)
    std::string  action;        // e.g. "Mill", "Drill"
    std::string  strategy;      // e.g. "DynamicMill"
    std::string  material;      // e.g. "Titanium"
    std::string  rationale;     // parameter summary
    AuditOutcome outcome        = AuditOutcome::Proposed;
    std::string  errorDetail;   // populated on AuditOutcome::Error

    std::string toLogLine() const;
};

// --------------------------------------------------------------------------
class AuditLog {
public:
    // filePath: absolute path to the log file.
    // If empty, entries are kept in memory only (useful for unit tests).
    explicit AuditLog(const std::string& filePath = "");
    ~AuditLog();

    // Record a new suggestion.  Returns the index of the created entry.
    std::size_t record(AuditEntry entry);

    // Update the outcome of a previously recorded entry.
    void updateOutcome(std::size_t entryIndex,
                       AuditOutcome outcome,
                       const std::string& errorDetail = "");

    // Read access (for UI display of recent suggestions)
    const std::vector<AuditEntry>& entries() const { return m_entries; }
    const AuditEntry*              last()     const;

    // Flush pending writes to disk
    void flush();

private:
    void writeLine(const std::string& line);
    static std::string generateSessionId();

    std::string              m_filePath;
    std::string              m_sessionId;
    std::vector<AuditEntry>  m_entries;
    std::ofstream            m_file;
    std::mutex               m_mutex;
};

#endif // AUDIT_LOG_H
