#include "FileImporter.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static std::string extensionFromPath(const std::string& path) {
    auto pos = path.rfind('.');
    return pos == std::string::npos ? "" : toLower(path.substr(pos));
}

static bool isAsciiSTL(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    char buf[6] = {};
    f.read(buf, 5);
    return std::strncmp(buf, "solid", 5) == 0;
}

class StepImporter final : public IGeometryImporter {
public:
    const char* name() const override { return "STEP"; }
    GeometryPriority priority() const override { return GeometryPriority::Precise; }
    std::vector<std::string> supportedExtensions() const override { return {".step", ".stp"}; }

    bool importFile(const std::string& path, ImportResult& outResult, std::string& outMessage) const override {
        std::ifstream f(path);
        if (!f.is_open()) {
            outMessage = "Cannot open STEP file: " + path;
            outResult = std::monostate{};
            return false;
        }

        double dimX = 100.0, dimY = 50.0, dimZ = 25.0;
        std::string productName = "STEP_Import";
        std::string schema = "AP203/AP214/AP242 (undetected)";
        int entityCount = 0;
        bool inData = false;

        std::string line;
        while (std::getline(f, line)) {
            std::string low = toLower(line);

            if (line.find("DATA;") != std::string::npos)  { inData = true;  continue; }
            if (line.find("ENDSEC;") != std::string::npos){ inData = false; continue; }

            if (!inData) {
                auto pos = line.find("FILE_NAME(");
                if (pos != std::string::npos) {
                    auto q1 = line.find('\'', pos);
                    auto q2 = (q1 != std::string::npos) ? line.find('\'', q1 + 1) : std::string::npos;
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1 + 1)
                        productName = line.substr(q1 + 1, q2 - q1 - 1);
                }
                if (low.find("ap203") != std::string::npos) schema = "AP203";
                if (low.find("ap214") != std::string::npos) schema = "AP214";
                if (low.find("ap242") != std::string::npos) schema = "AP242";
            }

            if (inData) {
                if (!line.empty() && line[0] == '#') ++entityCount;
                if (line.find("CARTESIAN_POINT") != std::string::npos) {
                    auto p1 = line.find('(');
                    auto p2 = line.rfind(')');
                    if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                        auto tp1 = line.find('(', p1 + 1);
                        if (tp1 != std::string::npos) {
                            std::istringstream cs(line.substr(tp1 + 1));
                            double x = 0.0, y = 0.0, z = 0.0;
                            char comma = 0;
                            if (cs >> x >> comma >> y >> comma >> z) {
                                dimX = std::max(dimX, std::abs(x) * 2.0);
                                dimY = std::max(dimY, std::abs(y) * 2.0);
                                dimZ = std::max(dimZ, std::abs(z) * 2.0);
                            }
                        }
                    }
                }
            }
        }

        if (productName.empty() || productName == " ") productName = "STEP_Import";

        auto solid = BRep::Solid::makeBox(
            std::max(dimX, 1.0),
            std::max(dimY, 1.0),
            std::max(dimZ, 1.0));
        solid.setName(productName);

        outResult = solid;
        outMessage = "STEP " + schema + ": parsed " + std::to_string(entityCount)
                   + " entities (representative precise B-Rep placeholder).";
        return true;
    }
};

class IgesImporter final : public IGeometryImporter {
public:
    const char* name() const override { return "IGES"; }
    GeometryPriority priority() const override { return GeometryPriority::Precise; }
    std::vector<std::string> supportedExtensions() const override { return {".iges", ".igs"}; }

    bool importFile(const std::string& path, ImportResult& outResult, std::string& outMessage) const override {
        std::ifstream f(path);
        if (!f.is_open()) {
            outMessage = "Cannot open IGES file: " + path;
            outResult = std::monostate{};
            return false;
        }

        std::string productName = "IGES_Import";
        int paramRecords = 0;
        double dimHint = 80.0;

        std::string line;
        while (std::getline(f, line)) {
            if (line.size() < 73) continue;
            char section = line[72];

            if (section == 'S') {
                std::string desc = line.substr(0, 72);
                auto end = desc.find_last_not_of(' ');
                if (end != std::string::npos && end > 0 && desc[0] != ' ')
                    productName = desc.substr(0, end + 1);
            }
            if (section == 'P') {
                ++paramRecords;
                std::string params = line.substr(0, 64);
                std::istringstream ps(params);
                std::string tok;
                while (std::getline(ps, tok, ',')) {
                    try {
                        double v = std::stod(tok);
                        if (std::abs(v) > dimHint && std::abs(v) < 10000.0)
                            dimHint = std::abs(v);
                    } catch (...) {}
                }
            }
        }

        if (productName.empty()) productName = "IGES_Import";

        double d = std::max(dimHint, 10.0);
        auto solid = BRep::Solid::makeBox(d, d * 0.5, d * 0.25);
        solid.setName(productName);

        outResult = solid;
        outMessage = "IGES: parsed " + std::to_string(paramRecords)
                   + " parameter records (representative precise B-Rep placeholder).";
        return true;
    }
};

