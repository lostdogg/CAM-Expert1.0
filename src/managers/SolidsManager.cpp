#include "SolidsManager.h"

void SolidsManager::addSolid(BRep::Solid solid, const std::string& name) {
    SolidEntry e;
    if (!name.empty()) solid.setName(name);
    e.solid = std::move(solid);
    m_entries.push_back(std::move(e));
    notify();
}

void SolidsManager::removeSolid(int index) {
    if (index < 0 || index >= static_cast<int>(m_entries.size())) return;
    m_entries.erase(m_entries.begin() + index);
    notify();
}

void SolidsManager::clear() {
    m_entries.clear();
    notify();
}

int SolidsManager::count() const { return static_cast<int>(m_entries.size()); }

SolidEntry& SolidsManager::at(int i) {
    return m_entries.at(static_cast<std::size_t>(i));
}

const SolidEntry& SolidsManager::at(int i) const {
    return m_entries.at(static_cast<std::size_t>(i));
}

void SolidsManager::setVisible(int index, bool visible) {
    if (index < 0 || index >= static_cast<int>(m_entries.size())) return;
    m_entries[static_cast<std::size_t>(index)].visible = visible;
    notify();
}

void SolidsManager::setSelected(int index, bool selected) {
    if (index < 0 || index >= static_cast<int>(m_entries.size())) return;
    m_entries[static_cast<std::size_t>(index)].selected = selected;
}

void SolidsManager::clearSelection() {
    for (auto& e : m_entries) e.selected = false;
}

std::vector<int> SolidsManager::selectedIndices() const {
    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i)
        if (m_entries[static_cast<std::size_t>(i)].selected)
            result.push_back(i);
    return result;
}

void SolidsManager::notify() { if (m_onChange) m_onChange(); }
