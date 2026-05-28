#include "Art.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <vector>

static constexpr double PI_ART = 3.14159265358979323846;

// --------------------------------------------------------------------------
// GrayscaleImage::sample – bilinear interpolation
// --------------------------------------------------------------------------
float GrayscaleImage::sample(float px, float py) const {
    if (!valid()) return 0.0f;
    px = std::clamp(px, 0.0f, static_cast<float>(width  - 1));
    py = std::clamp(py, 0.0f, static_cast<float>(height - 1));

    const int x0 = static_cast<int>(px), y0 = static_cast<int>(py);
    const int x1 = std::min(x0 + 1, width  - 1);
    const int y1 = std::min(y0 + 1, height - 1);
    const float tx = px - x0, ty = py - y0;

    auto pix = [&](int x, int y) -> float {
        return pixels[static_cast<std::size_t>(y * width + x)] / 255.0f;
    };

    return (1-tx)*(1-ty)*pix(x0,y0) + tx*(1-ty)*pix(x1,y0)
         + (1-tx)*   ty *pix(x0,y1) + tx*   ty *pix(x1,y1);
}

// --------------------------------------------------------------------------
// gaussianBlur – separable 1-D convolution
// --------------------------------------------------------------------------
std::vector<float>
Art::gaussianBlur(const std::vector<float>& input,
                    int width, int height,
                    double sigma) {
    if (sigma <= 0.0) return input;

    const int kRadius = static_cast<int>(std::ceil(3.0 * sigma));
    const int kSize   = 2 * kRadius + 1;

    // Build kernel
    std::vector<float> kernel(static_cast<std::size_t>(kSize));
    float kSum = 0.0f;
    for (int i = 0; i < kSize; ++i) {
        int d = i - kRadius;
        kernel[static_cast<std::size_t>(i)] =
            static_cast<float>(std::exp(-0.5 * d * d / (sigma * sigma)));
        kSum += kernel[static_cast<std::size_t>(i)];
    }
    for (auto& k : kernel) k /= kSum;

    std::vector<float> tmp(input.size(), 0.0f);
    std::vector<float> out(input.size(), 0.0f);

    // Horizontal pass
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float v = 0.0f;
            for (int k = 0; k < kSize; ++k) {
                int sx = std::clamp(x + k - kRadius, 0, width - 1);
                v += kernel[static_cast<std::size_t>(k)] *
                     input[static_cast<std::size_t>(y * width + sx)];
            }
            tmp[static_cast<std::size_t>(y * width + x)] = v;
        }
    }

    // Vertical pass
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float v = 0.0f;
            for (int k = 0; k < kSize; ++k) {
                int sy = std::clamp(y + k - kRadius, 0, height - 1);
                v += kernel[static_cast<std::size_t>(k)] *
                     tmp[static_cast<std::size_t>(sy * width + x)];
            }
            out[static_cast<std::size_t>(y * width + x)] = v;
        }
    }
    return out;
}

