#include "SolidsManager.h"

// --------------------------------------------------------------------------
// Internal helpers
// --------------------------------------------------------------------------
bool SolidsManager::validSolidIndex(int i) const {
    return i >= 0 && i < static_cast<int>(m_entries.size());
}

bool SolidsManager::validFeatureIndex(int solidIndex, int featureIndex) const {
    if (!validSolidIndex(solidIndex)) return false;
    const auto& hist = m_entries[static_cast<std::size_t>(solidIndex)].history;
    return featureIndex >= 0 && featureIndex < static_cast<int>(hist.size());
}

// --------------------------------------------------------------------------
void SolidsManager::addSolid(BRep::Solid solid, const std::string& name) {
    SolidEntry e;
    if (!name.empty()) solid.setName(name);
    e.solid = std::move(solid);
    m_entries.push_back(std::move(e));
    notify();
}

void SolidsManager::removeSolid(int index) {
    if (!validSolidIndex(index)) return;
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
    if (!validSolidIndex(index)) return;
    m_entries[static_cast<std::size_t>(index)].visible = visible;
    notify();
}

void SolidsManager::setSelected(int index, bool selected) {
    if (!validSolidIndex(index)) return;
    m_entries[static_cast<std::size_t>(index)].selected = selected;
}

void SolidsManager::clearSelection() {
    for (auto& e : m_entries) e.selected = false;
}

void SolidsManager::renameSolid(int index, const std::string& name) {
    if (!validSolidIndex(index)) return;
    m_entries[static_cast<std::size_t>(index)].solid.setName(name);
    notify();
}

void SolidsManager::setLayer(int index, const std::string& layer) {
    if (!validSolidIndex(index)) return;
    m_entries[static_cast<std::size_t>(index)].layer = layer;
    notify();
}

std::vector<int> SolidsManager::indicesOnLayer(const std::string& layer) const {
    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i)
        if (m_entries[static_cast<std::size_t>(i)].layer == layer)
            result.push_back(i);
    return result;
}

Geom::AABB SolidsManager::aggregateBoundingBox() const {
    Geom::AABB box;
    for (const auto& e : m_entries) {
        if (!e.visible) continue;
        Geom::AABB sb = e.solid.boundingBox();
        if (sb.isValid()) {
            box.expand(sb.min);
            box.expand(sb.max);
        }
    }
    return box;
}

std::vector<int> SolidsManager::selectedIndices() const {
    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i)
        if (m_entries[static_cast<std::size_t>(i)].selected)
            result.push_back(i);
    return result;
}

// --------------------------------------------------------------------------
// Feature / history-tree management
// --------------------------------------------------------------------------

int SolidsManager::addFeature(int solidIndex, const FeatureOp& op) {
    if (!validSolidIndex(solidIndex)) return -1;
    auto& hist = m_entries[static_cast<std::size_t>(solidIndex)].history;
    hist.push_back(op);
    notify();
    return static_cast<int>(hist.size()) - 1;
}

void SolidsManager::suppressFeature(int solidIndex, int featureIndex, bool suppressed) {
    if (!validFeatureIndex(solidIndex, featureIndex)) return;
    m_entries[static_cast<std::size_t>(solidIndex)]
             .history[static_cast<std::size_t>(featureIndex)]
             .suppressed = suppressed;
    notify();
}

bool SolidsManager::editFeature(int solidIndex, int featureIndex, const FeatureOp& op) {
    if (!validFeatureIndex(solidIndex, featureIndex)) return false;
    m_entries[static_cast<std::size_t>(solidIndex)]
             .history[static_cast<std::size_t>(featureIndex)] = op;
    notify();
    return true;
}

void SolidsManager::removeFeature(int solidIndex, int featureIndex) {
    if (!validFeatureIndex(solidIndex, featureIndex)) return;
    auto& hist = m_entries[static_cast<std::size_t>(solidIndex)].history;
    hist.erase(hist.begin() + featureIndex);
    notify();
}

int SolidsManager::featureCount(int solidIndex) const {
    if (!validSolidIndex(solidIndex)) return 0;
    return static_cast<int>(
        m_entries[static_cast<std::size_t>(solidIndex)].history.size());
}

const FeatureOp& SolidsManager::getFeature(int solidIndex, int featureIndex) const {
    return m_entries.at(static_cast<std::size_t>(solidIndex))
                    .history.at(static_cast<std::size_t>(featureIndex));
}

FeatureOp& SolidsManager::getFeature(int solidIndex, int featureIndex) {
    return m_entries.at(static_cast<std::size_t>(solidIndex))
                    .history.at(static_cast<std::size_t>(featureIndex));
}

std::vector<int> SolidsManager::solidsDrivenBy(int wfEntityIndex) const {
    std::vector<int> result;
    for (int si = 0; si < static_cast<int>(m_entries.size()); ++si) {
        const auto& hist = m_entries[static_cast<std::size_t>(si)].history;
        for (const auto& op : hist) {
            if (op.wfChainIdx == wfEntityIndex) {
                result.push_back(si);
                break;
            }
        }
    }
    return result;
}

void SolidsManager::notify() { if (m_onChange) m_onChange(); }
