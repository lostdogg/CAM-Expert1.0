#include "Backplot.h"

// --------------------------------------------------------------------------
void Backplot::build(const ToolpathManager* mgr) {
    m_moves.clear();
    if (!mgr) return;

    for (int opIdx = 0; opIdx < mgr->count(); ++opIdx) {
        const auto& tp = mgr->at(opIdx);
        const auto& pts = tp.points();
        for (std::size_t i = 1; i < pts.size(); ++i) {
            BackplotMove mv;
            mv.from           = pts[i-1].position;
            mv.to             = pts[i].position;
            mv.motion         = pts[i].motion;
            mv.operationIndex = opIdx;
            m_moves.push_back(mv);
        }
    }
    m_currentIdx = 0;
}

// --------------------------------------------------------------------------
void Backplot::run(const ToolpathManager* mgr, MoveCallback onMove) {
    build(mgr);
    for (const auto& mv : m_moves)
        if (onMove) onMove(mv);
}

// --------------------------------------------------------------------------
void Backplot::reset() { m_currentIdx = 0; }

bool Backplot::step() {
    if (m_currentIdx >= static_cast<int>(m_moves.size())) return false;
    ++m_currentIdx;
    return m_currentIdx < static_cast<int>(m_moves.size());
}

const BackplotMove* Backplot::currentMove() const {
    if (m_currentIdx < 0 || m_currentIdx >= static_cast<int>(m_moves.size()))
        return nullptr;
    return &m_moves[static_cast<std::size_t>(m_currentIdx)];
}
