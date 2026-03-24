#include "FileImporter.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cctype>

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------
static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string extension(const std::string& path) {
    auto pos = path.rfind('.');
    return pos == std::string::npos ? "" : toLower(path.substr(pos));
}

// --------------------------------------------------------------------------
FileFormat FileImporter::detectFormat(const std::string& filePath) {
    std::string ext = extension(filePath);
    if (ext == ".step" || ext == ".stp")            return FileFormat::STEP;
    if (ext == ".iges" || ext == ".igs")            return FileFormat::IGES;
    if (ext == ".stl")                              return FileFormat::STL;
    if (ext == ".obj")                              return FileFormat::OBJ;
    if (ext == ".sldprt" || ext == ".sldasm")       return FileFormat::SolidWorks;
    if (ext == ".dwg"    || ext == ".dxf")          return FileFormat::AutoCAD;
    if (ext == ".ipt"    || ext == ".iam")          return FileFormat::Inventor;
    if (ext == ".prt")                              return FileFormat::SiemensNX;
    return FileFormat::Unknown;
}

std::string FileImporter::formatName(FileFormat fmt) {
    switch (fmt) {
    case FileFormat::STEP:       return "STEP";
    case FileFormat::IGES:       return "IGES";
    case FileFormat::STL:        return "STL";
    case FileFormat::OBJ:        return "OBJ";
    case FileFormat::SolidWorks: return "SolidWorks";
    case FileFormat::AutoCAD:    return "AutoCAD";
    case FileFormat::Inventor:   return "Inventor";
    case FileFormat::SiemensNX:  return "Siemens NX";
    default:                     return "Unknown";
    }
}

// --------------------------------------------------------------------------
ImportResult FileImporter::import(const std::string& filePath) {
    m_lastError.clear();
    FileFormat fmt = detectFormat(filePath);
    switch (fmt) {
    case FileFormat::STEP:       return importSTEP(filePath);
    case FileFormat::IGES:       return importIGES(filePath);
    case FileFormat::STL:        return importSTL(filePath);
    case FileFormat::OBJ:        return importOBJ(filePath);
    case FileFormat::SolidWorks: return importSolidWorks(filePath);
    case FileFormat::AutoCAD:    return importAutoCAD(filePath);
    case FileFormat::Inventor:   return importInventor(filePath);
    case FileFormat::SiemensNX:  return importSiemensNX(filePath);
    default:
        m_lastError = "Unknown file format: " + filePath;
        return BRep::Solid{};
    }
}

// --------------------------------------------------------------------------
// STEP import (ISO 10303) – returns a B-Rep solid.
// A production importer would use OpenCASCADE or similar; here we build a
// representative solid to demonstrate the B-Rep architecture.
// --------------------------------------------------------------------------
ImportResult FileImporter::importSTEP(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        m_lastError = "Cannot open STEP file: " + path;
        return BRep::Solid{};
    }
    // Placeholder: return a default box representing a parsed STEP solid
    auto solid = BRep::Solid::makeBox(100.0, 50.0, 25.0);
    solid.setName("STEP_Import");
    return solid;
}

// --------------------------------------------------------------------------
ImportResult FileImporter::importIGES(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        m_lastError = "Cannot open IGES file: " + path;
        return BRep::Solid{};
    }
    auto solid = BRep::Solid::makeBox(80.0, 40.0, 20.0);
    solid.setName("IGES_Import");
    return solid;
}

// --------------------------------------------------------------------------
// STL import – ASCII or binary triangle mesh
// --------------------------------------------------------------------------
bool FileImporter::isAsciiSTL(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    char buf[6] = {};
    f.read(buf, 5);
    return strncmp(buf, "solid", 5) == 0;
}

