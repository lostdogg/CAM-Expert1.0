#pragma once
#ifndef SQL_TOOL_DATABASE_H
#define SQL_TOOL_DATABASE_H

#include "Toolpath.h"
#include "MaterialLibrary.h"
#include <string>
#include <vector>

// --------------------------------------------------------------------------
// SqlToolDatabase
//
// Canonical "Single Source of Truth" store for tool/material/process metadata.
// This class keeps normalized rows in-memory and provides SQL schema/migration
// scripts plus SQL export for persistence and interchange.
// --------------------------------------------------------------------------

struct SqlToolRow {
    std::string key; // stable logical key (catalogue number or internal key)
    CuttingTool tool;
};

struct SqlMaterialRow {
    std::string        key; // normalized material name
    MaterialProperties material;
};

struct SqlCuttingDataRow {
    std::string            toolKey;
    MaterialClass          materialClass = MaterialClass::Steel;
    double                 nominalDiameter       = 0.0;
    double                 surfaceSpeedMin       = 0.0;
    double                 surfaceSpeedMax       = 0.0;
    double                 feedPerToothMin       = 0.0;
    double                 feedPerToothMax       = 0.0;
    double                 maxAxialDepth         = 0.0;
    double                 maxRadialDepth        = 0.0;
    double                 recommendedAxialDepth = 0.0;
    double                 recommendedRadialDepth= 0.0;
    double                 maxRadialEngagement   = 0.0;
    double                 trochoidalLoopRadius  = 0.0;
    CuttingParams::Coolant coolant = CuttingParams::Coolant::Flood;
    bool                   optimisedForHSM       = false;
    bool                   optimisedForTrochoidal= false;
    double                 expectedToolLifeMin   = 0.0;
};

class SqlToolDatabase {
public:
    static constexpr int kSchemaVersion = 1;

    // Schema and migration SQL
    static std::string schemaDDL();
    static std::string migrationDDL(int fromVersion, int toVersion = kSchemaVersion);

    // Upserts
    void upsertTool(const SqlToolRow& row);
    void upsertMaterial(const SqlMaterialRow& row);
    void upsertCuttingData(const SqlCuttingDataRow& row);

    // Reads
    const SqlToolRow* findTool(const std::string& key) const;
    const SqlMaterialRow* findMaterial(const std::string& key) const;
    const SqlCuttingDataRow* findCuttingData(const std::string& toolKey,
                                             MaterialClass cls) const;

    const std::vector<SqlToolRow>& tools() const { return m_tools; }
    const std::vector<SqlMaterialRow>& materials() const { return m_materials; }
    const std::vector<SqlCuttingDataRow>& cuttingData() const { return m_cuttingData; }

    // Export all rows as SQL INSERT script (idempotent upsert style)
    std::string exportSqlSnapshot() const;

    // Utility
    void clear();

private:
    static std::string sqlEscape(const std::string& s);
    static const char* boolSql(bool v) { return v ? "1" : "0"; }

    std::vector<SqlToolRow>       m_tools;
    std::vector<SqlMaterialRow>   m_materials;
    std::vector<SqlCuttingDataRow> m_cuttingData;
};

#endif // SQL_TOOL_DATABASE_H

