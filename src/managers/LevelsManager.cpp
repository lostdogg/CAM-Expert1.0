#include "LevelsManager.h"
#include <algorithm>

LevelsManager::LevelsManager() {
    // Always start with a default level 1
    Level def;
    def.id          = 1;
    def.name        = "Default";
    def.visible     = true;
    def.locked      = false;
    def.color       = "#FFFFFF";
    m_levels.push_back(def);
    m_nextId = 2;
}

int LevelsManager::addLevel(const std::string& name) {
    Level lv;
    lv.id    = nextId();
    lv.name  = name;
    m_levels.push_back(lv);
    notify();
    return lv.id;
}

void LevelsManager::removeLevel(int levelId) {
    m_levels.erase(
        std::remove_if(m_levels.begin(), m_levels.end(),
            [levelId](const Level& l){ return l.id == levelId; }),
        m_levels.end());
    notify();
}

void LevelsManager::clear() {
    m_levels.clear();
    m_nextId = 1;
    // Restore default level
    Level def;
    def.id = 1; def.name = "Default";
    m_levels.push_back(def);
    m_nextId = 2;
    notify();
}

Level* LevelsManager::findLevel(int levelId) {
    for (auto& lv : m_levels)
        if (lv.id == levelId) return &lv;
    return nullptr;
}

const Level* LevelsManager::findLevel(int levelId) const {
    for (const auto& lv : m_levels)
        if (lv.id == levelId) return &lv;
    return nullptr;
}

void LevelsManager::setVisible(int levelId, bool vis) {
    if (auto* lv = findLevel(levelId)) { lv->visible = vis; notify(); }
}

void LevelsManager::setLocked(int levelId, bool locked) {
    if (auto* lv = findLevel(levelId)) { lv->locked = locked; notify(); }
}

void LevelsManager::setColor(int levelId, const std::string& hex) {
    if (auto* lv = findLevel(levelId)) { lv->color = hex; notify(); }
}

int LevelsManager::count() const { return static_cast<int>(m_levels.size()); }

int  LevelsManager::nextId() { return m_nextId++; }
void LevelsManager::notify() { if (m_onChange) m_onChange(); }
