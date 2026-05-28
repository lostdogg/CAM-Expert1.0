#pragma once
#ifndef ZMAP_H
#define ZMAP_H

#include <vector>

// --------------------------------------------------------------------------
// ZMap – Z-map (dexel) stock representation
//
// Shared between the simulation (Verify) and CAM (Art) modules so that
// neither module needs to include headers from the other.
// --------------------------------------------------------------------------
struct ZMap {
    double xMin = 0, xMax = 0, yMin = 0, yMax = 0;
    int    xRes = 0, yRes = 0;
    std::vector<double> heights;

    double& at(int xi, int yi) {
        return heights[static_cast<std::size_t>(yi * xRes + xi)];
    }
    double  at(int xi, int yi) const {
        return heights[static_cast<std::size_t>(yi * xRes + xi)];
    }

    double cellW() const { return (xRes > 0) ? (xMax - xMin) / xRes : 0.0; }
    double cellH() const { return (yRes > 0) ? (yMax - yMin) / yRes : 0.0; }
};

#endif // ZMAP_H