// --------------------------------------------------------------------------
// imageToRelief() – grayscale image → triangulated mesh
// --------------------------------------------------------------------------
MeshData Art::imageToRelief(const GrayscaleImage&      image,
                              const ImageToReliefParams& p) {
    MeshData mesh;
    if (!image.valid() || p.meshResX < 2 || p.meshResY < 2) return mesh;

    const int resX = p.meshResX;
    const int resY = p.meshResY;

    // Sample the image with optional Gaussian smoothing
    std::vector<float> heightMap(static_cast<std::size_t>(resX * resY));

    for (int gy = 0; gy < resY; ++gy) {
        for (int gx = 0; gx < resX; ++gx) {
            float px = gx * (image.width  - 1) / static_cast<float>(resX - 1);
            float py = gy * (image.height - 1) / static_cast<float>(resY - 1);
            float v  = image.sample(px, py);
            if (p.invertImage) v = 1.0f - v;
            heightMap[static_cast<std::size_t>(gy * resX + gx)] = v;
        }
    }

    if (p.smoothEdges && p.smoothSigma > 0.0) {
        heightMap = gaussianBlur(heightMap, resX, resY, p.smoothSigma);
    }

    // Build vertex grid
    mesh.vertices.reserve(static_cast<std::size_t>(resX * resY));
    for (int gy = 0; gy < resY; ++gy) {
        for (int gx = 0; gx < resX; ++gx) {
            double wx = p.physicalWidth  * gx / (resX - 1);
            double wy = p.physicalHeight * gy / (resY - 1);
            float  h  = heightMap[static_cast<std::size_t>(gy * resX + gx)];
            double wz = p.baseZ + h * (p.maxReliefHeight - p.baseZ);
            mesh.vertices.push_back({wx, wy, wz});
        }
    }

    // Build quad triangles
    mesh.triangles.reserve(static_cast<std::size_t>(2 * (resX-1) * (resY-1)));
    for (int gy = 0; gy < resY - 1; ++gy) {
        for (int gx = 0; gx < resX - 1; ++gx) {
            int v00 = gy * resX + gx;
            int v10 = gy * resX + gx + 1;
            int v01 = (gy + 1) * resX + gx;
            int v11 = (gy + 1) * resX + gx + 1;
            mesh.triangles.push_back({v00, v10, v11});
            mesh.triangles.push_back({v00, v11, v01});
        }
    }

    mesh.computeNormals();
    return mesh;
}

// --------------------------------------------------------------------------
// vectorToTexture() – 2-D chains → embossed/engraved texture mesh
// --------------------------------------------------------------------------
MeshData Art::vectorToTexture(
    const std::vector<std::vector<Geom::Vec2>>& chains,
    const NurbsSurface&                          hostSurf,
    const VectorTextureParams&                   p) {

    MeshData mesh;

    for (const auto& chain : chains) {
        if (chain.size() < 2) continue;

        const int n = static_cast<int>(chain.size());

        // For each edge segment, generate a quad strip
        for (int i = 0; i < n - 1; ++i) {
            const Geom::Vec2& a = chain[i];
            const Geom::Vec2& b = chain[i + 1];

            double dx = b.x - a.x, dy = b.y - a.y;
            double len = std::sqrt(dx*dx + dy*dy);
            if (len < 1e-9) continue;
            double nx = -dy / len, ny = dx / len;  // inward normal

            double hw = p.bandWidth * 0.5;

            // Four corners of the quad (in XY)
            Geom::Vec2 c0 = {a.x + nx * hw, a.y + ny * hw};
            Geom::Vec2 c1 = {a.x - nx * hw, a.y - ny * hw};
            Geom::Vec2 c2 = {b.x + nx * hw, b.y + ny * hw};
            Geom::Vec2 c3 = {b.x - nx * hw, b.y - ny * hw};

            // Project to host surface Z
            auto projectZ = [&](const Geom::Vec2& pt, double zOff) -> Geom::Vec3 {
                if (p.drapeToSurface) {
                    // Map XY to UV via simple nearest-grid approach
                    double u = (pt.x - hostSurf.uMin()) / (hostSurf.uMax() - hostSurf.uMin());
                    double v = (pt.y - hostSurf.vMin()) / (hostSurf.vMax() - hostSurf.vMin());
                    u = std::clamp(u, 0.0, 1.0);
                    v = std::clamp(v, 0.0, 1.0);
                    Geom::Vec3 sp = hostSurf.evaluate(u, v);
                    Geom::Vec3 sn = hostSurf.normal(u, v);
                    return {sp.x + sn.x * (p.drapeOffset + zOff),
                            sp.y + sn.y * (p.drapeOffset + zOff),
                            sp.z + sn.z * (p.drapeOffset + zOff)};
                }
                return {pt.x, pt.y, p.drapeOffset + zOff};
            };

            // Band profile: flat top or rounded or V-groove
            double topH = p.bandHeight;
            double sideH = 0.0;
            if (p.profile == VectorTextureParams::BandProfile::Round) {
                sideH = -p.bandHeight * 0.5;  // rounded: sides droop
            } else if (p.profile == VectorTextureParams::BandProfile::VGroove) {
                sideH = -p.bandHeight; topH = 0.0;
            }

            int base = static_cast<int>(mesh.vertices.size());
            mesh.vertices.push_back(projectZ(c0, sideH));
            mesh.vertices.push_back(projectZ({a.x, a.y}, topH));  // centre a
            mesh.vertices.push_back(projectZ(c1, sideH));
            mesh.vertices.push_back(projectZ(c2, sideH));
            mesh.vertices.push_back(projectZ({b.x, b.y}, topH));  // centre b
            mesh.vertices.push_back(projectZ(c3, sideH));

            // Triangulate the quad strip (two triangles per side)
            mesh.triangles.push_back({base+0, base+1, base+4});
            mesh.triangles.push_back({base+0, base+4, base+3});
            mesh.triangles.push_back({base+1, base+2, base+5});
            mesh.triangles.push_back({base+1, base+5, base+4});
        }
    }

    mesh.computeNormals();
    return mesh;
}

