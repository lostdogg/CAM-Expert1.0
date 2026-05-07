#include "CloudToolLibrary.h"
#include "SqlToolDatabase.h"
#include <cmath>
#include <algorithm>
#include <cctype>

// --------------------------------------------------------------------------
CloudToolLibrary::CloudToolLibrary() {
    loadBuiltinTools();
}

// --------------------------------------------------------------------------
// loadBuiltinTools()
//
// Populates the in-memory cache with representative data for commonly used
// end mills from major manufacturers.  In a production deployment, this
// function would instead issue HTTPS requests to vendor APIs
// (e.g. CoroPlus®, MachiningCloud) and cache the responses on disk.
// --------------------------------------------------------------------------
void CloudToolLibrary::loadBuiltinTools() {

    // ================================================================
    // TOOL DIGITAL TWINS
    // ================================================================

    // ---- Sandvik Coromant CoroMill® 390 – Ø12 mm 4-flute end mill ----
    {
        ToolDigitalTwin t;
        t.manufacturerCode    = "Sandvik";
        t.catalogueNumber     = "R390-012A20-11M";
        t.description         = "CoroMill 390 Ø12mm 4-flute carbide end mill";
        t.cuttingDiameter     = 12.0;
        t.cuttingLength       = 26.0;
        t.cornerRadius        = 0.0;
        t.numberOfFlutes      = 4;
        t.helixAngle          = 38.0;
        t.cuttingMaterial     = "Carbide";
        t.coatingType         = "TiAlN";
        t.holderType          = "HSK-A63";
        t.shankDiameter       = 20.0;
        t.shankLength         = 50.0;
        t.overallAssemblyLength = 120.0;
        t.holderEnvelopeDiameter = 63.0;
        t.holderEnvelopeLength   = 95.0;
        t.sourceUrl           = "https://www.sandvik.coromant.com/tools/R390-012A20-11M";
        m_tools.push_back(t);
    }

    // ---- Kennametal HARVI™ III – Ø16 mm 4-flute end mill ----
    {
        ToolDigitalTwin t;
        t.manufacturerCode    = "Kennametal";
        t.catalogueNumber     = "B216A16-S16-216";
        t.description         = "HARVI III Ø16mm 4-flute AlTiN coated end mill";
        t.cuttingDiameter     = 16.0;
        t.cuttingLength       = 32.0;
        t.cornerRadius        = 1.0;
        t.numberOfFlutes      = 4;
        t.helixAngle          = 40.0;
        t.cuttingMaterial     = "Carbide";
        t.coatingType         = "AlTiN";
        t.holderType          = "Capto C5";
        t.shankDiameter       = 20.0;
        t.shankLength         = 60.0;
        t.overallAssemblyLength = 130.0;
        t.holderEnvelopeDiameter = 50.0;
        t.holderEnvelopeLength   = 100.0;
        t.sourceUrl           = "https://www.kennametal.com/tools/B216A16-S16-216";
        m_tools.push_back(t);
    }

    // ---- Iscar MULTI-MASTER – Ø10 mm ball end mill ----
    {
        ToolDigitalTwin t;
        t.manufacturerCode    = "Iscar";
        t.catalogueNumber     = "MM EC-B 10-C10-T";
        t.description         = "Multi-Master Ø10mm ball-nose carbide end mill";
        t.cuttingDiameter     = 10.0;
        t.cuttingLength       = 20.0;
        t.cornerRadius        = 5.0;  // ball = diameter/2
        t.numberOfFlutes      = 2;
        t.helixAngle          = 30.0;
        t.cuttingMaterial     = "Carbide";
        t.coatingType         = "TiN";
        t.holderType          = "SK30";
        t.shankDiameter       = 10.0;
        t.shankLength         = 40.0;
        t.overallAssemblyLength = 100.0;
        t.holderEnvelopeDiameter = 30.0;
        t.holderEnvelopeLength   = 70.0;
        t.sourceUrl           = "https://www.iscar.com/tools/MM-EC-B-10-C10-T";
        m_tools.push_back(t);
    }

    // ================================================================
    // MANUFACTURER CUTTING DATA
    // ================================================================

    // ---- Sandvik R390-012A20-11M + Aluminum ----
    {
        ManufacturerCuttingData d;
        d.catalogueNumber         = "R390-012A20-11M";
        d.materialClass           = MaterialClass::Aluminum;
        d.nominalDiameter         = 12.0;
        d.surfaceSpeedMin         = 600.0;
        d.surfaceSpeedMax         = 1200.0;
        d.feedPerToothMin         = 0.08;
        d.feedPerToothMax         = 0.20;
        d.maxAxialDepth           = 26.0;  // full flute length
        d.maxRadialDepth          = 12.0;  // full Ø
        d.recommendedAxialDepth   = 24.0;  // ~92 % of flute – HSM deep cut
        d.recommendedRadialDepth  = 1.8;   // 15 % of Ø – dynamic/trochoidal
        d.maxRadialEngagement     = 0.15;
        d.trochoidalLoopRadius    = 0.30;
        d.recommendedCoolant      = CuttingParams::Coolant::Mist;
        d.optimisedForHSM         = true;
        d.optimisedForTrochoidal  = false;
        d.expectedToolLifeMin     = 60.0;
        m_cuttingData.push_back(d);
    }

    // ---- Sandvik R390-012A20-11M + Steel ----
    {
        ManufacturerCuttingData d;
        d.catalogueNumber         = "R390-012A20-11M";
        d.materialClass           = MaterialClass::Steel;
        d.nominalDiameter         = 12.0;
        d.surfaceSpeedMin         = 100.0;
        d.surfaceSpeedMax         = 250.0;
        d.feedPerToothMin         = 0.04;
        d.feedPerToothMax         = 0.10;
        d.maxAxialDepth           = 26.0;
        d.maxRadialDepth          = 8.0;
        d.recommendedAxialDepth   = 20.0;
        d.recommendedRadialDepth  = 1.5;
        d.maxRadialEngagement     = 0.12;
        d.trochoidalLoopRadius    = 0.25;
        d.recommendedCoolant      = CuttingParams::Coolant::Flood;
        d.optimisedForHSM         = true;
        d.optimisedForTrochoidal  = false;
        d.expectedToolLifeMin     = 45.0;
        m_cuttingData.push_back(d);
    }

    // ---- Sandvik R390-012A20-11M + Titanium ----
    {
        ManufacturerCuttingData d;
        d.catalogueNumber         = "R390-012A20-11M";
        d.materialClass           = MaterialClass::Titanium;
        d.nominalDiameter         = 12.0;
        d.surfaceSpeedMin         = 40.0;
        d.surfaceSpeedMax         = 80.0;
        d.feedPerToothMin         = 0.04;
        d.feedPerToothMax         = 0.07;
        d.maxAxialDepth           = 26.0;
        d.maxRadialDepth          = 1.2;   // very thin radial engagement
        d.recommendedAxialDepth   = 22.0;  // deep axial, thin radial
        d.recommendedRadialDepth  = 0.9;   // ~7.5 % of Ø
        d.maxRadialEngagement     = 0.08;
        d.trochoidalLoopRadius    = 0.20;
        d.recommendedCoolant      = CuttingParams::Coolant::Flood;
        d.optimisedForHSM         = false;
        d.optimisedForTrochoidal  = true;
        d.expectedToolLifeMin     = 30.0;
        m_cuttingData.push_back(d);
    }

    // ---- Kennametal B216A16-S16-216 + Aluminum ----
    {
        ManufacturerCuttingData d;
        d.catalogueNumber         = "B216A16-S16-216";
        d.materialClass           = MaterialClass::Aluminum;
        d.nominalDiameter         = 16.0;
        d.surfaceSpeedMin         = 500.0;
        d.surfaceSpeedMax         = 1000.0;
        d.feedPerToothMin         = 0.10;
        d.feedPerToothMax         = 0.22;
        d.maxAxialDepth           = 32.0;
        d.maxRadialDepth          = 16.0;
        d.recommendedAxialDepth   = 28.0;
        d.recommendedRadialDepth  = 2.4;   // 15 % of Ø
        d.maxRadialEngagement     = 0.15;
        d.trochoidalLoopRadius    = 0.30;
        d.recommendedCoolant      = CuttingParams::Coolant::Mist;
        d.optimisedForHSM         = true;
        d.optimisedForTrochoidal  = false;
        d.expectedToolLifeMin     = 75.0;
        m_cuttingData.push_back(d);
    }

    // ---- Iscar MM-EC-B-10-C10-T + Steel (3D finishing) ----
    {
        ManufacturerCuttingData d;
        d.catalogueNumber         = "MM EC-B 10-C10-T";
        d.materialClass           = MaterialClass::Steel;
        d.nominalDiameter         = 10.0;
        d.surfaceSpeedMin         = 120.0;
        d.surfaceSpeedMax         = 220.0;
        d.feedPerToothMin         = 0.02;
        d.feedPerToothMax         = 0.06;
        d.maxAxialDepth           = 10.0;  // ball – axial = Ø typically
        d.maxRadialDepth          = 5.0;
        d.recommendedAxialDepth   = 8.0;
        d.recommendedRadialDepth  = 0.5;   // fine finish scallop
        d.maxRadialEngagement     = 0.10;
        d.trochoidalLoopRadius    = 0.25;
        d.recommendedCoolant      = CuttingParams::Coolant::Flood;
        d.optimisedForHSM         = false;
        d.optimisedForTrochoidal  = false;
        d.expectedToolLifeMin     = 50.0;
        m_cuttingData.push_back(d);
    }
}

