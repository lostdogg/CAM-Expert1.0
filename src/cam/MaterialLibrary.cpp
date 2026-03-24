#include "MaterialLibrary.h"
#include <cmath>
#include <algorithm>
#include <cctype>

// --------------------------------------------------------------------------
// Convert FeedSpeedResult → CuttingParams
// --------------------------------------------------------------------------
CuttingParams FeedSpeedResult::toCuttingParams() const {
    CuttingParams p;
    p.spindleRPM   = spindleRPM;
    p.feedRate     = feedRate;
    p.feedPerTooth = feedPerTooth;
    p.surfaceSpeed = surfaceSpeed;
    p.axialDepth   = axialDepth;
    p.radialDepth  = radialDepth;
    p.coolant      = coolant;
    // plunge rate: 30 % of cutting feed rate as a safe default
    p.plungeRate   = feedRate * 0.30;
    return p;
}

// --------------------------------------------------------------------------
// MaterialLibrary ctor + default library
// --------------------------------------------------------------------------
MaterialLibrary::MaterialLibrary() {
    loadDefaults();
}

void MaterialLibrary::loadDefaults() {
    // -------- Aluminum --------
    {
        MaterialProperties m;
        m.matClass              = MaterialClass::Aluminum;
        m.name                  = "Aluminum";
        m.hardnessBrinell       = 95;
        m.tensileStrengthMPa    = 310;
        m.thermalConductivity   = 167;   // excellent heat conductor
        m.machinabilityIndex    = 3.0;   // very easy to machine
        m.surfaceSpeedMin       = 400;
        m.surfaceSpeedMax       = 1200;
        m.feedPerToothMin       = 0.05;
        m.feedPerToothMax       = 0.20;
        m.preferredCoolant      = CuttingParams::Coolant::Mist;
        // Strategy flags – HSM / high-velocity approach
        m.useHighSpeedMachining      = true;
        m.useTrochoidalPaths         = false;
        m.requiresGCodeSmoothing     = false;
        m.highChipEvacuationPriority = true;
        m.maxRadialEngagement        = 0.70;  // up to 70 % of Ø
        m.minRadialEngagement        = 0.30;
        m.preferredEntry             = MaterialProperties::EntryMethod::HelicalRamp;
        m.repositionStyle            = MaterialProperties::RepositionStyle::HighSpeedAir;
        m_materials.push_back(m);
    }

    // -------- Titanium --------
    {
        MaterialProperties m;
        m.matClass              = MaterialClass::Titanium;
        m.name                  = "Titanium";
        m.hardnessBrinell       = 334;
        m.tensileStrengthMPa    = 950;
        m.thermalConductivity   = 6.7;   // very poor – heat stays at cutting zone
        m.machinabilityIndex    = 0.25;
        m.surfaceSpeedMin       = 40;
        m.surfaceSpeedMax       = 80;
        m.feedPerToothMin       = 0.03;
        m.feedPerToothMax       = 0.07;
        m.preferredCoolant      = CuttingParams::Coolant::Flood;
        // Strategy flags – low-and-slow, trochoidal, smoothing
        m.useHighSpeedMachining      = false;
        m.useTrochoidalPaths         = true;
        m.requiresGCodeSmoothing     = true;
        m.highChipEvacuationPriority = false;
        m.maxRadialEngagement        = 0.10;  // 5–10 % of Ø (arc of contact)
        m.minRadialEngagement        = 0.05;
        m.preferredEntry             = MaterialProperties::EntryMethod::TangentialArc;
        m.repositionStyle            = MaterialProperties::RepositionStyle::ControlledMicroLift;
        m_materials.push_back(m);
    }

    // -------- Mild Steel --------
    {
        MaterialProperties m;
        m.matClass              = MaterialClass::Steel;
        m.name                  = "Steel";
        m.hardnessBrinell       = 180;
        m.tensileStrengthMPa    = 600;
        m.thermalConductivity   = 50;
        m.machinabilityIndex    = 0.65;
        m.surfaceSpeedMin       = 80;
        m.surfaceSpeedMax       = 200;
        m.feedPerToothMin       = 0.03;
        m.feedPerToothMax       = 0.12;
        m.preferredCoolant      = CuttingParams::Coolant::Flood;
        m.useHighSpeedMachining  = false;
        m.useTrochoidalPaths     = false;
        m.maxRadialEngagement    = 0.50;
        m.minRadialEngagement    = 0.10;
        m.preferredEntry         = MaterialProperties::EntryMethod::HelicalRamp;
        m.repositionStyle        = MaterialProperties::RepositionStyle::HighSpeedAir;
        m_materials.push_back(m);
    }

    // -------- Stainless Steel --------
    {
        MaterialProperties m;
        m.matClass              = MaterialClass::StainlessSteel;
        m.name                  = "StainlessSteel";
        m.hardnessBrinell       = 200;
        m.tensileStrengthMPa    = 700;
        m.thermalConductivity   = 16;
        m.machinabilityIndex    = 0.45;
        m.surfaceSpeedMin       = 50;
        m.surfaceSpeedMax       = 150;
        m.feedPerToothMin       = 0.02;
        m.feedPerToothMax       = 0.08;
        m.preferredCoolant      = CuttingParams::Coolant::Flood;
        m.useHighSpeedMachining  = false;
        m.useTrochoidalPaths     = true;
        m.requiresGCodeSmoothing = true;
        m.maxRadialEngagement    = 0.20;
        m.minRadialEngagement    = 0.05;
        m.preferredEntry         = MaterialProperties::EntryMethod::TangentialArc;
        m.repositionStyle        = MaterialProperties::RepositionStyle::ControlledMicroLift;
        m_materials.push_back(m);
    }

    // -------- Inconel / Ni-base superalloy --------
    {
        MaterialProperties m;
        m.matClass              = MaterialClass::Inconel;
        m.name                  = "Inconel";
        m.hardnessBrinell       = 350;
        m.tensileStrengthMPa    = 1200;
        m.thermalConductivity   = 11.4;
        m.machinabilityIndex    = 0.15;
        m.surfaceSpeedMin       = 20;
        m.surfaceSpeedMax       = 50;
        m.feedPerToothMin       = 0.02;
        m.feedPerToothMax       = 0.05;
        m.preferredCoolant      = CuttingParams::Coolant::ThroughTool;
        m.useHighSpeedMachining  = false;
        m.useTrochoidalPaths     = true;
        m.requiresGCodeSmoothing = true;
        m.maxRadialEngagement    = 0.08;
        m.minRadialEngagement    = 0.03;
        m.preferredEntry         = MaterialProperties::EntryMethod::TangentialArc;
        m.repositionStyle        = MaterialProperties::RepositionStyle::ControlledMicroLift;
        m_materials.push_back(m);
    }

    // -------- Cast Iron --------
    {
        MaterialProperties m;
        m.matClass              = MaterialClass::CastIron;
        m.name                  = "CastIron";
        m.hardnessBrinell       = 220;
        m.tensileStrengthMPa    = 250;
        m.thermalConductivity   = 46;
        m.machinabilityIndex    = 0.70;
        m.surfaceSpeedMin       = 100;
        m.surfaceSpeedMax       = 250;
        m.feedPerToothMin       = 0.05;
        m.feedPerToothMax       = 0.15;
        m.preferredCoolant      = CuttingParams::Coolant::Air; // dry preferred
        m.maxRadialEngagement    = 0.40;
        m.minRadialEngagement    = 0.10;
        m.preferredEntry         = MaterialProperties::EntryMethod::HelicalRamp;
        m.repositionStyle        = MaterialProperties::RepositionStyle::HighSpeedAir;
        m_materials.push_back(m);
    }

    // -------- Brass --------
    {
        MaterialProperties m;
        m.matClass              = MaterialClass::Brass;
        m.name                  = "Brass";
        m.hardnessBrinell       = 100;
        m.tensileStrengthMPa    = 400;
        m.thermalConductivity   = 109;
        m.machinabilityIndex    = 2.0;
        m.surfaceSpeedMin       = 200;
        m.surfaceSpeedMax       = 600;
        m.feedPerToothMin       = 0.04;
        m.feedPerToothMax       = 0.15;
        m.preferredCoolant      = CuttingParams::Coolant::Mist;
        m.useHighSpeedMachining  = true;
        m.maxRadialEngagement    = 0.60;
        m.minRadialEngagement    = 0.20;
        m.preferredEntry         = MaterialProperties::EntryMethod::HelicalRamp;
        m.repositionStyle        = MaterialProperties::RepositionStyle::HighSpeedAir;
        m_materials.push_back(m);
    }

    // -------- Plastic --------
    {
        MaterialProperties m;
        m.matClass              = MaterialClass::Plastic;
        m.name                  = "Plastic";
        m.hardnessBrinell       = 20;
        m.tensileStrengthMPa    = 60;
        m.thermalConductivity   = 0.2;   // insulator – heat builds fast
        m.machinabilityIndex    = 4.0;
        m.surfaceSpeedMin       = 200;
        m.surfaceSpeedMax       = 800;
        m.feedPerToothMin       = 0.05;
        m.feedPerToothMax       = 0.20;
        m.preferredCoolant      = CuttingParams::Coolant::Air;
        m.useHighSpeedMachining  = true;
        m.highChipEvacuationPriority = true;
        m.maxRadialEngagement    = 0.60;
        m.minRadialEngagement    = 0.20;
        m.preferredEntry         = MaterialProperties::EntryMethod::HelicalRamp;
        m.repositionStyle        = MaterialProperties::RepositionStyle::HighSpeedAir;
        m_materials.push_back(m);
    }

    // -------- Carbon Fibre (CFRP) --------
    {
        MaterialProperties m;
        m.matClass              = MaterialClass::Carbon_CFRP;
        m.name                  = "CFRP";
        m.hardnessBrinell       = 200;   // highly abrasive
        m.tensileStrengthMPa    = 600;
        m.thermalConductivity   = 5.0;
        m.machinabilityIndex    = 0.60;
        m.surfaceSpeedMin       = 200;
        m.surfaceSpeedMax       = 600;
        m.feedPerToothMin       = 0.05;
        m.feedPerToothMax       = 0.15;
        m.preferredCoolant      = CuttingParams::Coolant::Air;
        m.requiresGCodeSmoothing = true;
        m.maxRadialEngagement    = 0.30;
        m.minRadialEngagement    = 0.10;
        m.preferredEntry         = MaterialProperties::EntryMethod::TangentialArc;
        m.repositionStyle        = MaterialProperties::RepositionStyle::ControlledMicroLift;
        m_materials.push_back(m);
    }
}

