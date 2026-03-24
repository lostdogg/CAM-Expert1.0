#pragma once
#ifndef LEVELS_MANAGER_H
#define LEVELS_MANAGER_H

#include <string>
#include <vector>
#include <functional>

// --------------------------------------------------------------------------
// LevelsManager  (equivalent to "Layers" in other CAD tools)
//
// Each level holds a named group of geometry entities. You can toggle the
// visibility of entire levels to simplify complex files or expose only the
// geometry relevant to the current machining operation.
// --------------------------------------------------------------------------

struct Level {
    int         id      = 0;
    std::string name;
    bool        visible = true;
    bool        locked  = false;
    std::string color   = "#FFFFFF";    // hex display colour
    std::string description;
};

class LevelsManager {
public:
    LevelsManager();

    int  addLevel(const std::string& name);
    void removeLevel(int levelId);
    void clear();

    Level*       findLevel(int levelId);
    const Level* findLevel(int levelId) const;

    void setVisible(int levelId, bool vis);
    void setLocked(int levelId, bool locked);
    void setColor(int levelId, const std::string& hex);

    int                      count()  const;
    const std::vector<Level>& levels() const { return m_levels; }

    using ChangeCallback = std::function<void()>;
    void setOnChange(ChangeCallback cb) { m_onChange = std::move(cb); }

private:
    void notify();
    int  nextId();

    std::vector<Level> m_levels;
    int                m_nextId  = 1;
    ChangeCallback     m_onChange;
};

#endif // LEVELS_MANAGER_H