// --------------------------------------------------------------------------
const ToolDigitalTwin* CloudToolLibrary::lookupTool(
    const std::string& catalogueNumber) const {
    for (const auto& t : m_tools)
        if (t.catalogueNumber == catalogueNumber)
            return &t;
    return nullptr;
}

// --------------------------------------------------------------------------
const ManufacturerCuttingData* CloudToolLibrary::lookupCuttingData(
    const std::string& catalogueNumber,
    MaterialClass materialClass) const {
    for (const auto& d : m_cuttingData)
        if (d.catalogueNumber == catalogueNumber && d.materialClass == materialClass)
            return &d;
    return nullptr;
}

// --------------------------------------------------------------------------
// getOptimalParameters()
//
// Scales manufacturer data to the actual tool diameter and returns a
// CuttingParams ready to pass into any toolpath generator.
// --------------------------------------------------------------------------
CuttingParams CloudToolLibrary::getOptimalParameters(
    const CuttingTool& tool,
    MaterialClass materialClass) const {

    // Try to find an exact match (same catalogue number is not stored on
    // CuttingTool, so we search by closest diameter within the same material)
    const ManufacturerCuttingData* best = nullptr;
    double bestDiamDiff = 1e9;
    for (const auto& d : m_cuttingData) {
        if (d.materialClass != materialClass) continue;
        double diff = std::abs(d.nominalDiameter - tool.diameter);
        if (diff < bestDiamDiff) {
            bestDiamDiff = diff;
            best = &d;
        }
    }

    if (!best) {
        // No cloud data available – fall back to MaterialLibrary
        return m_matLib.calculate(tool, materialClass).toCuttingParams();
    }

    // Scale surface speed to mid-range
    double vc = (best->surfaceSpeedMin + best->surfaceSpeedMax) * 0.5; // m/min
    double rpm = (vc * 1000.0) / (PI_CTL * tool.diameter);             // rev/min

    // Scale feed-per-tooth to mid-range, adjusted for diameter ratio.
    // Square-root scaling maintains proportional chip thickness: a smaller
    // tool cutting at the same chip load relative to its diameter should use
    // a slightly lower absolute fpt to avoid overloading the reduced cross-section.
    double diamRatio = tool.diameter / best->nominalDiameter;
    double fpt = (best->feedPerToothMin + best->feedPerToothMax) * 0.5;
    fpt *= std::sqrt(diamRatio);

    double feedRate   = rpm * tool.numFlutes * fpt;
    double axialDepth = best->recommendedAxialDepth * diamRatio;
    double radialDepth= best->recommendedRadialDepth * diamRatio;

    // Clip to per-diameter maximum
    axialDepth  = std::min(axialDepth,  best->maxAxialDepth  * diamRatio);
    radialDepth = std::min(radialDepth, best->maxRadialDepth * diamRatio);

    CuttingParams cp;
    cp.spindleRPM    = rpm;
    cp.surfaceSpeed  = vc;
    cp.feedPerTooth  = fpt;
    cp.feedRate      = feedRate;
    cp.plungeRate    = feedRate * 0.30;
    cp.axialDepth    = axialDepth;
    cp.radialDepth   = radialDepth;
    cp.stockAllowance= 0.25;
    cp.coolant       = best->recommendedCoolant;
    return cp;
}

