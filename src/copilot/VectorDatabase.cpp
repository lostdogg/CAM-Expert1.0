#include "VectorDatabase.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>

// --------------------------------------------------------------------------
// KnowledgeEntry helpers
// --------------------------------------------------------------------------

bool KnowledgeEntry::hasTag(const std::string& tag) const {
    std::string lower = tags;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    std::string lowerTag = tag;
    std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(), ::tolower);
    return lower.find(lowerTag) != std::string::npos;
}

// --------------------------------------------------------------------------
// VectorDatabase
// --------------------------------------------------------------------------

VectorDatabase::VectorDatabase() {
    populateDefaults();
}

void VectorDatabase::clear() {
    m_entries.clear();
    m_nextId   = 1;
    m_dfDirty  = true;
    m_dfCache.clear();
}

std::size_t VectorDatabase::addEntry(KnowledgeCategory  category,
                                      const std::string& title,
                                      const std::string& content,
                                      const std::string& tags) {
    KnowledgeEntry e;
    e.id       = m_nextId++;
    e.category = category;
    e.title    = title;
    e.content  = content;
    e.tags     = tags;
    m_entries.push_back(std::move(e));
    m_dfDirty  = true;
    return m_entries.back().id;
}

// --------------------------------------------------------------------------
// populateDefaults – built-in knowledge base
// --------------------------------------------------------------------------

