#pragma once
#ifndef FILE_IMPORTER_H
#define FILE_IMPORTER_H

#include "BRep.h"
#include "MeshData.h"
#include <string>
#include <memory>
#include <variant>
#include <vector>

// --------------------------------------------------------------------------
// FileImporter
//
// Handles a wide range of CAD file formats:
//   Neutral:   STEP (.step/.stp), IGES (.iges/.igs), STL (.stl), OBJ (.obj)
//   Native:    SolidWorks (.sldprt/.sldasm), AutoCAD (.dwg/.dxf),
//              Inventor (.ipt/.iam), Siemens NX (.prt)
//
// Returns either a B-Rep solid (for STEP/IGES/native) or a MeshData
// (for STL/OBJ). Internally, it auto-detects the format from the file
// extension (and file header magic bytes as a fallback).
// --------------------------------------------------------------------------

// Represent either a B-Rep solid or a triangle mesh
using ImportResult = std::variant<BRep::Solid, MeshData>;

enum class FileFormat {
    Unknown,
    STEP,       // .step / .stp  – neutral B-Rep (ISO 10303)
    IGES,       // .iges / .igs  – neutral B-Rep (ANS/ASME)
    STL,        // .stl          – triangle mesh
    OBJ,        // .obj          – Wavefront triangle mesh
    SolidWorks, // .sldprt / .sldasm
    AutoCAD,    // .dwg / .dxf
    Inventor,   // .ipt / .iam
    SiemensNX   // .prt
};

class FileImporter {
public:
    FileImporter() = default;

    // Primary entry point – detects format and imports
    ImportResult import(const std::string& filePath);

    // Last error message (empty on success)
    const std::string& lastError() const { return m_lastError; }

    // Format detection utility
    static FileFormat detectFormat(const std::string& filePath);
    static std::string formatName(FileFormat fmt);

private:
    ImportResult importSTEP(const std::string& path);
    ImportResult importIGES(const std::string& path);
    ImportResult importSTL (const std::string& path);
    ImportResult importOBJ (const std::string& path);
    ImportResult importSolidWorks(const std::string& path);
    ImportResult importAutoCAD   (const std::string& path);
    ImportResult importInventor  (const std::string& path);
    ImportResult importSiemensNX (const std::string& path);

    // ASCII / binary detection for STL
    static bool isAsciiSTL(const std::string& path);

    std::string m_lastError;
};

#endif // FILE_IMPORTER_H
