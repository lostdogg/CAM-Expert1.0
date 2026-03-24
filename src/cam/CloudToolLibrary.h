#pragma once
#ifndef CLOUD_TOOL_LIBRARY_H
#define CLOUD_TOOL_LIBRARY_H

#include "Toolpath.h"
#include "MaterialLibrary.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// --------------------------------------------------------------------------
// CloudToolLibrary – Manufacturer-Linked Digital Tool Data
//
// Modern CAM toolpaths are no longer built on guessed feeds and speeds.
// The software connects to cloud repositories (e.g. CoroPlus®, MachiningCloud,
// Kennametal NOVO, Sandvik Coromant) to pull real-world cutting data directly
// from the tool manufacturer.
//
// This module provides:
//
//  1. ToolDigitalTwin – the exact 3-D parametric model of a tool assembly
//     (insert, holder, extension, collet) including all dimensions needed for
//     accurate gouge checking and machine simulation.
//
//  2. ManufacturerCuttingData – manufacturer-validated feeds, speeds, and
//     depth-of-cut recommendations for a specific tool in a specific material.
//
//  3. CloudToolLibrary – in-process cache and query interface.  In a real
//     deployment this class issues HTTPS requests to the vendor API; here it
//     provides a rich in-memory database of typical values plus the full
//     calculation API so downstream toolpath generators can consume the data
//     without depending on network availability.
//
//  4. DynamicArcLimits – the mechanical limits that constrain how aggressively
//     the machine can arc through corners without exceeding the tool's rated
//     load.  Derived from manufacturer data and used by DynamicMotion to
//     compute safe trochoidal and HSM arc radii.
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// ToolDigitalTwin
//
// Represents the exact 3-D model of a complete tool assembly as supplied by
// the manufacturer.  The CAM kernel uses this to:
//   • Display the true tool silhouette in the 3-D viewport
//   • Perform accurate holder/fixture collision detection during 5-axis moves
//   • Report the correct centre-of-gravity for inertia compensation
// --------------------------------------------------------------------------
struct ToolDigitalTwin {
    // Identification
    std::string manufacturerCode;     // e.g. "Sandvik"
    std::string catalogueNumber;      // e.g. "R390-012A20-11M"
    std::string description;          // human-readable name

    // Cutting part (insert or end geometry)
    double  cuttingDiameter     = 12.0;   // mm
    double  cuttingLength       = 25.0;   // mm (flute / effective cutting length)
    double  cornerRadius        = 0.0;    // mm (bull-nose or ball radius)
    int     numberOfFlutes      = 4;
    double  helixAngle          = 35.0;   // degrees
    std::string cuttingMaterial = "Carbide";
    std::string coatingType     = "TiAlN";

    // Holder / shank
    std::string holderType;               // e.g. "SK40", "HSK-A63", "Capto C5"
    double  shankDiameter       = 20.0;   // mm
    double  shankLength         = 50.0;   // mm
    double  overallAssemblyLength = 120.0; // mm (tip to gauge line)

    // Collision envelope (simplified cylinder model)
    // The non-cutting portion (shank + holder) is represented as a cylinder
    // for rapid gouge detection.
    double  holderEnvelopeDiameter = 40.0; // mm – worst-case holder OD
    double  holderEnvelopeLength   = 95.0; // mm – from gauge line to holder top

    // Digital twin source
    std::string sourceUrl;   // original download URL (audit trail)
    std::string checksumSha256; // file integrity checksum
};

// --------------------------------------------------------------------------
// ManufacturerCuttingData
//
// Validated feed / speed / depth-of-cut recommendations from the tool
// manufacturer for a specific material class.  These values represent the
// "sweet spot" the manufacturer has verified in controlled testing.
// --------------------------------------------------------------------------
struct ManufacturerCuttingData {
    std::string catalogueNumber;         // references the ToolDigitalTwin
    MaterialClass materialClass = MaterialClass::Steel;

    // Recommended cutting parameters (use for Ø = nominalDiameter)
    double nominalDiameter      = 12.0;  // mm – tool diameter these values apply to
    double surfaceSpeedMin      = 80.0;  // m/min
    double surfaceSpeedMax      = 200.0; // m/min
    double feedPerToothMin      = 0.02;  // mm/tooth
    double feedPerToothMax      = 0.08;  // mm/tooth
    double maxAxialDepth        = 25.0;  // mm (ae_max)
    double maxRadialDepth       = 6.0;   // mm (ap_max) – fraction × Ø
    double recommendedAxialDepth= 20.0;  // mm – manufacturer "sweet spot"
    double recommendedRadialDepth= 1.8;  // mm – ~15 % of Ø for dynamic milling

    // Dynamic motion arc limits (see DynamicArcLimits below)
    double maxRadialEngagement  = 0.15;  // fraction of Ø – constant chip-load limit
    double trochoidalLoopRadius = 0.3;   // fraction of Ø