// --------------------------------------------------------------------------
// organicSmooth() – Laplacian mesh relaxation
// --------------------------------------------------------------------------
MeshData Art::organicSmooth(const MeshData&              mesh,
                               const OrganicSmoothingParams& p) {
    if (mesh.vertices.empty()) return mesh;

    MeshData result = mesh;

    // Build adjacency list (vertex → adjacent vertices)
    const std::size_t nv = mesh.vertices.size();
    std::vector<std::vector<int>> adj(nv);
    for (const auto& tri : mesh.triangles) {
        int a = tri.v0, b = tri.v1, c = tri.v2;
        adj[static_cast<std::size_t>(a)].push_back(b);
        adj[static_cast<std::size_t>(a)].push_back(c);
        adj[static_cast<std::size_t>(b)].push_back(a);
        adj[static_cast<std::size_t>(b)].push_back(c);
        adj[static_cast<std::size_t>(c)].push_back(a);
        adj[static_cast<std::size_t>(c)].push_back(b);
    }

    // Find boundary vertices
    std::vector<bool> isBoundary(nv, false);
    if (p.preserveBoundary) {
        // A vertex is on the boundary if it has an edge not shared by two triangles.
        std::map<std::pair<int,int>, int> edgeCount;
        for (const auto& tri : mesh.triangles) {
            int vs[3] = {tri.v0, tri.v1, tri.v2};
            for (int e = 0; e < 3; ++e) {
                int u = vs[e], v = vs[(e+1)%3];
                if (u > v) std::swap(u, v);
                edgeCount[{u,v}]++;
            }
        }
        for (const auto& [edge, cnt] : edgeCount) {
            if (cnt < 2) {
                isBoundary[static_cast<std::size_t>(edge.first)]  = true;
                isBoundary[static_cast<std::size_t>(edge.second)] = true;
            }
        }
    }

    // Laplacian relaxation
    for (int iter = 0; iter < p.iterations; ++iter) {
        std::vector<Geom::Vec3> newPos(nv);
        for (std::size_t vi = 0; vi < nv; ++vi) {
            if (isBoundary[vi]) { newPos[vi] = result.vertices[vi]; continue; }
            const auto& neighbors = adj[vi];
            if (neighbors.empty()) { newPos[vi] = result.vertices[vi]; continue; }

            double sx = 0, sy = 0, sz = 0;
            for (int nb : neighbors) {
                sx += result.vertices[static_cast<std::size_t>(nb)].x;
                sy += result.vertices[static_cast<std::size_t>(nb)].y;
                sz += result.vertices[static_cast<std::size_t>(nb)].z;
            }
            double invN = 1.0 / neighbors.size();
            Geom::Vec3 laplace = {sx * invN, sy * invN, sz * invN};

            // Optional feature preservation: reduce lambda at high curvature
            double lambda = p.lambda;
            if (p.featurePreserve) {
                // Estimate local curvature by angular deviation of vertex normal
                Geom::Vec3 cn = result.normals.empty() ?
                    Geom::Vec3{0,0,1} : result.normals[vi];
                double minDot = 1.0;
                for (int nb : neighbors) {
                    if (result.normals.size() > static_cast<std::size_t>(nb)) {
                        Geom::Vec3 nn = result.normals[static_cast<std::size_t>(nb)];
                        double dot = cn.x*nn.x + cn.y*nn.y + cn.z*nn.z;
                        minDot = std::min(minDot, dot);
                    }
                }
                double angleRad = std::acos(std::clamp(minDot, -1.0, 1.0));
                double threshold = p.featureAngle * PI_ART / 180.0;
                if (angleRad > threshold) lambda *= 0.1;
            }

            const Geom::Vec3& cur = result.vertices[vi];
            newPos[vi] = {
                cur.x + lambda * (laplace.x - cur.x),
                cur.y + lambda * (laplace.y - cur.y),
                cur.z + lambda * (laplace.z - cur.z)
            };
        }
        result.vertices = newPos;
        result.computeNormals();
    }

    return result;
}

