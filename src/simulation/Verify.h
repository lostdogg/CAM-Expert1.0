#pragma once
#ifndef VERIFY_H
#define VERIFY_H

#include "../cam/Toolpath.h"
#include "../cad/MeshData.h"
#include "../managers/ToolpathManager.h"
#include <vector>
#include <string>
#include <functional>

// --------------------------------------------------------------------------
// Verify – solid stock simulation
// --------------------------------------------------------------------------

struct VerifyResult {
    bool   hasGouge     = false;
    bool   hasUndercut  = false;
    int    gougeCount   = 0;
    int    undercutCount= 0;
    double maxGougeDepth   = 0; // mm
    double maxUndercutDepth= 0; // mm

    std::vector<Geom::Vec3> gougeLocations;
    std::vector<Geom::Vec3> undercutLocations;
};

// The stock model – a Z-map (dexel) representation
struct ZMap {
    double xMin, xMax, yMin, yMax;
    int    xRes, yRes;
    std::vector<double> heights;

    double& at(int xi, int yi) {
        return heights[static_cast<std::size_t>(yi * xRes + xi)];
    }
    double  at(int xi, int yi) const {
        return heights[static_cast<std::size_t>(yi * xRes + xi)];
    }

    double cellW() const { return (xMax - xMin) / xRes; }
    double cellH() const { return (yMax - yMin) / yRes; }
};

// Options defined outside Verify class to avoid GCC nested-struct default-arg issue
struct VerifyOptions {
    double stockXMin = -60, stockXMax = 60;
    double stockYMin = -60, stockYMax = 60;
    double stockZTop =  0,  stockZBot =-50;
    int    zMapRes   = 512;
    double gougeTol  = 0.01;
};

class Verify {
public:
    using Options = VerifyOptions;

    explicit Verify(Options opts = Options{});

    VerifyResult run(const ToolpathManager* mgr);
    VerifyResult compare(const ZMap& stock, const MeshData& target) const;
    const ZMap& stockMap() const { return m_stockMap; }
    static MeshData zMapToMesh(const ZMap& zmap);

    using ProgressCallback = std::function<void(int percent)>;
    void setProgressCallback(ProgressCallback cb) { m_progress = std::move(cb); }

private:
    void subtractMove(const ToolpathPoint& from, const ToolpathPoint& to,
                       const CuttingTool& tool, ZMap& zmap);

    Options          m_opts;
    ZMap             m_stockMap;
    ProgressCallback m_progress;
};

#endif // VERIFY_H
