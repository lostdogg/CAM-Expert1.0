#include "TransformToolpath.h"
#include <sstream>
#include <cmath>

static constexpr double PI_TF = 3.14159265358979323846;

// --------------------------------------------------------------------------
// TransformResult helpers
// --------------------------------------------------------------------------

std::string TransformResult::workOffsetSequence() const {
    std::ostringstream oss;
    for (const auto& c : copies)
        oss << "G" << c.wcsPlane.gCodeOffsetNumber() << "\n";
    return oss.str();
}

std::string TransformResult::subprogramBody() const {
    if (copies.empty()) return "";
    // The subprogram body is the first copy's toolpath (source geometry)
    std::ostringstream oss;
    oss << "O" << subprogramNumber << " (SUBPROGRAM)\n";
    for (const auto& pt : copies[0].toolpath.points()) {
        switch (pt.motion) {
        case MotionType::Rapid:
            oss << "G00 X" << pt.position.x
                << " Y" << pt.position.y
                << " Z" << pt.position.z << "\n";
            break;
        default:
            oss << "G01 X" << pt.position.x
                << " Y" << pt.position.y
                << " Z" << pt.position.z << "\n";
            break;
        }
    }
    oss << "M99\n";
    return oss.str();
}

// --------------------------------------------------------------------------
// Internal helpers
// --------------------------------------------------------------------------

void TransformToolpath::applyMatrix(Toolpath& tp, const Geom::Mat4& m) {
    for (auto& pt : tp.mutablePoints()) {
        pt.position = m.transformPoint(pt.position);
        pt.toolAxis = m.transformVector(pt.toolAxis).normalized();
        if (pt.arcRadius > 0)
            pt.arcCenter = m.transformPoint(pt.arcCenter);
    }
}

CoordPlane TransformToolpath::makeCopyPlane(const Geom::Vec3& origin,
                                             int wcsOffset,
                                             const std::string& name) {
    CoordPlane p;
    p.name      = name;
    p.type      = PlaneType::WCS;
    p.origin    = origin;
    p.wcsOffset = wcsOffset;
    return p;
}

void TransformToolpath::invertArcs(Toolpath& tp) {
    for (auto& pt : tp.mutablePoints()) {
        if (pt.motion == MotionType::ArcCW)
            pt.motion = MotionType::ArcCCW;
        else if (pt.motion == MotionType::ArcCCW)
            pt.motion = MotionType::ArcCW;
    }
}

// --------------------------------------------------------------------------
// Translate
// --------------------------------------------------------------------------
TransformResult TransformToolpath::translate(const Toolpath&       source,
                                              const TranslateParams& params,
                                              TransformOutputType    outputType,
                                              bool                   useSubprogram,
                                              int                    firstWcsOffset) {
    TransformResult result;
    result.useSubprogram = useSubprogram;

    int total = 1 + params.copies;
    for (int i = 0; i < total; ++i) {
        Geom::Vec3 offset{params.delta.x * i,
                          params.delta.y * i,
                          params.delta.z * i};

        Toolpath copy = source;
        copy.setName(source.name() + "_T" + std::to_string(i));

        if (outputType == TransformOutputType::Toolpath) {
            // Shift XYZ coordinates; keep the same WCS
            Geom::Mat4 t = Geom::Mat4::translate(offset);
            applyMatrix(copy, t);
        }

        TransformResult::Copy c;
        c.toolpath = std::move(copy);

        // Build WCS plane for Coordinate output
        int wcsOff = firstWcsOffset + i;
        c.wcsPlane = makeCopyPlane(offset, wcsOff,
                                   "Fixture " + std::to_string(i + 1));
        c.subprogramNumber = result.subprogramNumber + i;

        result.copies.push_back(std::move(c));
    }

    return result;
}

