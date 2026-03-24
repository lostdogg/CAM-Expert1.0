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
//   • Rapid   → yellow
//   • Feed    → green
//   • Plunge  → red
//   • Retract → blue
//   • Arc     → purple
//
// The backplot can be stepped forward/backward by move or by operation, and
// supports variable playback speed. Use filterByOperation() to isolate one
// operation's moves for detailed inspection.
// --------------------------------------------------------------------------

struct BackplotMove {
    Geom::Vec3 from;
    Geom::Vec3 to;
    MotionType motion;
    int        operationIndex = 0;
};

// RGB colour [0,1] for a motion type – matches the viewport colour convention
struct BackplotColor { float r, g, b; };

class Backplot {
public:
    Backplot() = default;

    // Build the backplot move list from all operations in the manager
    void build(const ToolpathManager* mgr);

    // Build moves for a single operation only
    void buildFiltered(const ToolpathManager* mgr, int operationIndex);

    // Run/animate: calls onMove for each move in order
    using MoveCallback = std::function<void(const BackplotMove&)>;
    void run(const ToolpathManager* mgr, MoveCallback onMove = {});

    // Step controls
    void  reset();
    bool  step();        // advance one move; returns false when at the last move
    bool  stepBack();    // retreat one move; returns false when at the beginning

    // Jump to the first move of a specific operation
    void  goToOperation(int operationIndex);

    // Jump to a specific move index (clamped to valid range)
    void  goToMove(int moveIndex);

    const BackplotMove* currentMove() const;

    int   totalMoves()   const { return static_cast<int>(m_moves.size()); }
    int   currentIndex() const { return m_currentIdx; }

    // Return the display colour for a given motion type
    static BackplotColor motionColor(MotionType mt);

    double playbackSpeed = 1.0;  // 1.0 = real-time, 2.0 = double speed

private:
    std::vector<BackplotMove> m_moves;
    int                       m_currentIdx = 0;
};

#endif // BACKPLOT_H
