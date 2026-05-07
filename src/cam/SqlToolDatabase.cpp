#include "SqlToolDatabase.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace {
static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
}

std::string SqlToolDatabase::schemaDDL() {
    return
R"(PRAGMA foreign_keys = ON;
CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS tools (
    key TEXT PRIMARY KEY,
    id INTEGER NOT NULL,
    name TEXT NOT NULL,
    type INTEGER NOT NULL,
    diameter REAL NOT NULL,
    corner_radius REAL NOT NULL,
    flute_length REAL NOT NULL,
    overall_length REAL NOT NULL,
    num_flutes INTEGER NOT NULL,
    rake_angle REAL NOT NULL,
    material TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS materials (
    key TEXT PRIMARY KEY,
    mat_class INTEGER NOT NULL,
    name TEXT NOT NULL,
    hardness_bhn REAL NOT NULL,
    tensile_mpa REAL NOT NULL,
    thermal_conductivity REAL NOT NULL,
    machinability_index REAL NOT NULL,
    vc_min REAL NOT NULL,
    vc_max REAL NOT NULL,
    fz_min REAL NOT NULL,
    fz_max REAL NOT NULL,
    preferred_coolant INTEGER NOT NULL,
    use_hsm INTEGER NOT NULL,
    use_trochoidal INTEGER NOT NULL,
    requires_smoothing INTEGER NOT NULL,
    high_chip_evac INTEGER NOT NULL,
    max_radial_eng REAL NOT NULL,
    min_radial_eng REAL NOT NULL,
    preferred_entry INTEGER NOT NULL,
    reposition_style INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS cutting_data (
    tool_key TEXT NOT NULL,
    material_class INTEGER NOT NULL,
    nominal_diameter REAL NOT NULL,
    vc_min REAL NOT NULL,
    vc_max REAL NOT NULL,
    fz_min REAL NOT NULL,
    fz_max REAL NOT NULL,
    max_axial REAL NOT NULL,
    max_radial REAL NOT NULL,
    rec_axial REAL NOT NULL,
    rec_radial REAL NOT NULL,
    max_radial_eng REAL NOT NULL,
    trochoidal_loop_radius REAL NOT NULL,
    coolant INTEGER NOT NULL,
    optimised_hsm INTEGER NOT NULL,
    optimised_trochoidal INTEGER NOT NULL,
    expected_tool_life_min REAL NOT NULL,
    PRIMARY KEY(tool_key, material_class),
    FOREIGN KEY(tool_key) REFERENCES tools(key) ON DELETE CASCADE
);
)";
}

std::string SqlToolDatabase::migrationDDL(int fromVersion, int toVersion) {
    if (fromVersion >= toVersion)
        return "-- No migration needed.\n";
    // Version 1 is initial normalized schema.
    if (fromVersion < 1 && toVersion >= 1)
        return schemaDDL();
    return "-- Unsupported schema migration path.\n";
}

void SqlToolDatabase::upsertTool(const SqlToolRow& row) {
    for (auto& r : m_tools) {
        if (r.key == row.key) { r = row; return; }
    }
    m_tools.push_back(row);
}

void SqlToolDatabase::upsertMaterial(const SqlMaterialRow& row) {
    for (auto& r : m_materials) {
        if (toLower(r.key) == toLower(row.key)) { r = row; return; }
    }
    m_materials.push_back(row);
}

void SqlToolDatabase::upsertCuttingData(const SqlCuttingDataRow& row) {
    for (auto& r : m_cuttingData) {
        if (r.toolKey == row.toolKey && r.materialClass == row.materialClass) {
            r = row;
            return;
        }
    }
    m_cuttingData.push_back(row);
}

const SqlToolRow* SqlToolDatabase::findTool(const std::string& key) const {
    for (const auto& r : m_tools)
        if (r.key == key) return &r;
    return nullptr;
}

const SqlMaterialRow* SqlToolDatabase::findMaterial(const std::string& key) const {
    std::string needle = toLower(key);
    for (const auto& r : m_materials)
        if (toLower(r.key) == needle) return &r;
    return nullptr;
}

const SqlCuttingDataRow* SqlToolDatabase::findCuttingData(const std::string& toolKey,
                                                          MaterialClass cls) const {
    for (const auto& r : m_cuttingData)
        if (r.toolKey == toolKey && r.materialClass == cls) return &r;
    return nullptr;
}

std::string SqlToolDatabase::sqlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\'') out += "''";
        else out += c;
    }
    return out;
}

