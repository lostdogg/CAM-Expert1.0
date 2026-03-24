#include "Backplot.h"
#include <algorithm>

// --------------------------------------------------------------------------
void Backplot::build(const ToolpathManager* mgr) {
    m_moves.clear();
    if (!mgr) return;

    for (int opIdx = 0; opIdx < mgr->count(); ++opIdx) {
        const auto& tp  = mgr->at(opIdx);
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
void Backplot::buildFiltered(const ToolpathManager* mgr, int operationIndex) {
    m_moves.clear();
    if (!mgr) return;
    if (operationIndex < 0 || operationIndex >= mgr->count()) return;

    const auto& tp  = mgr->at(operationIndex);
    const auto& pts = tp.points();
    for (std::size_t i = 1; i < pts.size(); ++i) {
        BackplotMove mv;
        mv.from           = pts[i-1].position;
        mv.to             = pts[i].position;
        mv.motion         = pts[i].motion;
        mv.operationIndex = operationIndex;
        m_moves.push_back(mv);
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
    if (m_currentIdx >= static_cast<int>(m_moves.size()) - 1) return false;
    ++m_currentIdx;
    return true;
}

bool Backplot::stepBack() {
    if (m_currentIdx <= 0) return false;
    --m_currentIdx;
    return true;
}

// --------------------------------------------------------------------------
void Backplot::goToOperation(int operationIndex) {
    for (int i = 0; i < static_cast<int>(m_moves.size()); ++i) {
        if (m_moves[static_cast<std::size_t>(i)].operationIndex == operationIndex) {
            m_currentIdx = i;
            return;
        }
    }
}

void Backplot::goToMove(int moveIndex) {
    m_currentIdx = std::max(0,
        std::min(moveIndex, static_cast<int>(m_moves.size()) - 1));
}

// --------------------------------------------------------------------------
const BackplotMove* Backplot::currentMove() const {
    if (m_currentIdx < 0 || m_currentIdx >= static_cast<int>(m_moves.size()))
        return nullptr;
    return &m_moves[static_cast<std::size_t>(m_currentIdx)];
}

// --------------------------------------------------------------------------
BackplotColor Backplot::motionColor(MotionType mt) {
    switch (mt) {
    case MotionType::Rapid:
    case MotionType::MicroLift:
        return {1.0f, 1.0f, 0.0f};   // yellow – rapid traverse
    case MotionType::PlungeFeed:
        return {1.0f, 0.3f, 0.3f};   // red – plunge
    case MotionType::Retract:
        return {0.3f, 0.5f, 1.0f};   // blue – retract
    case MotionType::ArcCW:
    case MotionType::ArcCCW:
        return {0.8f, 0.4f, 1.0f};   // purple – arc motion
    case MotionType::Dwell:
        return {1.0f, 0.5f, 0.0f};   // orange – dwell
    default:
        return {0.0f, 1.0f, 0.0f};   // green – linear feed
    }
}