void VectorDatabase::populateDefaults() {
    // ---- Tool catalog entries -----------------------------------------------
    addEntry(KnowledgeCategory::ToolCatalog,
        "Carbide End Mill 12mm 4-Flute",
        "Solid carbide 4-flute end mill. Diameter: 12 mm. Flute length: 30 mm. "
        "Suitable for aluminum, steel, stainless. Max RPM: 24000. "
        "Recommended SFM Aluminum: 800-1200 m/min. Steel: 60-120 m/min.",
        "endmill carbide 12mm aluminum steel");

    addEntry(KnowledgeCategory::ToolCatalog,
        "Carbide End Mill 6mm 2-Flute AlTiN-coated",
        "2-flute AlTiN-coated end mill for titanium and Inconel. Diameter: 6 mm. "
        "Flute length: 18 mm. Recommended surface speed: 40-60 m/min for Ti. "
        "Use trochoidal paths; max radial engagement 0.10×D.",
        "endmill titanium inconel 6mm trochoidal coating altin");

    addEntry(KnowledgeCategory::ToolCatalog,
        "Drill Bit HSS 8mm",
        "HSS twist drill 8 mm diameter. Point angle 118 deg. "
        "For through-holes in aluminum and mild steel. "
        "Recommended feed: 0.15 mm/rev. Peck cycle for depth > 3×D.",
        "drill hss 8mm hole aluminum steel peck");

    addEntry(KnowledgeCategory::ToolCatalog,
        "Ball End Mill 8mm Carbide",
        "Solid carbide ball end mill 8 mm. R4.0 nose radius. "
        "Suitable for 3D contour finishing and scallop paths. "
        "Step-over 0.5-1.0 mm for Ra 0.8 surface finish.",
        "ballendmill 3d finish scallop contour carbide 8mm");

    addEntry(KnowledgeCategory::ToolCatalog,
        "Face Mill 63mm Insert",
        "63 mm face mill with APKT-style inserts. "
        "Face milling aluminum stock at 400-600 m/min. "
        "Max depth of cut 2 mm, 80% width engagement.",
        "facemill insert 63mm aluminum facing");

    // ---- Material property entries ------------------------------------------
    addEntry(KnowledgeCategory::MaterialProperty,
        "Aluminum 7075-T6",
        "Aluminum alloy 7075-T6. BHN 150. Tensile 570 MPa. "
        "Machinability index 3.0. Thermal conductivity 130 W/m·K. "
        "Strategy: HSM (high-speed machining). Surface speed 400-1200 m/min. "
        "Radial engagement up to 0.70×D for roughing. Prefer flood or air-blast coolant.",
        "aluminum 7075 hsm high-speed machining");

    addEntry(KnowledgeCategory::MaterialProperty,
        "Titanium Grade 5 (Ti-6Al-4V)",
        "Ti-6Al-4V. BHN 334. Tensile 950 MPa. Machinability index 0.4. "
        "Low thermal conductivity 7 W/m·K – heat concentrates at tool tip. "
        "Strategy: trochoidal (dynamic milling), max radial engagement 0.10×D. "
        "Surface speed 40-80 m/min. Tangential arc entry preferred. "
        "Enable G-code smoothing to avoid dwell marks. Coolant: high-pressure flood.",
        "titanium ti6al4v trochoidal dynamic low-and-slow");

    addEntry(KnowledgeCategory::MaterialProperty,
        "Stainless Steel 316L",
        "316L austenitic stainless. BHN 217. Tensile 580 MPa. Work-hardens easily. "
        "Machinability index 0.5. Surface speed 80-150 m/min. "
        "Keep tool engaged; avoid rubbing. Flood coolant mandatory.",
        "stainless 316l work-harden flood");

    addEntry(KnowledgeCategory::MaterialProperty,
        "Inconel 718",
        "Nickel superalloy. BHN 380. Tensile 1375 MPa. Machinability index 0.2. "
        "Extreme heat resistance; use carbide with TiAlN coating. "
        "Max surface speed 30-40 m/min. Trochoidal essential. "
        "High-pressure coolant at 70 bar recommended.",
        "inconel nickel superalloy trochoidal high-pressure");

    addEntry(KnowledgeCategory::MaterialProperty,
        "Steel 4140",
        "AISI 4140 alloy steel. BHN 197. Tensile 655 MPa. Machinability 0.65. "
        "Surface speed 60-120 m/min. Feed 0.05-0.10 mm/tooth. "
        "General purpose carbide end mills suitable. Flood coolant.",
        "steel 4140 alloy carbide general-purpose");

    // ---- Machining strategy entries -----------------------------------------
    addEntry(KnowledgeCategory::MachiningStrategy,
        "Dynamic Mill (Trochoidal) – Pockets",
        "Dynamic milling uses small circular loops along the tool path. "
        "Keeps radial chip load constant – ideal for hard materials. "
        "Typical parameters: max engagement 0.10-0.15×D, axial depth 1.0-1.5×D. "
        "Entry: helical ramp or tangential arc. Never plunge into material. "
        "Best for: Titanium, Inconel, hardened steel.",
        "trochoidal dynamic pocket roughing hard");

    addEntry(KnowledgeCategory::MachiningStrategy,
        "HSM Pocket Roughing – Aluminum",
        "High-speed machining of aluminum pockets. Large axial (1.5-2.0×D), "
        "wide radial step (0.60-0.70×D). Helical entry. High RPM and feed rate. "
        "Air-blast or mist coolant. Tool: 2-3 flute polished for chip clearance.",
        "hsm aluminum pocket high-speed roughing helical");

    addEntry(KnowledgeCategory::MachiningStrategy,
        "2D Contour Finishing",
        "Final pass along a 2D profile. Small radial step (0.05-0.10 mm), "
        "single axial depth. Tangential lead-in / lead-out arcs. "
        "Suitable for any material. Achieves Ra 1.6 - 3.2 surfaces.",
        "contour finishing 2d profile tangential lead");

    addEntry(KnowledgeCategory::MachiningStrategy,
        "Peck Drilling – Deep Holes",
        "Intermittent peck cycle for holes deeper than 3×D. "
        "Peck depth 0.5-1.0×D per peck, retract to clear chips. "
        "Prevents chip packing and heat build-up. "
        "Suitable for all materials; essential for titanium and stainless.",
        "peck drilling deep hole chip clearance");

    addEntry(KnowledgeCategory::MachiningStrategy,
        "Helical Entry (Ramp) – Pockets",
        "Ramped helical descent into enclosed pockets instead of plunging. "
        "Ramp angle: 1-3 deg for hard materials, 3-5 deg for aluminum. "
        "Prevents core push and reduces plunge load on end mill tip.",
        "helical entry ramp pocket plunge");

    // ---- Safety rules -------------------------------------------------------
    addEntry(KnowledgeCategory::SafetyRule,
        "Max Radial Engagement Rule",
        "Radial step-over must never exceed the tool diameter. "
        "For dynamic milling in hard materials: max 0.15×D. "
        "For aluminum HSM: max 0.70×D. "
        "Exceeding these limits causes tool breakage.",
        "safety engagement stepover tool breakage");

    addEntry(KnowledgeCategory::SafetyRule,
        "Holder Clearance Rule",
        "Minimum clearance between tool holder and workpiece: 2 mm. "
        "If holder collision detected, increase tilt angle or use extended-reach holder. "
        "Never reduce this clearance below 1 mm.",
        "safety holder collision clearance tilt");

    addEntry(KnowledgeCategory::SafetyRule,
        "Gouge Tolerance",
        "Maximum allowable deviation below nominal surface: 0.0127 mm (0.0005 inch). "
        "Simulated gouges beyond this threshold must be corrected before posting. "
        "AI-suggested corrections must be re-verified after application.",
        "safety gouge tolerance simulation verify");
}