class ParasolidImporter final : public IGeometryImporter {
public:
    const char* name() const override { return "Parasolid"; }
    GeometryPriority priority() const override { return GeometryPriority::Precise; }
    std::vector<std::string> supportedExtensions() const override { return {".x_t", ".x_b"}; }

    bool importFile(const std::string& path, ImportResult& outResult, std::string& outMessage) const override {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) {
            outMessage = "Cannot open Parasolid file: " + path;
            outResult = std::monostate{};
            return false;
        }

        auto solid = BRep::Solid::makeBox(120.0, 70.0, 35.0);
        solid.setName("Parasolid_Import");
        outResult = solid;
        outMessage = "Parasolid import bridge placeholder: kernel-accurate reader can be plugged in via IGeometryImporter.";
        return true;
    }
};

class StlImporter final : public IGeometryImporter {
public:
    const char* name() const override { return "STL"; }
    GeometryPriority priority() const override { return GeometryPriority::Mesh; }
    std::vector<std::string> supportedExtensions() const override { return {".stl"}; }

    bool importFile(const std::string& path, ImportResult& outResult, std::string& outMessage) const override {
        std::vector<Geom::Triangle> tris;

        if (isAsciiSTL(path)) {
            std::ifstream f(path);
            if (!f.is_open()) {
                outMessage = "Cannot open STL file: " + path;
                outResult = std::monostate{};
                return false;
            }
            std::string line, token;
            Geom::Triangle tri;
            int vIdx = 0;
            while (std::getline(f, line)) {
                std::istringstream ss(line);
                ss >> token;
                if (token == "vertex") {
                    double x = 0.0, y = 0.0, z = 0.0;
                    ss >> x >> y >> z;
                    tri.v[vIdx++] = {x, y, z};
                    if (vIdx == 3) {
                        tris.push_back(tri);
                        vIdx = 0;
                    }
                }
            }
        } else {
            std::ifstream f(path, std::ios::binary);
            if (!f.is_open()) {
                outMessage = "Cannot open STL file: " + path;
                outResult = std::monostate{};
                return false;
            }
            char header[80] = {};
            f.read(header, 80);
            uint32_t numTris = 0;
            f.read(reinterpret_cast<char*>(&numTris), 4);
            tris.reserve(numTris);
            for (uint32_t i = 0; i < numTris; ++i) {
                float n[3] = {}, v[9] = {};
                uint16_t attr = 0;
                f.read(reinterpret_cast<char*>(n), 12);
                f.read(reinterpret_cast<char*>(v), 36);
                f.read(reinterpret_cast<char*>(&attr), 2);
                if (!f.good()) break;
                Geom::Triangle tri;
                for (int j = 0; j < 3; ++j)
                    tri.v[j] = {v[j * 3], v[j * 3 + 1], v[j * 3 + 2]};
                tris.push_back(tri);
            }
        }

        if (tris.empty()) {
            outMessage = "STL file contains no triangles: " + path;
            outResult = std::monostate{};
            return false;
        }

        MeshData mesh(std::move(tris));
        mesh.setName("STL_Import");
        outResult = mesh;
        outMessage.clear();
        return true;
    }
};

class ObjImporter final : public IGeometryImporter {
public:
    const char* name() const override { return "OBJ"; }
    GeometryPriority priority() const override { return GeometryPriority::Mesh; }
    std::vector<std::string> supportedExtensions() const override { return {".obj"}; }

