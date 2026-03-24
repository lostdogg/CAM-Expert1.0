#pragma once
#ifndef BACKPLOT_H
#define BACKPLOT_H

#include "../cam/Toolpath.h"
#include "../managers/ToolpathManager.h"
#include <functional>
#include <vector>

// --------------------------------------------------------------------------
// Backplot
//
// Fast wireframe animation of tool motion. For each motion segment the tool
// tip traces a coloured line:
//   • Rapid  → yellow
//   • Feed   → green
//   • Plunge → red
//   • Retract→ blue
//
// The backplot can be stepped forward/backward by operation or by individual
// move, and supports variable playback speed.
// --------------------------------------------------------------------------

struct BackplotMove {
    Geom::Vec3 from;
    Geom::Vec3 to;
    MotionType motion;
    int        operationIndex = 0;
};

class Backplot {
public:
    Backplot() = default;

    // Build the backplot move list from a manager's operations
    void build(const ToolpathManager* mgr);

    // Run/animate: calls onMove for each move in order
    using MoveCallback = std::function<void(const BackplotMove&)>;
    void run(const ToolpathManager* mgr, MoveCallback onMove = {});

    // Step controls
    void  reset();
    bool  step();        // advance one move; returns false when done
    const BackplotMove* currentMove() const;

    int   totalMoves()   const { return static_cast<int>(m_moves.size()); }
    int   currentIndex() const { return m_currentIdx; }

    double playbackSpeed = 1.0;  // 1.0 = real-time, 2.0 = double speed

private:
    std::vector<BackplotMove> m_moves;
    int                       m_currentIdx = 0;
};

#endif // BACKPLOT_H