// --------------------------------------------------------------------------
// Tokeniser
// --------------------------------------------------------------------------

std::vector<std::string> VectorDatabase::tokenise(const std::string& text) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-') {
            cur += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            if (!cur.empty()) {
                tokens.push_back(cur);
                cur.clear();
            }
        }
    }
    if (!cur.empty())
        tokens.push_back(cur);
    return tokens;
}

// --------------------------------------------------------------------------
// TF-IDF
// --------------------------------------------------------------------------

void VectorDatabase::rebuildDfCache() const {
    m_dfCache.clear();
    for (const auto& e : m_entries) {
        auto terms = tokenise(e.content + " " + e.title + " " + e.tags);
        // Unique terms per document
        std::sort(terms.begin(), terms.end());
        terms.erase(std::unique(terms.begin(), terms.end()), terms.end());
        for (const auto& t : terms)
            m_dfCache[t]++;
    }
    m_dfDirty = false;
}

std::unordered_map<std::string, double>
VectorDatabase::buildTfIdf(const std::string& text) const {
    if (m_dfDirty)
        rebuildDfCache();

    auto tokens = tokenise(text);
    if (tokens.empty())
        return {};

    std::unordered_map<std::string, int> tf;
    for (const auto& t : tokens)
        tf[t]++;

    double N = static_cast<double>(m_entries.empty() ? 1 : m_entries.size());
    std::unordered_map<std::string, double> tfidf;
    for (const auto& [term, count] : tf) {
        double termFreq = static_cast<double>(count) / static_cast<double>(tokens.size());
        auto it = m_dfCache.find(term);
        int df = (it != m_dfCache.end()) ? it->second : 0;
        double idf = std::log((N + 1.0) / (static_cast<double>(df) + 1.0));
        tfidf[term] = termFreq * idf;
    }
    return tfidf;
}

double VectorDatabase::cosineSimilarity(
    const std::unordered_map<std::string, double>& a,
    const std::unordered_map<std::string, double>& b) const
{
    double dot = 0, normA = 0, normB = 0;
    for (const auto& [t, va] : a) {
        normA += va * va;
        auto it = b.find(t);
        if (it != b.end())
            dot += va * it->second;
    }
    for (const auto& [t, vb] : b)
        normB += vb * vb;

    if (normA < 1e-12 || normB < 1e-12)
        return 0.0;
    return dot / (std::sqrt(normA) * std::sqrt(normB));
}

// --------------------------------------------------------------------------
// search
// --------------------------------------------------------------------------