// --------------------------------------------------------------------------
// computeDynamicArcLimits()
//
// Derives safe arc radii from the feed rate and machine acceleration limit.
//
//   Centripetal acceleration:   a_c = v² / r
//   Minimum radius:             r_min = v² / a_limit   (m²/s² / m·s⁻²  = m)
//
// The tool-based minimum (trochoidalLoopRadius × Ø) is also respected –
// the larger of the two limits is returned as the effective minimum.
// --------------------------------------------------------------------------
DynamicArcLimits CloudToolLibrary::computeDynamicArcLimits(
    const CuttingTool& tool,
    const ManufacturerCuttingData& mcd,
    double feedRateMmMin,
    double machineAccelLimitMpss) {

    DynamicArcLimits lim;

    // Convert feed to m/s
    double v_ms = feedRateMmMin / 60000.0;

    // Physics-based minimum radius (mm)
    double r_min_physics_mm = 0.0;
    if (machineAccelLimitMpss > 0.0)
        r_min_physics_mm = (v_ms * v_ms / machineAccelLimitMpss) * 1000.0;

    // Tool-data-based minimum radius (mm)
    double r_min_tool_mm = mcd.trochoidalLoopRadius * tool.diameter;

    lim.minLoopRadius = std::max(r_min_physics_mm, r_min_tool_mm);
    lim.minLoopRadius = std::max(lim.minLoopRadius, 0.5); // absolute floor 0.5 mm

    // Maximum loop radius: beyond this the trochoidal benefit disappears
    lim.maxLoopRadius = mcd.maxRadialEngagement * tool.diameter * 3.0;

    // Recommended: midpoint weighted toward the tool data
    lim.recommendedLoopRadius =
        lim.minLoopRadius + (lim.maxLoopRadius - lim.minLoopRadius) * 0.35;

    lim.centripAccelLimitMpss = machineAccelLimitMpss;

    // Velocity ratio when cornering (slow down to keep a_c within limit)
    if (lim.recommendedLoopRadius > 0.0 && machineAccelLimitMpss > 0.0) {
        double r_m = lim.recommendedLoopRadius * 1e-3;
        double v_corner_ms = std::sqrt(machineAccelLimitMpss * r_m);
        double v_prog_ms   = v_ms > 1e-9 ? v_ms : 1.0;
        lim.maxCornerVelocityRatio =
            std::min(1.0, v_corner_ms / v_prog_ms);
    } else {
        lim.maxCornerVelocityRatio = 0.6; // safe default
    }

    return lim;
}

