#include "Turning.h"
#include <cmath>
#include <algorithm>

// --------------------------------------------------------------------------
// Rough turning: peel off concentric material passes
// --------------------------------------------------------------------------
Toolpath Turning::roughTurn(const TurningParams& p,
                              const CuttingTool& tool,
                              const CuttingParams& cuts) {
    Toolpath tp(StrategyType::RoughTurning, tool, cuts);
    tp.setName("Rough Turn");

    double clearX  = p.stockDiameter * 0.5 + p.safeRadius;
    double clearZ  = p.safeRadius;
    double targetR = p.finishedDiameter * 0.5 + p.stockAllowance;
    int    passes  = static_cast<int>(
                         std::ceil((p.stockDiameter * 0.5 - targetR) / p.depthOfCut));

    // Rapid to safe position
    ToolpathPoint safe;
    safe.position = {clearZ, clearX, 0};  // (Z, X in turning = radius, unused Y)
    safe.toolAxis = {1, 0, 0};            // tool axis along Z for turning
    safe.motion   = MotionType::Rapid;
    tp.addPoint(safe);

    for (int pass = 0; pass < passes; ++pass) {
        double radius = p.stockDiameter * 0.5 - (pass + 1) * p.depthOfCut;
        if (radius < targetR) radius = targetR;

        // Rapid to start of pass (clearance in Z, cutting radius in X)
        ToolpathPoint approach;
        approach.position = {clearZ, radius, 0};
        approach.toolAxis = {1, 0, 0};
        approach.motion   = MotionType::Rapid;
        tp.addPoint(approach);

        // Feed in Z direction across part length
        ToolpathPoint cut;
        cut.position = {-p.partLength, radius, 0};
        cut.toolAxis = {1, 0, 0};
        cut.motion   = MotionType::Linear;
        tp.addPoint(cut);

        // Retract in X
        ToolpathPoint retractX;
        retractX.position = {-p.partLength, clearX, 0};
        retractX.toolAxis = {1, 0, 0};
        retractX.motion   = MotionType::Rapid;
        tp.addPoint(retractX);
    }

    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
// Finish turning: follow a 2-D profile (Z, radius) at the finished surface
// --------------------------------------------------------------------------
Toolpath Turning::finishTurn(const std::vector<Geom::Vec2>& profile,
                               const TurningParams& p,
                               const CuttingTool& tool,
                               const CuttingParams& cuts) {
    Toolpath tp(StrategyType::FinishTurning, tool, cuts);
    tp.setName("Finish Turn");

    if (profile.empty()) { tp.markClean(); return tp; }

    double clearX = p.stockDiameter * 0.5 + p.safeRadius;
    double clearZ = p.safeRadius;

    // Rapid to start
    ToolpathPoint rapid;
    rapid.position = {clearZ, clearX, 0};
    rapid.toolAxis = {1, 0, 0};
    rapid.motion   = MotionType::Rapid;
    tp.addPoint(rapid);

    // Approach first point
    ToolpathPoint approach;
    approach.position = {profile[0].x, profile[0].y, 0};
    approach.toolAxis = {1, 0, 0};
    approach.motion   = MotionType::Rapid;
    tp.addPoint(approach);

    // Follow profile
    for (const auto& pt : profile) {
        ToolpathPoint tpt;
        tpt.position = {pt.x, pt.y, 0};
        tpt.toolAxis = {1, 0, 0};
        tpt.motion   = MotionType::Linear;
        tp.addPoint(tpt);
    }

    // Retract
    ToolpathPoint ret;
    ret.position = {clearZ, clearX, 0};
    ret.toolAxis = {1, 0, 0};
    ret.motion   = MotionType::Rapid;
    tp.addPoint(ret);

    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
// Groove turning (peck groove cycle)
// --------------------------------------------------------------------------
Toolpath Turning::groove(const GrooveParams& p,
                          const CuttingTool& tool,
                          const CuttingParams& cuts) {
    Toolpath tp(StrategyType::Grooving, tool, cuts);
    tp.setName("Groove Turn");

    double targetR = p.centreDiameter * 0.5 - p.depth;
    double startR  = p.centreDiameter * 0.5;

    // Rapid to groove position
    ToolpathPoint rapid;
    rapid.position = {0, startR + 5.0, 0};
    rapid.toolAxis = {1, 0, 0};
    rapid.motion   = MotionType::Rapid;
    tp.addPoint(rapid);

    // Peck groove
    double currentR = startR;
    while (currentR > targetR) {
        currentR -= p.peckDepth;
        if (currentR < targetR) currentR = targetR;

        ToolpathPoint peck;
        peck.position = {0, currentR, 0};
        peck.toolAxis = {1, 0, 0};
        peck.motion   = MotionType::PlungeFeed;
        tp.addPoint(peck);

        // Retract
        ToolpathPoint ret;
        ret.position = {0, startR, 0};
        ret.toolAxis = {1, 0, 0};
        ret.motion   = MotionType::Rapid;
        tp.addPoint(ret);
    }

    // Widen groove by shifting tool sideways
    for (double dz : {-p.width / 2.0, p.width / 2.0}) {
        ToolpathPoint move;
        move.position = {dz, targetR, 0};
        move.toolAxis = {1, 0, 0};
        move.motion   = MotionType::Linear;
        tp.addPoint(move);

        ToolpathPoint ret;
        ret.position = {dz, startR, 0};
        ret.toolAxis = {1, 0, 0};
        ret.motion   = MotionType::Rapid;
        tp.addPoint(ret);
    }

    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
// Thread turning: generate synchronized multi-pass thread cycle
// --------------------------------------------------------------------------
Toolpath Turning::thread(const ThreadParams& p,
                          const CuttingTool& tool,
                          const CuttingParams& cuts) {
    Toolpath tp(StrategyType::ThreadTurning, tool, cuts);
    tp.setName("Thread Turn");

    double majorR = p.majorDiameter * 0.5;
    double minorR = majorR - p.threadDepth;
    int    passes = 6; // multiple infeed passes for thread
    double safeR  = majorR + 5.0;

    for (int pass = 0; pass < passes + p.springPasses; ++pass) {
        double t = std::min(1.0, static_cast<double>(pass) / passes);
        double infeed = minorR + (1.0 - t) * p.threadDepth;

        // Rapid to start
        ToolpathPoint rapid;
        rapid.position = {p.pitch, infeed, 0};  // Z = 1 thread pitch lead-in
        rapid.toolAxis = {1, 0, 0};
        rapid.motion   = MotionType::Rapid;
        tp.addPoint(rapid);

        // Synchronized threading move (G32 / G76)
        ToolpathPoint cut;
        cut.position = {-(p.length + p.pitch), infeed, 0}; // thread + lead-out
        cut.toolAxis = {1, 0, 0};
        cut.motion   = MotionType::Linear;
        cut.feedOverride = p.pitch / cuts.feedRate; // feed = pitch
        tp.addPoint(cut);

        // Retract
        ToolpathPoint ret;
        ret.position = {p.pitch, safeR, 0};
        ret.toolAxis = {1, 0, 0};
        ret.motion   = MotionType::Rapid;
        tp.addPoint(ret);
    }

    tp.markClean();
    return tp;
}

// --------------------------------------------------------------------------
// Sub-spindle transfer synchronization point
// --------------------------------------------------------------------------
Toolpath Turning::subSpindleTransfer(double transferZ,
                                      double spindleRPM,
                                      const CuttingTool& tool) {
    CuttingParams cuts;
    cuts.spindleRPM = spindleRPM;
    Toolpath tp(StrategyType::Custom, tool, cuts);
    tp.setName("Sub-Spindle Transfer");

    // Move to transfer position at controlled feed
    ToolpathPoint xferPt;
    xferPt.position = {transferZ, 0, 0};
    xferPt.toolAxis = {1, 0, 0};
    xferPt.motion   = MotionType::Linear;
    tp.addPoint(xferPt);

    tp.markClean();
    return tp;
}
