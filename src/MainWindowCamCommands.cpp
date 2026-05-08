#include "MainWindow.h"

bool MainWindow::handleCamCommand(int id) {
    switch (id) {
    case IDM_MACHINE_BACKPLOT:     runBackplot();           return true;
    case IDM_MACHINE_VERIFY:       runVerify();             return true;
    case IDM_MACHINE_SIM:          runMachineSim();         return true;
    case IDM_MACHINE_POST:         postProcess();           return true;
    case IDM_MACHINE_GEN_POCKET:   generateToolpathPocket(); return true;
    case IDM_MACHINE_GEN_CONTOUR:  generateToolpathContour(); return true;
    case IDM_MACHINE_CHAMFER:      generateToolpathChamfer(); return true;
    case IDM_MACHINE_THREAD:       generateToolpathThread();  return true;
    case IDM_MACHINE_PROBE_Z:      probeZSurface();         return true;
    case IDM_MACHINE_PROBE_BORE:   probeBoreCenter();       return true;
    case IDM_MACHINE_PROBE_CORNER: probeCorner();           return true;
    case IDM_MACHINE_3D_WATERLINE: generate3DWaterline();   return true;
    case IDM_MACHINE_3D_SCALLOP:   generate3DScallop();     return true;
    case IDM_MACHINE_3D_RASTER:    generate3DRaster();      return true;
    case IDM_MACHINE_5AXIS:        generate5AxisSwarf();    return true;
    case IDM_MACHINE_REGEN:        regenerateAllToolpaths(); return true;
    case IDM_MACHINE_SUMMARY:      showMachiningSummary();  return true;
    default:
        return false;
    }
}