// --------------------------------------------------------------------------
// insideBoundary() – point-in-polygon test (ray casting)
// --------------------------------------------------------------------------
bool Art::insideBoundary(const std::vector<Geom::Vec2>& boundary,
                           double x, double y) {
    bool inside = false;
    const int n = static_cast<int>(boundary.size());
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const auto& pi = boundary[static_cast<std::size_t>(i)];
        const auto& pj = boundary[static_cast<std::size_t>(j)];
        if (((pi.y > y) != (pj.y > y)) &&
            (x < (pj.x - pi.x) * (y - pi.y) / (pj.y - pi.y) + pi.x))
            inside = !inside;
    }
    return inside;
}

// --------------------------------------------------------------------------
// buildRasterPass() – helper for reliefToolpath
// --------------------------------------------------------------------------
void Art::buildRasterPass(Toolpath& tp,
                            double xMin, double xMax,
                            double yMin, double yMax,
                            double z,
                            double stepOver,
                            double angle,
                            double feedRate) {
    (void)angle; // simplified: always X-aligned
    (void)feedRate;

    if (stepOver <= 0.0) return;

    bool forward = true;
    for (double y = yMin; y <= yMax + 1e-9; y += stepOver) {
        double startX = forward ? xMin : xMax;
        double endX   = forward ? xMax : xMin;

        ToolpathPoint p1, p2;
        p1.position = {startX, y, z};
        p1.toolAxis = {0, 0, 1};
        p1.motion   = MotionType::Rapid;

        p2.position = {endX, y, z};
        p2.toolAxis = {0, 0, 1};
        p2.motion   = MotionType::Linear;

        tp.addPoint(p1);
        tp.addPoint(p2);
        forward = !forward;
    }
}