ImportResult FileImporter::importSTL(const std::string& path) {
    std::vector<Geom::Triangle> tris;

    if (isAsciiSTL(path)) {
        // --- ASCII STL ---
        std::ifstream f(path);
        std::string line, token;
        Geom::Triangle tri;
        int vIdx = 0;
        while (std::getline(f, line)) {
            std::istringstream ss(line);
            ss >> token;
            if (token == "vertex") {
                double x, y, z;
                ss >> x >> y >> z;
                tri.v[vIdx++] = {x, y, z};
                if (vIdx == 3) {
                    tris.push_back(tri);
                    vIdx = 0;
                }
            }
        }
    } else {
        // --- Binary STL ---
        std::ifstream f(path, std::ios::binary);
        char header[80];
        f.read(header, 80);
        uint32_t numTris = 0;
        f.read(reinterpret_cast<char*>(&numTris), 4);
        tris.reserve(numTris);
        for (uint32_t i = 0; i < numTris; ++i) {
            float n[3], v[9];
            uint16_t attr;
            f.read(reinterpret_cast<char*>(n), 12);
            f.read(reinterpret_cast<char*>(v), 36);
            f.read(reinterpret_cast<char*>(&attr), 2);
            Geom::Triangle tri;
            for (int j = 0; j < 3; ++j)
                tri.v[j] = {v[j*3], v[j*3+1], v[j*3+2]};
            tris.push_back(tri);
        }
    }

    if (tris.empty()) {
        m_lastError = "STL file contains no triangles: " + path;
        return MeshData{};
    }
    MeshData mesh(std::move(tris));
    mesh.setName("STL_Import");
    return mesh;
}

// --------------------------------------------------------------------------
// OBJ import (Wavefront .obj)
// --------------------------------------------------------------------------
ImportResult FileImporter::importOBJ(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        m_lastError = "Cannot open OBJ file: " + path;
        return MeshData{};
    }

    std::vector<Geom::Vec3> positions;
    std::vector<Geom::Triangle> tris;
    std::string line, token;

    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        ss >> token;
        if (token == "v") {
            double x, y, z;
            ss >> x >> y >> z;
            positions.push_back({x, y, z});
        } else if (token == "f") {
            // Handle "v", "v/vt", "v/vt/vn", "v//vn"
            std::vector<int> faceIdx;
            std::string vtok;
            while (ss >> vtok) {
                int vi = std::stoi(vtok.substr(0, vtok.find('/')));
                if (vi < 0)
                    vi = static_cast<int>(positions.size()) + vi;
                else
                    vi--;
                faceIdx.push_back(vi);
            }
            // Fan triangulation
            for (int i = 1; i + 1 < static_cast<int>(faceIdx.size()); ++i) {
                Geom::Triangle tri;
                tri.v[0] = positions[faceIdx[0]];
                tri.v[1] = positions[faceIdx[i]];
                tri.v[2] = positions[faceIdx[i+1]];
                tris.push_back(tri);
            }
        }
    }

    MeshData mesh(std::move(tris));
    mesh.setName("OBJ_Import");
    return mesh;
}

// --------------------------------------------------------------------------
// Native format stubs – production builds use vendor SDK bridges
// --------------------------------------------------------------------------
ImportResult FileImporter::importSolidWorks(const std::string& path) {
    m_lastError = "SolidWorks import requires SolidWorks SDK (eDrawings).";
    auto solid = BRep::Solid::makeBox(100.0, 50.0, 25.0);
    solid.setName("SolidWorks_Stub");
    return solid;
}

ImportResult FileImporter::importAutoCAD(const std::string& path) {
    m_lastError = "AutoCAD DWG/DXF import requires ODA library.";
    return BRep::Solid{};
}

ImportResult FileImporter::importInventor(const std::string& path) {
    m_lastError = "Inventor import requires Autodesk SDK.";
    return BRep::Solid{};
}

ImportResult FileImporter::importSiemensNX(const std::string& path) {
    m_lastError = "Siemens NX import requires NX Open API.";
    return BRep::Solid{};
}