std::vector<SearchResult> VectorDatabase::search(
    const std::string& query,
    int                topK,
    KnowledgeCategory* filterCat) const
{
    auto queryVec = buildTfIdf(query);

    std::vector<SearchResult> results;
    for (const auto& e : m_entries) {
        if (filterCat && e.category != *filterCat)
            continue;
        auto docVec = buildTfIdf(e.content + " " + e.title + " " + e.tags);
        double score = cosineSimilarity(queryVec, docVec);
        results.push_back({&e, score});
    }

    std::sort(results.begin(), results.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.score > b.score;
              });

    if (static_cast<int>(results.size()) > topK)
        results.resize(static_cast<std::size_t>(topK));

    return results;
}

// --------------------------------------------------------------------------
// findById
// --------------------------------------------------------------------------

const KnowledgeEntry* VectorDatabase::findById(std::size_t id) const {
    for (const auto& e : m_entries)
        if (e.id == id)
            return &e;
    return nullptr;
}

// --------------------------------------------------------------------------
// recordProjectHistory
// --------------------------------------------------------------------------

void VectorDatabase::recordProjectHistory(const std::string& featureType,
                                           const std::string& materialName,
                                           double             toolDiameter,
                                           const std::string& strategyName,
                                           const std::string& outcomeNotes)
{
    std::ostringstream content;
    content << "Feature: " << featureType
            << ". Material: " << materialName
            << ". Tool diameter: " << toolDiameter << " mm"
            << ". Strategy: " << strategyName
            << ". Notes: " << outcomeNotes;

    std::string tags = featureType + " " + materialName + " " + strategyName;

    addEntry(KnowledgeCategory::ProjectHistory,
             "Project: " + featureType + " / " + materialName,
             content.str(),
             tags);
}

// --------------------------------------------------------------------------
// save / load (newline-delimited, hand-rolled to avoid external JSON deps)
// --------------------------------------------------------------------------

bool VectorDatabase::save(const std::string& filePath) const {
    std::ofstream ofs(filePath);
    if (!ofs.is_open())
        return false;

    for (const auto& e : m_entries) {
        // Simple pipe-delimited line: id|category|title|content|tags
        // Fields may not contain '|' or newline; escape if needed.
        auto esc = [](const std::string& s) {
            std::string r;
            for (char c : s) {
                if (c == '|')  r += "\\|";
                else if (c == '\n') r += "\\n";
                else r += c;
            }
            return r;
        };
        ofs << e.id << '|'
            << static_cast<int>(e.category) << '|'
            << esc(e.title) << '|'
            << esc(e.content) << '|'
            << esc(e.tags) << '\n';
    }
    return true;
}

bool VectorDatabase::load(const std::string& filePath) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open())
        return false;

    clear();

    auto unescape = [](const std::string& s) {
        std::string r;
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                if (s[i+1] == '|')  { r += '|';  ++i; }
                else if (s[i+1] == 'n') { r += '\n'; ++i; }
                else r += s[i];
            } else {
                r += s[i];
            }
        }
        return r;
    };

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        // Split by '|' (non-escaped)
        std::vector<std::string> parts;
        std::string cur;
        for (std::size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '|' && (i == 0 || line[i-1] != '\\')) {
                parts.push_back(cur);
                cur.clear();
            } else {
                cur += line[i];
            }
        }
        parts.push_back(cur);

        if (parts.size() < 5) continue;

        KnowledgeEntry e;
        e.id = static_cast<std::size_t>(std::stoul(parts[0]));
        e.category = static_cast<KnowledgeCategory>(std::stoi(parts[1]));
        e.title   = unescape(parts[2]);
        e.content = unescape(parts[3]);
        e.tags    = unescape(parts[4]);

        if (e.id >= m_nextId)
            m_nextId = e.id + 1;
        m_entries.push_back(std::move(e));
    }
    m_dfDirty = true;
    return true;
}