// --------------------------------------------------------------------------
// reliefToolpath()
// --------------------------------------------------------------------------
std::vector<Toolpath>
Art::reliefToolpath(const MeshData&              relief,
                     const ReliefToolpathParams&  p,
                     const CuttingTool&           tool,
                     const CuttingParams&         cuts) {
    std::vector<Toolpath> result;
    if (relief.vertices.empty()) return result;

    // Compute bounding box of the relief mesh
    double xMin =  1e9, xMax = -1e9;
    double yMin =  1e9, yMax = -1e9;
    double zMin =  1e9, zMax = -1e9;
    for (const auto& v : relief.vertices) {
        xMin = std::min(xMin, v.x); xMax = std::max(xMax, v.x);
        yMin = std::min(yMin, v.y); yMax = std::max(yMax, v.y);
        zMin = std::min(zMin, v.z); zMax = std::max(zMax, v.z);
    }

    // Depth limit
    double cutZMin = (p.maxDepth >= 0.0) ? -p.maxDepth : zMin;

    // Rough pass
    if (p.roughAndFinish) {
        Toolpath rough(StrategyType::ArtRelief, tool, cuts);
        rough.setName("Relief Rough");
        buildRasterPass(rough, xMin, xMax, yMin, yMax,
                         cutZMin + p.roughAllowance,
                         p.roughStepOver, p.angle, cuts.feedRate);
        result.push_back(std::move(rough));
    }

    // Finish pass: sample mesh Z at each raster point
    Toolpath finish(StrategyType::ArtRelief, tool, cuts);
    finish.setName("Relief Finish");

    const double so = p.stepOver;
    bool forward = true;
    for (double y = yMin; y <= yMax + 1e-9; y += so) {
        // Check boundary confinement
        if (!p.confineBoundary.empty()) {
            bool inBounds = false;
            for (double x = xMin; x <= xMax; x += so) {
                if (insideBoundary(p.confineBoundary, x, y)) { inBounds = true; break; }
            }
            if (!inBounds) continue;
        }

        double startX = forward ? xMin : xMax;
        double endX   = forward ? xMax : xMin;
        double stepX  = forward ? so : -so;

        for (double x = startX;
             forward ? (x <= endX + 1e-9) : (x >= endX - 1e-9);
             x += stepX) {

            if (!p.confineBoundary.empty() &&
                !insideBoundary(p.confineBoundary, x, y)) continue;

            // Sample mesh Z at (x, y) via nearest vertex
            double bestZ = zMax + p.stockAllowance;
            double bestDist = 1e9;
            for (const auto& v : relief.vertices) {
                double dx = v.x - x, dy = v.y - y;
                double dist = dx*dx + dy*dy;
                if (dist < bestDist) {
                    bestDist = dist;
                    bestZ    = v.z + p.stockAllowance;
                }
            }

            if (p.maxDepth >= 0.0 && bestZ < -p.maxDepth) {
                bestZ = -p.maxDepth;
            }

            ToolpathPoint pt;
            pt.position = {x, y, bestZ};
            pt.toolAxis = {0, 0, 1};
            pt.motion   = finish.points().empty() ? MotionType::Rapid : MotionType::Linear;
            finish.addPoint(pt);
        }
        forward = !forward;
    }

    result.push_back(std::move(finish));
    return result;
}

// --------------------------------------------------------------------------
// blendMeshes()
// --------------------------------------------------------------------------
MeshData Art::blendMeshes(const MeshData& base,
                            const MeshData& overlay,
                            double          blendRadius) {
    MeshData result = base;

    for (auto& bv : result.vertices) {
        // Find overlay vertices within blendRadius
        for (const auto& ov : overlay.vertices) {
            double dx = ov.x - bv.x, dy = ov.y - bv.y;
            double dist = std::sqrt(dx*dx + dy*dy);
            if (dist >= blendRadius) continue;
            // Cosine falloff
            double alpha = 0.5 * (1.0 + std::cos(PI_ART * dist / blendRadius));
            // Overlay displacement is its Z relative to zero
            bv.z += alpha * ov.z;
        }
    }

    result.computeNormals();
    return result;
}

// --------------------------------------------------------------------------
// heightmapToZMap()
// --------------------------------------------------------------------------
ZMap Art::heightmapToZMap(const GrayscaleImage&      image,
                            const ImageToReliefParams& p) {
    ZMap zmap;
    if (!image.valid()) return zmap;

    zmap.xMin = 0.0; zmap.xMax = p.physicalWidth;
    zmap.yMin = 0.0; zmap.yMax = p.physicalHeight;
    zmap.xRes = p.meshResX;
    zmap.yRes = p.meshResY;
    zmap.heights.resize(static_cast<std::size_t>(p.meshResX * p.meshResY));

    for (int gy = 0; gy < p.meshResY; ++gy) {
        for (int gx = 0; gx < p.meshResX; ++gx) {
            float px = gx * (image.width  - 1) / static_cast<float>(p.meshResX - 1);
            float py = gy * (image.height - 1) / static_cast<float>(p.meshResY - 1);
            float v  = image.sample(px, py);
            if (p.invertImage) v = 1.0f - v;
            double z = p.baseZ + v * (p.maxReliefHeight - p.baseZ);
            zmap.at(gx, gy) = z;
        }
    }
    return zmap;
}