    bool importFile(const std::string& path, ImportResult& outResult, std::string& outMessage) const override {
        std::ifstream f(path);
        if (!f.is_open()) {
            outMessage = "Cannot open OBJ file: " + path;
            outResult = std::monostate{};
            return false;
        }

        std::vector<Geom::Vec3> positions;
        std::vector<Geom::Triangle> tris;
        std::string line, token;

        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            ss >> token;
            if (token == "v") {
                double x = 0.0, y = 0.0, z = 0.0;
                ss >> x >> y >> z;
                positions.push_back({x, y, z});
            } else if (token == "f") {
                std::vector<int> faceIdx;
                std::string vtok;
                while (ss >> vtok) {
                    int vi = std::stoi(vtok.substr(0, vtok.find('/')));
                    if (vi < 0) vi = static_cast<int>(positions.size()) + vi;
                    else vi--;
                    if (vi >= 0 && vi < static_cast<int>(positions.size()))
                        faceIdx.push_back(vi);
                }
                for (int i = 1; i + 1 < static_cast<int>(faceIdx.size()); ++i) {
                    Geom::Triangle tri;
                    tri.v[0] = positions[faceIdx[0]];
                    tri.v[1] = positions[faceIdx[i]];
                    tri.v[2] = positions[faceIdx[i + 1]];
                    tris.push_back(tri);
                }
            }
        }

        if (tris.empty()) {
            outMessage = "OBJ file contains no valid faces: " + path;
            outResult = std::monostate{};
            return false;
        }

        MeshData mesh(std::move(tris));
        mesh.setName("OBJ_Import");
        outResult = mesh;
        outMessage.clear();
        return true;
    }
};

class StubPreciseImporter final : public IGeometryImporter {
public:
    StubPreciseImporter(const char* importerName,
                        std::vector<std::string> extensions,
                        std::string sdkHint)
        : m_name(importerName), m_ext(std::move(extensions)), m_hint(std::move(sdkHint)) {}

    const char* name() const override { return m_name.c_str(); }
    GeometryPriority priority() const override { return GeometryPriority::Precise; }
    std::vector<std::string> supportedExtensions() const override { return m_ext; }

    bool importFile(const std::string&, ImportResult& outResult, std::string& outMessage) const override {
        auto solid = BRep::Solid::makeBox(100.0, 50.0, 25.0);
        std::string name = m_name;
        name += "_Stub";
        solid.setName(name);
        outResult = solid;
        outMessage = m_hint + " (stub geometry displayed).";
        return true;
    }

private:
    std::string m_name;
    std::vector<std::string> m_ext;
    std::string m_hint;
};

class StubMeshImporter final : public IGeometryImporter {
public:
    StubMeshImporter(const char* importerName,
                     std::vector<std::string> extensions,
                     std::string hint)
        : m_name(importerName), m_ext(std::move(extensions)), m_hint(std::move(hint)) {}

    const char* name() const override { return m_name.c_str(); }
    GeometryPriority priority() const override { return GeometryPriority::Mesh; }
    std::vector<std::string> supportedExtensions() const override { return m_ext; }

    bool importFile(const std::string&, ImportResult& outResult, std::string& outMessage) const override {
        outResult = std::monostate{};
        outMessage = m_hint;
        return false;
    }

private:
    std::string m_name;
    std::vector<std::string> m_ext;
    std::string m_hint;
};

} // namespace

FileImporter::FileImporter() {
    registerDefaultImporters();
}

FileFormat FileImporter::detectFormat(const std::string& filePath) {
    std::string ext = normalizeExtension(filePath);
    if (ext == ".step" || ext == ".stp") return FileFormat::STEP;
    if (ext == ".iges" || ext == ".igs") return FileFormat::IGES;
    if (ext == ".x_t" || ext == ".x_b") return FileFormat::Parasolid;
    if (ext == ".stl") return FileFormat::STL;
    if (ext == ".obj") return FileFormat::OBJ;
    if (ext == ".3mf") return FileFormat::ThreeMF;
    if (ext == ".amf") return FileFormat::AMF;
    if (ext == ".sldprt" || ext == ".sldasm") return FileFormat::SolidWorks;
    if (ext == ".dwg" || ext == ".dxf") return FileFormat::AutoCAD;
    if (ext == ".3dm") return FileFormat::Rhino3DM;
    if (ext == ".ipt" || ext == ".iam") return FileFormat::Inventor;
    if (ext == ".catpart") return FileFormat::CATIA;
    return FileFormat::Unknown;
}

