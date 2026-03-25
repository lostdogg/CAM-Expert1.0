#pragma once
#ifndef VECTOR_DATABASE_H
#define VECTOR_DATABASE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

// --------------------------------------------------------------------------
// VectorDatabase – Local RAG (Retrieval-Augmented Generation) Knowledge Base
//
// A lightweight in-process vector store that keeps tool-manufacturer catalogs,
// material properties, machining best-practices, and records of previously
// successful projects so the Copilot can "retrieve" relevant facts before
// generating a suggestion.
//
// Design principles:
//   • Zero external dependencies: uses a TF-IDF cosine-similarity ranking
//     so it runs entirely on the user's hardware without an internet connection.
//   • Privacy preserving: only stores metadata (feature type, material, tool
//     diameter, strategy name) – never raw 3D coordinates.
//   • Persistent: can serialise / deserialise the index to a local file so
//     the knowledge base survives application restarts and grows over time.
// --------------------------------------------------------------------------

// ---- Entry category tags --------------------------------------------------
enum class KnowledgeCategory {
    ToolCatalog,        // tool geometry and manufacturer specs
    MaterialProperty,   // workpiece material machinability data
    MachiningStrategy,  // best-practice strategy descriptions
    ProjectHistory,     // anonymised records of accepted suggestions
    SafetyRule,         // hard rules (e.g. max engagement fractions)
    Custom,             // user-defined entries
};

// --------------------------------------------------------------------------
// KnowledgeEntry – one record in the database
// --------------------------------------------------------------------------
struct KnowledgeEntry {
    std::size_t     id         = 0;
    KnowledgeCategory category = KnowledgeCategory::Custom;
    std::string     title;          // short label (used in retrieval results)
    std::string     content;        // free-text body (the "knowledge")
    std::string     tags;           // space-separated tags for fast filtering
    double          relevance = 0;  // populated by search(); 0 outside search

    // Convenience: does this entry's tags contain the given tag?
    bool hasTag(const std::string& tag) const;
};

// --------------------------------------------------------------------------
// SearchResult – returned by VectorDatabase::search()
// --------------------------------------------------------------------------
struct SearchResult {
    const KnowledgeEntry* entry    = nullptr;
    double                score    = 0;   // cosine similarity [0..1]
};

// --------------------------------------------------------------------------
// VectorDatabase
// --------------------------------------------------------------------------
class VectorDatabase {
public:
    VectorDatabase();

    // --- Population ---

    // Add a single entry; returns its assigned id.
    std::size_t addEntry(KnowledgeCategory category,
                         const std::string& title,
                         const std::string& content,
                         const std::string& tags = "");

    // Bulk-add the built-in knowledge base (tool catalogs, material data …).
    // Called automatically by the constructor; may also be called explicitly
    // after clear() to reset to defaults.
    void populateDefaults();

    // Remove all entries (use with care).
    void clear();

    // --- Query ---

    // Return the top-k most relevant entries for the given query string.
    // If category != nullopt, only entries in that category are considered.
    // Uses TF-IDF cosine similarity.
    std::vector<SearchResult> search(
        const std::string& query,
        int                topK        = 5,
        KnowledgeCategory* filterCat   = nullptr) const;

    // Retrieve a single entry by exact id.
    const KnowledgeEntry* findById(std::size_t id) const;

    // Return all entries (read-only).
    const std::vector<KnowledgeEntry>& entries() const { return m_entries; }

    // --- Persistence ---

    // Save the full index to a plain-text file (one JSON object per line).
    bool save(const std::string& filePath) const;

    // Load entries from a file previously saved by save().
    bool load(const std::string& filePath);

    // --- Project history (RAG learning) ---

    // Record an accepted toolpath suggestion for future retrieval.
    // Only anonymised metadata is stored; no raw geometry coordinates.
    void recordProjectHistory(const std::string& featureType,
                               const std::string& materialName,
                               double             toolDiameter,
                               const std::string& strategyName,
                               const std::string& outcomeNotes);

    // Entry count
    std::size_t size() const { return m_entries.size(); }

private:
    // Build a TF-IDF term-frequency map for a document.
    std::unordered_map<std::string, double> buildTfIdf(
        const std::string& text) const;

    // Cosine similarity between two TF-IDF vectors.
    double cosineSimilarity(
        const std::unordered_map<std::string, double>& a,
        const std::unordered_map<std::string, double>& b) const;

    // Tokenise text into lower-case words, stripping punctuation.
    static std::vector<std::string> tokenise(const std::string& text);

    std::vector<KnowledgeEntry>                                  m_entries;
    std::size_t                                                  m_nextId = 1;

    // IDF cache: term → document-frequency count
    mutable std::unordered_map<std::string, int>                 m_dfCache;
    mutable bool                                                 m_dfDirty = true;

    void rebuildDfCache() const;
};

#endif // VECTOR_DATABASE_H
