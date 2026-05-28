#pragma once
#ifndef WIRE_EDM_H
#define WIRE_EDM_H

#include "Toolpath.h"
#include "../cad/Geometry.h"
#include "../cad/BRep.h"
#include <vector>
#include <string>

// --------------------------------------------------------------------------
// WireEDM – Wire Electrical Discharge Machining
//
// Wire EDM erodes material by repeatedly sparking between a travelling
// wire electrode and the workpiece.  Unlike milling, there is no physical
// contact: the wire never touches the part.
//
// Supported operations:
//
//  1. 2-Axis cut      – wire runs perfectly vertical; the profile is the
//                       same top and bottom (prismatic shape).
//
//  2. 4-Axis taper    – the upper UV guides and lower XY guides move
//                       independently, producing a tapered or twisted form.
//
//  3. Skim / finish   – after the rough cut, one or more skim passes
//                       bring the wall to final accuracy/finish (the wire
//                       follows the same path offset inward/outward by the
//                       successive skim amounts).
//
//  4. No-core cut     – the slug (core) must fall free at the end so the
//                       wire makes a series of small "nibble" moves into
//                       the contour to break the slug connections.
//
//  5. Reverse skim    – on the final skim the wire travels in the reverse
//                       direction so the exit side of each spark is burnished.
//
//  6. Land / Tab      – a thin bridge of material (land) is intentionally
//                       left uncut to retain the core while the contour is
//                       being roughed.  The tab is cut later (slug relief pass).
//
// Stock recognition:
//   WireEDM::recogniseStock() scans a B-Rep for "through-hole" features
//   (faces whose surface normal is aligned with Z over the entire part height)
//   and returns them as candidate wire-start profiles.  This is the EDM
//   equivalent of mill feature recognition.
//
// Toolpath coordinate convention:
//   • position.x / position.y   – lower (XY) guide position
//   • position.z                 – vertical wire height (lower guide)
//   • toolAxis.x / toolAxis.y   – UV taper offset at the upper guide (mm)
//   • toolAxis.z                 – 1.0 (wire always vertical by default;
//                                  non-zero X/Y in toolAxis indicates taper)
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// WireEDMCutParams – process parameters for one cut pass
// --------------------------------------------------------------------------
struct WireEDMCutParams {
    // Wire properties
    double  wireDiameter   = 0.25;    // mm (typical brass wire)
    double  sparkGap       = 0.01;    // mm – gap between wire and workpiece
    double  kerf           = wireDiameter + 2.0 * sparkGap; // total material removed

    // Cut mode
    enum class CutMode { Rough, Skim, NoCore, ReverseSkimCut } mode = CutMode::Rough;

    // Feed rates
    double  roughFeed      = 2.0;     // mm/min for rough cut
    double  skimFeed       = 0.8;     // mm/min for skim passes
    double  tabFeed        = 0.3;     // mm/min when cutting tab/land

    // Skim offset sequence (inward offsets for successive skim passes, mm)
    std::vector<double> skimOffsets = { 0.08, 0.03 };

    // Flushing
    enum class Flush { Upper, Lower, Both } flushMode = Flush::Both;

    // Taper (4-axis)
    double  taperAngle     = 0.0;     // degrees – positive = upper guide leads
    bool    linearTaper    = true;    // true = constant angle; false = conical

    // Tab / land
    bool    useLand        = false;   // leave a tab to retain core
    double  landWidth      = 1.0;     // mm – width of the retained land
    double  landOffset     = 2.0;     // mm along profile where land is placed
};

// --------------------------------------------------------------------------
// WireEDMThread – the programmed wire start: a pre-drilled entry hole
// --------------------------------------------------------------------------
struct WireEDMThread {
    Geom::Vec2 position;    // XY centre of the thread (start) hole
    double     diameter = 0.5; // mm – hole diameter
    double     z        = 0.0; // Z level (top of part)
};

// --------------------------------------------------------------------------
// WireEDMCutResult – outcome of a cut generation
// --------------------------------------------------------------------------
struct WireEDMCutResult {
    Toolpath        roughPass;              // rough (primary) cut toolpath
    std::vector<Toolpath> skimPasses;       // skim pass toolpaths (one per offset)
    Toolpath        tabRelief;              // optional slug-relief pass to cut tabs
    bool            coreWillFall   = false; // whether the slug drops freely
    double          estimatedTimeMin = 0.0; // minutes (rough only)
    std::string     warnings;               // non-fatal diagnostic text
};

// --------------------------------------------------------------------------
// EdmStockFeature – one candidate cut profile found by recogniseStock()
// --------------------------------------------------------------------------
struct EdmStockFeature {
    std::vector<Geom::Vec2> profile; // 2-D boundary of the through-feature
    double                  topZ;    // top Z of the through-hole
    double                  botZ;    // bottom Z of the through-hole
    bool                    isPocket = false; // true if it's a blind pocket
};

// --------------------------------------------------------------------------
// WireEDM
// --------------------------------------------------------------------------
class WireEDM {
public:
    explicit WireEDM(WireEDMCutParams params = {});

