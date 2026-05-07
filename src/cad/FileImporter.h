#pragma once
#ifndef FILE_IMPORTER_H
#define FILE_IMPORTER_H

#include "BRep.h"
#include "MeshData.h"
#include <string>
#include <memory>
#include <variant>
#include <vector>
#include <unordered_map>

// Represent either an import failure, a B-Rep solid, or a triangle mesh.
using ImportResult = std::variant<std::monostate, BRep::Solid, MeshData>;

enum class FileFormat {
    Unknown,
    STEP,       // .step / .stp
    IGES,       // .iges / .igs
    Parasolid,  // .x_t / .x_b
    STL,        // .stl
    OBJ,        // .obj
    ThreeMF,    // .3mf
    AMF,        // .amf
    SolidWorks, // .sldprt / .sldasm
    AutoCAD,    // .dwg / .dxf
    Rhino3DM,   // .3dm
    Inventor,   // .ipt / .iam
    CATIA       // .CATPart
};

enum class GeometryPriority {
    Mesh = 10,
    Precise = 100
};

class IGeometryImporter {
public:
    virtual ~IGeometryImporter() = default;
    virtual const char* name() const = 0;
    virtual GeometryPriority priority() const = 0;
    virtual std::vector<std::string> supportedExtensions() const = 0;
    virtual bool importFile(const std::string& filePath,
                            ImportResult& outResult,
                            std::string& outMessage) const = 0;
};

class FileImporter {
public:
    FileImporter();

    // Primary entry point – resolves importer by extension and imports.
    ImportResult import(const std::string& filePath);

    // Last status/error message (empty on success with no warnings)
    const std::string& lastError() const { return m_lastError; }

    // Format detection utility by file extension.
    static FileFormat detectFormat(const std::string& filePath);
    static std::string formatName(FileFormat fmt);

    // Extensibility points: register custom importers without modifying core.
    void registerImporter(std::unique_ptr<IGeometryImporter> importer);
    std::vector<std::string> supportedExtensions() const;

private:
    void registerDefaultImporters();
    static std::string normalizeExtension(const std::string& extOrPath);

    std::string m_lastError;
    std::vector<std::unique_ptr<IGeometryImporter>> m_importers;
    std::unordered_map<std::string, std::vector<const IGeometryImporter*>> m_extensionMap;
};

#endif // FILE_IMPORTER_H
