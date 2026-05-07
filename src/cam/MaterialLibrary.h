#pragma once
#ifndef MATERIAL_LIBRARY_H
#define MATERIAL_LIBRARY_H

#include "Toolpath.h"
#include <string>
#include <vector>
#include <unordered_map>

class SqlToolDatabase;

// --------------------------------------------------------------------------
// MaterialLibrary
//
// Stores physical and machinability properties for common engineering
// materials, and provides a Feed/Speed Calculator that derives optimal
// CuttingParams for a given CuttingTool and material combination.
//
// The calculator implements two fundamentally different strategies:
//
//  Aluminum (and soft alloys):
//    • "Outrun the heat" – very high spindle speeds to carry heat away in chips
//    • Large radial step-over (50–70 % of tool Ø)
//    • Deep axial cuts (full flute engagement)
//    • Wide sweeping moves for chip evacuation
//    • Fast helical ramp entry
//
//  Titanium (and superalloys):
//    • "Low and slow" – reduced RPM but high feed-per-tooth
//    • Thin radial engagement (5–10 % of tool Ø) for short arc-of-contact
//    • Trochoidal toolpaths to prevent heat soak in corners
//    • Gradual tangential arc lead-in / lead-out
//    • G-code smoothing filters to eliminate jerky direction changes
// --------------------------------------------------------------------------

enum class MaterialClass {
    Aluminum,       // Al alloys (6061, 7075 …)
    Steel,          // mild / low-carbon steel
    StainlessSteel, // 304, 316 …
    Titanium,       // Ti-6Al-4V, Grade 5 …
    Inconel,        // Ni-base superalloys (718, 625)
    CastIron,
    Brass,
    Plastic,
    Carbon_CFRP,    // carbon-fibre reinforced polymer
    Custom
};

struct MaterialProperties {
    MaterialClass   matClass        = MaterialClass::Steel;
    std::string     name;

    // Mechanical
    double  hardnessBrinell     = 150;   // BHN
    double  tensileStrengthMPa  = 500;   // MPa
    double  thermalConductivity = 50;    // W/(m·K)  – key for heat model

    // Machinability index (1.0 = free-machining brass reference)
    double  machinabilityIndex  = 0.5;

    // Recommended surface speed range (m/min) for carbide tooling
    double  surfaceSpeedMin     = 100;
    double  surfaceSpeedMax     = 300;

    // Recommended feed-per-tooth (mm/tooth) for Ø12 mm endmill
    double  feedPerToothMin     = 0.02;
    double  feedPerToothMax     = 0.08;

    // Coolant preference
    CuttingParams::Coolant preferredCoolant = CuttingParams::Coolant::Flood;

    // ---------- Strategy flags ----------
    bool useHighSpeedMachining = false;   // HSM / Dynamic Motion preferred
    bool useTrochoidalPaths    = false;   // trochoidal loops mandatory
    bool requiresGCodeSmoothing= false;   // apply NURBS smoothing filter
    bool highChipEvacuationPriority = false; // wide sweeping chip clearance

    // Radial engagement limits (fraction of tool Ø)
    double maxRadialEngagement  = 0.5;    // fraction [0..1]
    double minRadialEngagement  = 0.05;   // for thin-engagement materials

    // Entry method preference
    enum class EntryMethod { HelicalRamp, TangentialArc, PlungeCenter };
    EntryMethod preferredEntry = EntryMethod::HelicalRamp;

    // Repositioning style
    enum class RepositionStyle { HighSpeedAir, ControlledMicroLift };
    RepositionStyle repositionStyle = RepositionStyle::HighSpeedAir;
};

// --------------------------------------------------------------------------
// FeedSpeedResult – output of the Feed/Speed Calculator
// --------------------------------------------------------------------------
struct FeedSpeedResult {
    double spindleRPM    = 0;    // rev/min
    double feedRate      = 0;    // mm/min
    double feedPerTooth  = 0;    // mm/tooth
    double surfaceSpeed  = 0;    // m/min
    double axialDepth    = 0;    // mm
    double radialDepth   = 0;    // mm (step-over)
    CuttingParams::Coolant coolant = CuttingParams::Coolant::Flood;

    // Derived strategy type
    bool   useHSM               = false;
    bool   useTrochoidal        = false;
    bool   applySmoothing       = false;
    double recommendedEngagement= 0;   // fraction of Ø
    MaterialProperties::EntryMethod     entryMethod    = MaterialProperties::EntryMethod::HelicalRamp;
    MaterialProperties::RepositionStyle repositionMode = MaterialProperties::RepositionStyle::HighSpeedAir;

    // Convert to CuttingParams
    CuttingParams toCuttingParams() const;
};

// --------------------------------------------------------------------------
// MaterialLibrary
// --------------------------------------------------------------------------
class MaterialLibrary {
public:
    MaterialLibrary();

    // Retrieve a built-in material by class
    const MaterialProperties& get(MaterialClass cls) const;

    // Retrieve by name (case-insensitive)
    const MaterialProperties* findByName(const std::string& name) const;

    // Add or replace a custom material
    void addCustom(MaterialProperties mat);

    // All materials in the library
    const std::vector<MaterialProperties>& all() const { return m_materials; }

    // ---- Feed/Speed Calculator ----
    // Computes optimal RPM, feed, and depth-of-cut for a given
    // tool / material combination, applying the correct strategy logic.
    FeedSpeedResult calculate(const CuttingTool& tool,
                               const MaterialProperties& mat) const;

    // Convenience overload: look up material by class first
    FeedSpeedResult calculate(const CuttingTool& tool,
                               MaterialClass cls) const;

    // SQL single-source-of-truth integration
    void exportToSqlDatabase(SqlToolDatabase& db) const;
    void importFromSqlDatabase(const SqlToolDatabase& db);

private:
    void loadDefaults();

    std::vector<MaterialProperties> m_materials;
    MaterialProperties m_fallback;   // returned when nothing matches
};

#endif // MATERIAL_LIBRARY_H