// --------------------------------------------------------------------------
const MaterialProperties& MaterialLibrary::get(MaterialClass cls) const {
    for (const auto& m : m_materials)
        if (m.matClass == cls) return m;
    return m_fallback;
}

// --------------------------------------------------------------------------
const MaterialProperties* MaterialLibrary::findByName(const std::string& name) const {
    // Case-insensitive compare
    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        return s;
    };
    std::string needle = toLower(name);
    for (const auto& m : m_materials) {
        if (toLower(m.name) == needle) return &m;
    }
    return nullptr;
}

// --------------------------------------------------------------------------
void MaterialLibrary::addCustom(MaterialProperties mat) {
    mat.matClass = MaterialClass::Custom;
    // Replace if name already exists
    for (auto& m : m_materials) {
        if (m.name == mat.name) { m = std::move(mat); return; }
    }
    m_materials.push_back(std::move(mat));
}

// --------------------------------------------------------------------------
// Feed/Speed Calculator
// --------------------------------------------------------------------------
FeedSpeedResult MaterialLibrary::calculate(const CuttingTool& tool,
                                            const MaterialProperties& mat) const {
    FeedSpeedResult res;

    // ---- Surface speed selection ----
    // For HSM materials (Aluminum) bias toward the maximum.
    // For tough materials (Titanium) stay at the lower end.
    double speedBias = mat.useHighSpeedMachining ? 0.85 : 0.45;
    res.surfaceSpeed = mat.surfaceSpeedMin
                     + speedBias * (mat.surfaceSpeedMax - mat.surfaceSpeedMin);

    // ---- Spindle RPM = (surfaceSpeed * 1000) / (π × Ø) ----
    double rpm = (res.surfaceSpeed * 1000.0)
               / (3.14159265358979 * std::max(0.1, tool.diameter));
    res.spindleRPM = std::round(rpm / 10.0) * 10.0; // round to nearest 10

    // ---- Feed per tooth (scale with tool Ø relative to 12 mm reference) ----
    double diaDiameter = tool.diameter / 12.0;
    double fptBias     = mat.useHighSpeedMachining ? 0.80 : 0.50;
    double fpt = mat.feedPerToothMin
               + fptBias * (mat.feedPerToothMax - mat.feedPerToothMin);
    fpt *= std::sqrt(diaDiameter);  // scale: bigger tool → bigger chip load
    res.feedPerTooth = fpt;

    // ---- Cutting feed rate = fpt × flutes × RPM ----
    res.feedRate = fpt * tool.numFlutes * res.spindleRPM;

    // ---- Axial depth of cut ----
    if (mat.useHighSpeedMachining) {
        // Aluminum: full flute engagement allowed
        res.axialDepth = tool.fluteLength;
    } else if (mat.useTrochoidalPaths) {
        // Titanium / superalloys: limit to ~1.5 × diameter
        res.axialDepth = tool.diameter * 1.5;
    } else {
        res.axialDepth = tool.diameter * 1.0;
    }

    // ---- Radial depth of cut (step-over) ----
    double engFrac = mat.useTrochoidalPaths
                   ? mat.minRadialEngagement      // thin arc-of-contact
                   : (mat.maxRadialEngagement * 0.70); // normal cut
    res.radialDepth           = tool.diameter * engFrac;
    res.recommendedEngagement = engFrac;

    // ---- Strategy flags ----
    res.useHSM          = mat.useHighSpeedMachining;
    res.useTrochoidal   = mat.useTrochoidalPaths;
    res.applySmoothing  = mat.requiresGCodeSmoothing;
    res.entryMethod     = mat.preferredEntry;
    res.repositionMode  = mat.repositionStyle;
    res.coolant         = mat.preferredCoolant;

    return res;
}

// --------------------------------------------------------------------------
FeedSpeedResult MaterialLibrary::calculate(const CuttingTool& tool,
                                            MaterialClass cls) const {
    return calculate(tool, get(cls));
}