// --------------------------------------------------------------------------
void CloudToolLibrary::registerTool(ToolDigitalTwin twin) {
    // Replace existing entry if catalogueNumber matches
    for (auto& t : m_tools) {
        if (t.catalogueNumber == twin.catalogueNumber) {
            t = std::move(twin);
            return;
        }
    }
    m_tools.push_back(std::move(twin));
}

// --------------------------------------------------------------------------
void CloudToolLibrary::registerCuttingData(ManufacturerCuttingData data) {
    for (auto& d : m_cuttingData) {
        if (d.catalogueNumber == data.catalogueNumber &&
            d.materialClass   == data.materialClass) {
            d = std::move(data);
            return;
        }
    }
    m_cuttingData.push_back(std::move(data));
}

// --------------------------------------------------------------------------
void CloudToolLibrary::exportToSqlDatabase(SqlToolDatabase& db) const {
    for (const auto& t : m_tools) {
        SqlToolRow row;
        row.key = t.catalogueNumber;
        row.tool.id = 0;
        row.tool.name = t.description;
        row.tool.type = (std::abs(t.cornerRadius - (t.cuttingDiameter * 0.5)) < 1e-6)
            ? ToolType::BallEndMill : ToolType::EndMill;
        row.tool.diameter = t.cuttingDiameter;
        row.tool.cornerRadius = t.cornerRadius;
        row.tool.fluteLength = t.cuttingLength;
        row.tool.overallLength = t.overallAssemblyLength;
        row.tool.numFlutes = t.numberOfFlutes;
        row.tool.rakeAngle = t.helixAngle;
        row.tool.material = t.cuttingMaterial;
        db.upsertTool(row);
    }

    for (const auto& d : m_cuttingData) {
        SqlCuttingDataRow row;
        row.toolKey               = d.catalogueNumber;
        row.materialClass         = d.materialClass;
        row.nominalDiameter       = d.nominalDiameter;
        row.surfaceSpeedMin       = d.surfaceSpeedMin;
        row.surfaceSpeedMax       = d.surfaceSpeedMax;
        row.feedPerToothMin       = d.feedPerToothMin;
        row.feedPerToothMax       = d.feedPerToothMax;
        row.maxAxialDepth         = d.maxAxialDepth;
        row.maxRadialDepth        = d.maxRadialDepth;
        row.recommendedAxialDepth = d.recommendedAxialDepth;
        row.recommendedRadialDepth= d.recommendedRadialDepth;
        row.maxRadialEngagement   = d.maxRadialEngagement;
        row.trochoidalLoopRadius  = d.trochoidalLoopRadius;
        row.coolant               = d.recommendedCoolant;
        row.optimisedForHSM       = d.optimisedForHSM;
        row.optimisedForTrochoidal= d.optimisedForTrochoidal;
        row.expectedToolLifeMin   = d.expectedToolLifeMin;
        db.upsertCuttingData(row);
    }

    m_matLib.exportToSqlDatabase(db);
}