    // -----------------------------------------------------------------------
    // cut2Axis()
    //
    // Generates a vertical-wire cut of a 2-D profile.
    // The profile must be a closed chain in XY; the wire travels from
    // threadHole.position outward to the start of the profile then follows
    // the chain (climb or conventional as specified).
    //
    // Returns a WireEDMCutResult containing the rough pass and any skim
    // passes defined in params.skimOffsets.
    // -----------------------------------------------------------------------
    WireEDMCutResult cut2Axis(const std::vector<Geom::Vec2>& profile,
                               double                         partHeight,
                               const WireEDMThread&           thread) const;

    // -----------------------------------------------------------------------
    // cut4Axis()
    //
    // Generates a 4-axis taper cut.  The lower guide follows `lowerProfile`
    // and the upper guide follows `upperProfile`.  Both chains must have the
    // same number of vertices so they can be interpolated point-by-point.
    //
    // Each ToolpathPoint.toolAxis encodes the upper-guide UV offset:
    //   toolAxis = upperPos - lowerPos   (NOT normalised; Z = partHeight)
    // -----------------------------------------------------------------------
    WireEDMCutResult cut4Axis(const std::vector<Geom::Vec2>& lowerProfile,
                               const std::vector<Geom::Vec2>& upperProfile,
                               double                         partHeight,
                               const WireEDMThread&           thread) const;

    // -----------------------------------------------------------------------
    // noCoreCut()
    //
    // Generates a no-core roughing strategy for a closed internal pocket.
    // The wire spirals inward (or uses a zig-zag) to remove the entire slug
    // without leaving a separate core to be ejected.  Suitable for small
    // internal shapes where ejecting a slug is impractical.
    //
    // The generated toolpath includes automatic wire repositions at the end
    // of each "nibble" segment.
    // -----------------------------------------------------------------------
    WireEDMCutResult noCoreCut(const std::vector<Geom::Vec2>& profile,
                                double                         partHeight,
                                const WireEDMThread&           thread) const;

    // -----------------------------------------------------------------------
    // addSkimPasses()
    //
    // Appends skim passes to an existing WireEDMCutResult.  Each pass offsets
    // the original rough profile inward by the corresponding skimOffsets entry
    // and runs at skimFeed.  The last skim is run in reverse if mode ==
    // ReverseSkimCut.
    // -----------------------------------------------------------------------
    static void addSkimPasses(WireEDMCutResult&              result,
                               const std::vector<Geom::Vec2>& baseProfile,
                               double                         partHeight,
                               const WireEDMCutParams&        params);

    // -----------------------------------------------------------------------
    // recogniseStock()
    //
    // Scans a B-Rep model for through-features (closed vertical walls) and
    // returns them as candidate EDM profiles.  This is used to auto-populate
    // the cut profile from the CAD model so the programmer does not need to
    // manually digitise the profile.
    // -----------------------------------------------------------------------
    static std::vector<EdmStockFeature>
        recogniseStock(const BRep::Solid& model, double zTolerance = 0.01);

    // -----------------------------------------------------------------------
    // landProfile()
    //
    // Modifies a closed profile to include a land (tab) of width landWidth
    // centred at landOffset distance along the profile perimeter.  The
    // modified profile is broken into two open chains: before and after the
    // land.  The tab itself is left un-cut during the rough pass and is
    // removed in the returned tabRelief toolpath.
    // -----------------------------------------------------------------------
    static std::vector<std::vector<Geom::Vec2>>
        landProfile(const std::vector<Geom::Vec2>& profile,
                    double                         landWidth,
                    double                         landOffset);

    // -----------------------------------------------------------------------
    // estimateCutTime()
    //
    // Returns estimated cut time in minutes for a given profile perimeter,
    // part height, and wire feed rate.
    //
    //   perimeter = Σ |p[i+1] - p[i]|   (mm)
    //   cuts_per_mm = partHeight / (roughFeed minutes/mm)
    //   time = perimeter / roughFeed
    // -----------------------------------------------------------------------
    static double estimateCutTime(double perimeterMm,
                                   double partHeightMm,
                                   double feedRateMmMin);

    const WireEDMCutParams& params() const { return m_params; }
    void setParams(const WireEDMCutParams& p) { m_params = p; }

private:
    // Build a single open-wire pass at constant Z (upper + lower at same XY)
    static Toolpath buildSingleAxisPass(const std::vector<Geom::Vec2>& profile,
                                         double z,
                                         MotionType motion,
                                         double     feedRate,
                                         const std::string& name);

    // Build an offset of a 2-D profile (inward or outward)
    static std::vector<Geom::Vec2>
        offsetProfile(const std::vector<Geom::Vec2>& profile, double amount);

    // Compute the signed 2-D perimeter of a profile (+ = CCW, − = CW)
    static double signedPerimeter(const std::vector<Geom::Vec2>& profile);

    WireEDMCutParams m_params;
};

#endif // WIRE_EDM_H