std::string SqlToolDatabase::exportSqlSnapshot() const {
    std::ostringstream oss;
    oss << schemaDDL() << "\n";
    oss << "DELETE FROM schema_version;\n";
    oss << "INSERT INTO schema_version(version) VALUES(" << kSchemaVersion << ");\n";

    for (const auto& t : m_tools) {
        oss << "INSERT OR REPLACE INTO tools("
            << "key,id,name,type,diameter,corner_radius,flute_length,overall_length,num_flutes,rake_angle,material"
            << ") VALUES('"
            << sqlEscape(t.key) << "',"
            << t.tool.id << ",'"
            << sqlEscape(t.tool.name) << "',"
            << static_cast<int>(t.tool.type) << ","
            << t.tool.diameter << ","
            << t.tool.cornerRadius << ","
            << t.tool.fluteLength << ","
            << t.tool.overallLength << ","
            << t.tool.numFlutes << ","
            << t.tool.rakeAngle << ",'"
            << sqlEscape(t.tool.material) << "');\n";
    }

    for (const auto& m : m_materials) {
        const auto& v = m.material;
        oss << "INSERT OR REPLACE INTO materials("
            << "key,mat_class,name,hardness_bhn,tensile_mpa,thermal_conductivity,machinability_index,"
            << "vc_min,vc_max,fz_min,fz_max,preferred_coolant,use_hsm,use_trochoidal,requires_smoothing,"
            << "high_chip_evac,max_radial_eng,min_radial_eng,preferred_entry,reposition_style"
            << ") VALUES('"
            << sqlEscape(m.key) << "',"
            << static_cast<int>(v.matClass) << ",'"
            << sqlEscape(v.name) << "',"
            << v.hardnessBrinell << ","
            << v.tensileStrengthMPa << ","
            << v.thermalConductivity << ","
            << v.machinabilityIndex << ","
            << v.surfaceSpeedMin << ","
            << v.surfaceSpeedMax << ","
            << v.feedPerToothMin << ","
            << v.feedPerToothMax << ","
            << static_cast<int>(v.preferredCoolant) << ","
            << boolSql(v.useHighSpeedMachining) << ","
            << boolSql(v.useTrochoidalPaths) << ","
            << boolSql(v.requiresGCodeSmoothing) << ","
            << boolSql(v.highChipEvacuationPriority) << ","
            << v.maxRadialEngagement << ","
            << v.minRadialEngagement << ","
            << static_cast<int>(v.preferredEntry) << ","
            << static_cast<int>(v.repositionStyle) << ");\n";
    }

    for (const auto& c : m_cuttingData) {
        oss << "INSERT OR REPLACE INTO cutting_data("
            << "tool_key,material_class,nominal_diameter,vc_min,vc_max,fz_min,fz_max,max_axial,max_radial,"
            << "rec_axial,rec_radial,max_radial_eng,trochoidal_loop_radius,coolant,optimised_hsm,"
            << "optimised_trochoidal,expected_tool_life_min"
            << ") VALUES('"
            << sqlEscape(c.toolKey) << "',"
            << static_cast<int>(c.materialClass) << ","
            << c.nominalDiameter << ","
            << c.surfaceSpeedMin << ","
            << c.surfaceSpeedMax << ","
            << c.feedPerToothMin << ","
            << c.feedPerToothMax << ","
            << c.maxAxialDepth << ","
            << c.maxRadialDepth << ","
            << c.recommendedAxialDepth << ","
            << c.recommendedRadialDepth << ","
            << c.maxRadialEngagement << ","
            << c.trochoidalLoopRadius << ","
            << static_cast<int>(c.coolant) << ","
            << boolSql(c.optimisedForHSM) << ","
            << boolSql(c.optimisedForTrochoidal) << ","
            << c.expectedToolLifeMin << ");\n";
    }

    return oss.str();
}

void SqlToolDatabase::clear() {
    m_tools.clear();
    m_materials.clear();
    m_cuttingData.clear();
}