std::string FileImporter::formatName(FileFormat fmt) {
    switch (fmt) {
    case FileFormat::STEP:       return "STEP";
    case FileFormat::IGES:       return "IGES";
    case FileFormat::Parasolid:  return "Parasolid";
    case FileFormat::STL:        return "STL";
    case FileFormat::OBJ:        return "OBJ";
    case FileFormat::ThreeMF:    return "3MF";
    case FileFormat::AMF:        return "AMF";
    case FileFormat::SolidWorks: return "SolidWorks";
    case FileFormat::AutoCAD:    return "AutoCAD";
    case FileFormat::Rhino3DM:   return "Rhino";
    case FileFormat::Inventor:   return "Inventor";
    case FileFormat::CATIA:      return "CATIA";
    default:                     return "Unknown";
    }
}

ImportResult FileImporter::import(const std::string& filePath) {
    m_lastError.clear();

    const std::string ext = normalizeExtension(filePath);
    auto mapIt = m_extensionMap.find(ext);
    if (mapIt == m_extensionMap.end() || mapIt->second.empty()) {
        m_lastError = "No importer registered for extension: " + ext;
        return std::monostate{};
    }

    ImportResult result = std::monostate{};
    std::string bestError;
    int bestPriority = -1;

    for (const IGeometryImporter* importer : mapIt->second) {
        if (!importer) continue;
        std::string message;
        ImportResult candidate = std::monostate{};
        const bool ok = importer->importFile(filePath, candidate, message);
        const int priority = static_cast<int>(importer->priority());

        if (ok && !std::holds_alternative<std::monostate>(candidate)) {
            m_lastError = message;
            return candidate;
        }

        if (priority > bestPriority) {
            bestPriority = priority;
            bestError = message.empty() ? (std::string(importer->name()) + " importer failed.") : message;
        }
    }

    m_lastError = bestError.empty() ? "Import failed: no compatible importer succeeded." : bestError;
    return result;
}

void FileImporter::registerImporter(std::unique_ptr<IGeometryImporter> importer) {
    if (!importer) return;
    IGeometryImporter* raw = importer.get();
    m_importers.push_back(std::move(importer));

    for (const std::string& ext : raw->supportedExtensions()) {
        const std::string norm = normalizeExtension(ext);
        if (norm.empty()) continue;
        auto& bucket = m_extensionMap[norm];
        bucket.push_back(raw);
        std::stable_sort(bucket.begin(), bucket.end(),
            [](const IGeometryImporter* a, const IGeometryImporter* b) {
                return static_cast<int>(a->priority()) > static_cast<int>(b->priority());
            });
    }
}

std::vector<std::string> FileImporter::supportedExtensions() const {
    std::vector<std::string> ext;
    ext.reserve(m_extensionMap.size());
    for (const auto& kv : m_extensionMap) ext.push_back(kv.first);
    std::sort(ext.begin(), ext.end());
    return ext;
}

void FileImporter::registerDefaultImporters() {
    registerImporter(std::make_unique<StepImporter>());
    registerImporter(std::make_unique<IgesImporter>());
    registerImporter(std::make_unique<ParasolidImporter>());
    registerImporter(std::make_unique<StlImporter>());
    registerImporter(std::make_unique<ObjImporter>());

    registerImporter(std::make_unique<StubPreciseImporter>(
        "SolidWorks", std::vector<std::string>{".sldprt", ".sldasm"},
        "SolidWorks import requires SolidWorks SDK (eDrawings)"));
    registerImporter(std::make_unique<StubPreciseImporter>(
        "AutoCAD", std::vector<std::string>{".dwg", ".dxf"},
        "DWG/DXF precise import requires ODA or equivalent bridge"));
    registerImporter(std::make_unique<StubPreciseImporter>(
        "Rhino3DM", std::vector<std::string>{".3dm"},
        "Rhino import requires OpenNURBS/SDK bridge"));
    registerImporter(std::make_unique<StubPreciseImporter>(
        "Inventor", std::vector<std::string>{".ipt", ".iam"},
        "Inventor import requires Autodesk SDK"));
    registerImporter(std::make_unique<StubPreciseImporter>(
        "CATIA", std::vector<std::string>{".catpart"},
        "CATIA import requires vendor bridge (e.g., Datakit/Spatial)"));

    registerImporter(std::make_unique<StubMeshImporter>(
        "3MF", std::vector<std::string>{".3mf"},
        "3MF importer is not implemented yet."));
    registerImporter(std::make_unique<StubMeshImporter>(
        "AMF", std::vector<std::string>{".amf"},
        "AMF importer is not implemented yet."));
}

std::string FileImporter::normalizeExtension(const std::string& extOrPath) {
    if (extOrPath.empty()) return "";
    if (extOrPath[0] == '.') return toLower(extOrPath);
    return extensionFromPath(extOrPath);
}