// --------------------------------------------------------------------------
// Rotate
// --------------------------------------------------------------------------
TransformResult TransformToolpath::rotate(const Toolpath&    source,
                                           const RotateParams& params,
                                           TransformOutputType outputType,
                                           bool                useSubprogram,
                                           int                 firstWcsOffset) {
    TransformResult result;
    result.useSubprogram = useSubprogram;

    int total = 1 + params.copies;
    for (int i = 0; i < total; ++i) {
        double angleDeg = params.angleDegIncrement * i;
        double angleRad = angleDeg * (PI_TF / 180.0);

        Toolpath copy = source;
        copy.setName(source.name() + "_R" + std::to_string(i));

        if (outputType == TransformOutputType::Toolpath ||
            outputType == TransformOutputType::ToolPlane) {
            // Build rotation matrix about params.axis through params.centre
            Geom::Mat4 toCentre = Geom::Mat4::translate(
                {-params.centre.x, -params.centre.y, -params.centre.z});
            Geom::Mat4 rot      = Geom::Mat4::rotate(params.axis.normalized(), angleRad);
            Geom::Mat4 fromCentre = Geom::Mat4::translate(params.centre);

            // Compose: fromCentre * rot * toCentre
            Geom::Mat4 combined = Geom::matMul(fromCentre,
                                   Geom::matMul(rot, toCentre));
            applyMatrix(copy, combined);
        }

        TransformResult::Copy c;

        // For the WCS plane of this copy: the origin rotates around the centre
        double cosA = std::cos(angleRad), sinA = std::sin(angleRad);
        // Source origin relative to centre, rotated (assuming rotation about Z)
        Geom::Vec3 rel{source.boundingBox().center().x - params.centre.x,
                       source.boundingBox().center().y - params.centre.y, 0};
        Geom::Vec3 rotRel{rel.x * cosA - rel.y * sinA,
                          rel.x * sinA + rel.y * cosA, 0};
        Geom::Vec3 copyOrigin{params.centre.x + rotRel.x,
                               params.centre.y + rotRel.y, 0};

        int wcsOff = firstWcsOffset + i;
        c.wcsPlane       = makeCopyPlane(copyOrigin, wcsOff,
                                          "RotCopy " + std::to_string(i));
        c.toolpath       = std::move(copy);
        c.subprogramNumber = result.subprogramNumber + i;
        result.copies.push_back(std::move(c));
    }

    return result;
}

// --------------------------------------------------------------------------
// Mirror
// --------------------------------------------------------------------------
TransformResult TransformToolpath::mirror(const Toolpath&    source,
                                           const MirrorParams& params,
                                           TransformOutputType outputType,
                                           int                 mirrorWcsOffset) {
    TransformResult result;
    result.useSubprogram = false;

    // Copy 0: the original source (no transform)
    {
        TransformResult::Copy c0;
        c0.toolpath = source;
        c0.toolpath.setName(source.name() + "_Orig");
        c0.wcsPlane = makeCopyPlane({}, 0, "Mirror Origin");
        c0.subprogramNumber = result.subprogramNumber;
        result.copies.push_back(std::move(c0));
    }

    // Copy 1: mirrored
    {
        Toolpath mirrored = source;
        mirrored.setName(source.name() + "_Mirrored");

        // Build mirror matrix: reflection through the plane defined by
        // (mirrorOrigin, mirrorNormal).  Using the Householder formula:
        //   M = I - 2 * n⊗n   (where n is the unit normal of the mirror plane)
        Geom::Vec3 n;
        switch (params.mirrorAxis) {
        case MirrorParams::Axis::X: n = {1, 0, 0}; break;
        case MirrorParams::Axis::Y: n = {0, 1, 0}; break;
        case MirrorParams::Axis::Z: n = {0, 0, 1}; break;
        default:                    n = params.customNormal.normalized(); break;
        }

        Geom::Mat4 mirrorMat;
        mirrorMat.m[0]  = 1 - 2*n.x*n.x;
        mirrorMat.m[4]  =   - 2*n.x*n.y;
        mirrorMat.m[8]  =   - 2*n.x*n.z;
        mirrorMat.m[1]  =   - 2*n.y*n.x;
        mirrorMat.m[5]  = 1 - 2*n.y*n.y;
        mirrorMat.m[9]  =   - 2*n.y*n.z;
        mirrorMat.m[2]  =   - 2*n.z*n.x;
        mirrorMat.m[6]  =   - 2*n.z*n.y;
        mirrorMat.m[10] = 1 - 2*n.z*n.z;
        // Translation: reflect mirrorOrigin through itself → stays fixed,
        // but all other points shift by 2*(p - o)·n
        // Encode as translate(-o) * pure_mirror * translate(+o)
        Geom::Vec3 o = params.mirrorOrigin;
        Geom::Mat4 toO   = Geom::Mat4::translate({-o.x, -o.y, -o.z});
        Geom::Mat4 fromO = Geom::Mat4::translate(o);
        Geom::Mat4 full  = Geom::matMul(fromO, Geom::matMul(mirrorMat, toO));

        if (outputType == TransformOutputType::Toolpath ||
            outputType == TransformOutputType::Coordinate) {
            applyMatrix(mirrored, full);
        }

        // Mirror reverses the handedness → climb becomes conventional
        if (params.compensateClimb)
            invertArcs(mirrored);

        // Determine reflected origin for the WCS plane
        Geom::Vec3 srcOrigin{};
        Geom::Vec3 mirOrigin = full.transformPoint(srcOrigin);

        TransformResult::Copy c1;
        c1.toolpath = std::move(mirrored);
        c1.wcsPlane = makeCopyPlane(mirOrigin, mirrorWcsOffset,
                                     source.name() + " Mirror");
        c1.subprogramNumber = result.subprogramNumber + 1;
        result.copies.push_back(std::move(c1));
    }

    return result;
}