// --------------------------------------------------------------------------
void CloudToolLibrary::importFromSqlDatabase(const SqlToolDatabase& db, bool replaceExisting) {
    if (replaceExisting) {
        m_tools.clear();
        m_cuttingData.clear();
    }

    for (const auto& t : db.tools()) {
        ToolDigitalTwin twin;
        twin.catalogueNumber       = t.key;
        twin.description           = t.tool.name;
        twin.cuttingDiameter       = t.tool.diameter;
        twin.cornerRadius          = t.tool.cornerRadius;
        twin.cuttingLength         = t.tool.fluteLength;
        twin.overallAssemblyLength = t.tool.overallLength;
        twin.numberOfFlutes        = t.tool.numFlutes;
        twin.helixAngle            = t.tool.rakeAngle;
        twin.cuttingMaterial       = t.tool.material;
        registerTool(std::move(twin));
    }

    for (const auto& c : db.cuttingData()) {
        ManufacturerCuttingData d;
        d.catalogueNumber         = c.toolKey;
        d.materialClass           = c.materialClass;
        d.nominalDiameter         = c.nominalDiameter;
        d.surfaceSpeedMin         = c.surfaceSpeedMin;
        d.surfaceSpeedMax         = c.surfaceSpeedMax;
        d.feedPerToothMin         = c.feedPerToothMin;
        d.feedPerToothMax         = c.feedPerToothMax;
        d.maxAxialDepth           = c.maxAxialDepth;
        d.maxRadialDepth          = c.maxRadialDepth;
        d.recommendedAxialDepth   = c.recommendedAxialDepth;
        d.recommendedRadialDepth  = c.recommendedRadialDepth;
        d.maxRadialEngagement     = c.maxRadialEngagement;
        d.trochoidalLoopRadius    = c.trochoidalLoopRadius;
        d.recommendedCoolant      = c.coolant;
        d.optimisedForHSM         = c.optimisedForHSM;
        d.optimisedForTrochoidal  = c.optimisedForTrochoidal;
        d.expectedToolLifeMin     = c.expectedToolLifeMin;
        registerCuttingData(std::move(d));
    }
}