    // Coolant
    CuttingParams::Coolant recommendedCoolant = CuttingParams::Coolant::Flood;

    // Strategy flags derived from manufacturer testing
    bool   optimisedForHSM      = false; // High-Speed Machining validated
    bool   optimisedForTrochoidal = false;
    double expectedToolLifeMin  = 0.0;   // minutes (manufacturer rated)
};

// --------------------------------------------------------------------------
// DynamicArcLimits
//
// The mechanical limits that constrain how aggressively the CNC interpolator
// can arc through direction changes without overloading the tool.  Derived
// from manufacturer cutter geometry and the feed/speed result.
//
// The dynamic motion algorithm uses these to choose:
//   • Minimum trochoidal loop radius (so the insert chord never exceeds limit)
//   • Maximum entry arc radius for helical ramps
//   • Cornering velocity cap (to keep centripetal acceleration below tool limit)
// --------------------------------------------------------------------------
struct DynamicArcLimits {
    double minLoopRadius        = 1.0;   // mm – absolute minimum arc radius
    double maxLoopRadius        = 10.0;  // mm – above this, the arc wastes time
    double recommendedLoopRadius= 3.0;   // mm – manufacturer sweet-spot
    double maxCornerVelocityRatio = 0.6; // fraction of programmed feed rate
                                         // to use when cornering

    // Centripetal acceleration limit:
    //   a_c = v² / r  ≤  a_limit
    //   r_min = v² / a_limit
    double centripAccelLimitMpss = 5.0;  // m/s² – from machine / tool rating
};

// --------------------------------------------------------------------------
// CloudToolLibrary
// --------------------------------------------------------------------------
class CloudToolLibrary {
public:
    CloudToolLibrary();

    // -----------------------------------------------------------------------
    // Populate with built-in representative tool data.
    // In a production system this would be refreshed from a cloud endpoint.
    // -----------------------------------------------------------------------
    void loadBuiltinTools();

    // -----------------------------------------------------------------------
    // lookupTool()
    //
    // Retrieve a ToolDigitalTwin by catalogue number.
    // Returns nullptr if the tool is not in the local cache.
    // -----------------------------------------------------------------------
    const ToolDigitalTwin* lookupTool(const std::string& catalogueNumber) const;

    // -----------------------------------------------------------------------
    // lookupCuttingData()
    //
    // Retrieve the manufacturer cutting data for a specific tool + material.
    // Returns nullptr if no record exists.
    // -----------------------------------------------------------------------
    const ManufacturerCuttingData* lookupCuttingData(
        const std::string& catalogueNumber,
        MaterialClass materialClass) const;

    // -----------------------------------------------------------------------
    // getOptimalParameters()
    //
    // Builds a CuttingParams structure using manufacturer-validated data for
    // the given tool and material, scaled to the actual tool diameter.
    //
    // Scaling rules:
    //   RPM        = (surfaceSpeed_m_min × 1000) / (π × Ø_mm)
    //   Feed       = RPM × flutes × feedPerTooth
    //   AxialDepth = recommendedAxialDepth × (Ø / nominalDiameter)   [clipped]
    //   RadialDepth= recommendedRadialDepth × (Ø / nominalDiameter)  [clipped]
    //
    // Falls back to MaterialLibrary defaults if no cloud record is available.
    // -----------------------------------------------------------------------
    CuttingParams getOptimalParameters(const CuttingTool& tool,
                                        MaterialClass materialClass) const;

    // -----------------------------------------------------------------------
    // computeDynamicArcLimits()
    //
    // Derives DynamicArcLimits from:
    //   • Tool geometry (diameter, corner radius, flute count)
    //   • Manufacturer cutting data (chip-load envelope)
    //   • Machine centripetal acceleration rating
    //
    // Formula for minimum loop radius given feed rate v (mm/min):
    //   v_m_s = v / 60000            (mm/min → m/s)
    //   r_min = v_m_s² / a_limit_mps2
    //   r_min_mm = r_min × 1000
    // -----------------------------------------------------------------------
    static DynamicArcLimits computeDynamicArcLimits(
        const CuttingTool& tool,
        const ManufacturerCuttingData& mcd,
        double feedRateMmMin,
        double machineAccelLimitMpss = 5.0);

    // -----------------------------------------------------------------------
    // registerTool() / registerCuttingData()
    //
    // Add custom tool entries (e.g. from a newly downloaded cloud record).
    // -----------------------------------------------------------------------
    void registerTool(ToolDigitalTwin twin);
    void registerCuttingData(ManufacturerCuttingData data);

    // All tools in the cache
    const std::vector<ToolDigitalTwin>& allTools() const { return m_tools; }

private:
    std::vector<ToolDigitalTwin>       m_tools;
    std::vector<ManufacturerCuttingData> m_cuttingData;

    MaterialLibrary m_matLib; // fallback for getOptimalParameters()

    static constexpr double PI_CTL = 3.14159265358979323846;
};

#endif // CLOUD_TOOL_LIBRARY_H
