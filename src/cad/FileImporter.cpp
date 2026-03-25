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
// A production importer would use OpenCASCADE or similar; here we parse the
// STEP file header and DATA section to extract entity counts and dimension
// hints, then build a representative solid that reflects those dimensions.
// --------------------------------------------------------------------------
ImportResult FileImporter::importSTEP(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        m_lastError = "Cannot open STEP file: " + path;
        return BRep::Solid{};
    }

    // Parse the STEP file to extract useful metadata.
    // ISO 10303-21 files have:
    //   ISO-10303-21;
    //   HEADER;  ... FILE_NAME, FILE_SCHEMA ...  ENDSEC;
    //   DATA;    #1 = ENTITY(params); ...         ENDSEC;
    //   END-ISO-10303-21;

    double dimX = 100.0, dimY = 50.0, dimZ = 25.0;
    std::string productName = "STEP_Import";
    int entityCount = 0;
    bool inData = false;

    std::string line;
    while (std::getline(f, line)) {
        // Count DATA section entities for info
        if (line.find("DATA;") != std::string::npos)  { inData = true;  continue; }
        if (line.find("ENDSEC;") != std::string::npos){ inData = false; continue; }

        if (!inData) {
            // Extract product name from FILE_NAME or PRODUCT entity
            auto pos = line.find("FILE_NAME(");
            if (pos != std::string::npos) {
                auto q1 = line.find('\'', pos);
                auto q2 = (q1 != std::string::npos) ? line.find('\'', q1 + 1) : std::string::npos;
                if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1 + 1)
                    productName = line.substr(q1 + 1, q2 - q1 - 1);
            }
        }

        if (inData) {
            if (!line.empty() && line[0] == '#') ++entityCount;

            // Try to extract CARTESIAN_POINT coordinates for bounding box hints
            if (line.find("CARTESIAN_POINT") != std::string::npos) {
                auto p1 = line.find('(');
                auto p2 = line.rfind(')');
                if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                    // Look for inner tuple (#id, CARTESIAN_POINT('', (x,y,z)));
                    auto tp1 = line.find('(', p1 + 1);
                    if (tp1 != std::string::npos) {
                        std::istringstream cs(line.substr(tp1 + 1));
                        double x, y, z;
                        char comma;
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

    // Build representative solid using the extracted dimensions
    auto solid = BRep::Solid::makeBox(
        std::max(dimX, 1.0),
        std::max(dimY, 1.0),
        std::max(dimZ, 1.0));
    solid.setName(productName);

    // Store entity count in the error string as informational (not an error)
    m_lastError.clear();
    if (entityCount > 0)
        m_lastError = "Parsed " + std::to_string(entityCount)
                    + " STEP entities (representative geometry shown).";
    return solid;
}

// --------------------------------------------------------------------------
// IGES import (ANSI Y14.26M) – returns a B-Rep solid.
// Parses the IGES file start section and Global section for units and author,
// then counts entity records to build a representative solid.
// --------------------------------------------------------------------------
ImportResult FileImporter::importIGES(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        m_lastError = "Cannot open IGES file: " + path;
        return BRep::Solid{};
    }

    // IGES has fixed-column format: columns 73-80 contain section letter + seq#
    // Section letters: S=Start, G=Global, D=Directory Entry, P=Parameter Data, T=Terminate
    std::string productName = "IGES_Import";
    int paramRecords = 0;
    double dimHint   = 80.0;

    std::string line;
    while (std::getline(f, line)) {
        if (line.size() < 73) continue;
        char section = (line.size() >= 73) ? line[72] : ' ';

        if (section == 'S') {
            // Start section: first line often contains the file description
            std::string desc = line.substr(0, 72);
            // Trim trailing spaces
            auto end = desc.find_last_not_of(' ');
            if (end != std::string::npos && end > 0 && desc[0] != ' ')
                productName = desc.substr(0, end + 1);
        }
        if (section == 'P') {
            ++paramRecords;
            // Look for numeric coordinates in parameter data
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

    m_lastError.clear();
    if (paramRecords > 0)
        m_lastError = "Parsed " + std::to_string(paramRecords)
                    + " IGES parameter records (representative geometry shown).";
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
