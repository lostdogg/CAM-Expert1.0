#pragma once
#ifndef INTENT_PARSER_H
#define INTENT_PARSER_H

#include "../cam/Toolpath.h"
#include "../cam/MaterialLibrary.h"
#include <string>
#include <vector>
#include <optional>

// --------------------------------------------------------------------------
// IntentParser – Semantic-to-Geometric Mapping (Step 1: Intent Extraction)
//
// Converts a free-form natural language command entered by the machinist into
// a structured ParsedIntent that the CopilotEngine can act on.
//
// The parser uses keyword matching and simple token analysis to identify:
//   • Action  – what to do (Mill, Drill, Turn, Verify, Optimize, Explain …)
//   • Strategy – how to do it (DynamicMotion, Trochoidal, Raster, Contour …)
//   • Target  – what geometry to act on (SelectedFace, Pocket, Wall, Boss …)
//   • Material – optional material hint extracted from the sentence
//
// By operating entirely locally (no network calls), sensitive part geometry
// is never transmitted off-machine, satisfying the NDA-compliance requirement.
// --------------------------------------------------------------------------

enum class CopilotAction {
    Mill,           // create/modify a milling toolpath
    Drill,          // create/modify a drilling cycle
    Turn,           // create/modify a turning toolpath
    Verify,         // run gouge/undercut simulation
    Optimize,       // suggest cycle-time or feed/speed improvements
    Explain,        // explain current G-code / post-processor output
    Troubleshoot,   // diagnose a detected gouge or simulation error
    Unknown
};

enum class CopilotTarget {
    SelectedFace,   // currently selected face in the viewport
    Pocket,         // a pocket / enclosed floor feature
    Wall,           // vertical wall face
    Boss,           // protruding boss feature
    Slot,           // slot feature
    Hole,           // cylindrical hole
    WholeModel,     // entire solid / stock
    ActiveOperation,// the currently selected toolpath operation
    Unknown
};

struct ParsedIntent {
    CopilotAction            action        = CopilotAction::Unknown;
    CopilotTarget            target        = CopilotTarget::Unknown;
    std::optional<StrategyType>   strategy;          // suggested strategy (if extracted)
    std::optional<MaterialClass>  materialHint;      // material mentioned in command
    std::string              rawCommand;             // original user text
    std::string              normalised;             // lower-cased, trimmed text
    double                   confidence    = 0.0;    // 0..1 – how sure the parser is

    bool isValid() const {
        return action != CopilotAction::Unknown && confidence > 0.3;
    }
};

// --------------------------------------------------------------------------
class IntentParser {
public:
    IntentParser() = default;

    // Parse a free-form natural-language command.
    // Returns a ParsedIntent; check isValid() before acting on it.
    ParsedIntent parse(const std::string& command) const;

private:
    // Sub-classifiers
    CopilotAction  classifyAction  (const std::string& text) const;
    CopilotTarget  classifyTarget  (const std::string& text) const;
    std::optional<StrategyType>  extractStrategy(const std::string& text) const;
    std::optional<MaterialClass> extractMaterial(const std::string& text) const;

    // Utility
    static std::string normalise(const std::string& s);
    static bool        contains (const std::string& haystack,
                                  const std::string& needle);
};

#endif // INTENT_PARSER_H
