#pragma once
#ifndef NCI_FORMAT_H
#define NCI_FORMAT_H

#include "Toolpath.h"
#include <string>
#include <vector>
#include <sstream>

// --------------------------------------------------------------------------
// NCI (Numerical Control Intermediate) Format
//
// NCI is the proprietary intermediate data format used internally.
// It captures all tool-tip positions, tool-axis vectors, feed/speed settings,
// and motion codes before the Post-Processor translates them to G-code.
//
// NCI record types (simplified):
//   0  = Tool path start / new operation
//   1  = Linear feed move
//   2  = Circular arc CW
//   3  = Circular arc CCW
//   4  = Dwell
//   5  = Tool change
//   14 = Tool axis vector (5-axis)
//   1000 = Spindle speed
//   1001 = Feed rate
//   99 = End of operation
// --------------------------------------------------------------------------

struct NciRecord {
    int    code  = 1;     // NCI motion code
    double x = 0, y = 0, z = 0;
    double i = 0, j = 0, k = 1; // tool axis or arc centre
    double feedRate   = 0;
    double spindleRPM = 0;
    bool   isRapid    = false;
};

class NciFormat {
public:
    // Serialize a Toolpath to NCI text
    static std::string serialize(const Toolpath& tp);

    // Serialize multiple toolpaths (full job)
    static std::string serializeAll(const std::vector<Toolpath>& toolpaths);

    // Parse NCI text back into a list of records (for post-processing)
    static std::vector<NciRecord> parse(const std::string& nciText);

    // Write NCI to file
    static bool writeFile(const std::string& filePath, const Toolpath& tp);
    static bool writeFile(const std::string& filePath,
                           const std::vector<Toolpath>& tps);

private:
    static int motionToCode(MotionType m);
};

#endif // NCI_FORMAT_H
