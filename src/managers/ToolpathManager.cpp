#include "ToolpathManager.h"
#include <stdexcept>
#include <numeric>

// --------------------------------------------------------------------------
void ToolpathManager::addToolpath(Toolpath tp) {
    m_toolpaths.push_back(std::move(tp));
    notifyChange();
}

void ToolpathManager::insertToolpath(int index, Toolpath tp) {
    if (index < 0 || index > static_cast<int>(m_toolpaths.size()))
        index = static_cast<int>(m_toolpaths.size());
    m_toolpaths.insert(m_toolpaths.begin() + index, std::move(tp));
    notifyChange();
}

void ToolpathManager::removeToolpath(int index) {
    if (index < 0 || index >= static_cast<int>(m_toolpaths.size())) return;
    m_toolpaths.erase(m_toolpaths.begin() + index);
    if (m_selectedIndex >= static_cast<int>(m_toolpaths.size()))
        m_selectedIndex = static_cast<int>(m_toolpaths.size()) - 1;
    notifyChange();
}

void ToolpathManager::moveUp(int index) {
    if (index <= 0 || index >= static_cast<int>(m_toolpaths.size())) return;
    std::swap(m_toolpaths[index], m_toolpaths[index - 1]);
    notifyChange();
}

void ToolpathManager::moveDown(int index) {
    if (index < 0 || index >= static_cast<int>(m_toolpaths.size()) - 1) return;
    std::swap(m_toolpaths[index], m_toolpaths[index + 1]);
    notifyChange();
}

void ToolpathManager::clear() {
    m_toolpaths.clear();
    m_selectedIndex     = -1;
    m_stockValidThrough = -1;
    notifyChange();
}

// --------------------------------------------------------------------------
int ToolpathManager::count() const {
    return static_cast<int>(m_toolpaths.size());
}

const Toolpath& ToolpathManager::at(int index) const {
    return m_toolpaths.at(static_cast<std::size_t>(index));
}

Toolpath& ToolpathManager::at(int index) {
    return m_toolpaths.at(static_cast<std::size_t>(index));
}

// --------------------------------------------------------------------------
void ToolpathManager::select(int index) {
    if (index >= 0 && index < static_cast<int>(m_toolpaths.size()))
        m_selectedIndex = index;
}

void ToolpathManager::deselect() {
    m_selectedIndex = -1;
}

// --------------------------------------------------------------------------
void ToolpathManager::regenerate(int index) {
    if (index < 0 || index >= static_cast<int>(m_toolpaths.size())) return;
    // Mark clean – real regeneration would re-run the strategy algorithm
    m_toolpaths[static_cast<std::size_t>(index)].markClean();
    notifyChange();
}

void ToolpathManager::regenerateAll() {
    for (auto& tp : m_toolpaths)
        if (tp.isDirty())
            tp.markClean();
    notifyChange();
}

// --------------------------------------------------------------------------
double ToolpathManager::totalMachiningTime() const {
    double total = 0;
    for (const auto& tp : m_toolpaths)
        total += tp.estimatedTime();
    return total;
}

double ToolpathManager::totalPathLength() const {
    double total = 0;
    for (const auto& tp : m_toolpaths)
        total += tp.totalLength();
    return total;
}

// --------------------------------------------------------------------------
void ToolpathManager::notifyChange() {
    if (m_onChange) m_onChange();
}
