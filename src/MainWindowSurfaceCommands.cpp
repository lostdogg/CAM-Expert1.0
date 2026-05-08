#include "MainWindow.h"

bool MainWindow::handleSurfaceCommand(int id) {
    switch (id) {
    case IDM_SURF_LOFT:          surfaceLoft();          return true;
    case IDM_SURF_REVOLVE:       surfaceRevolve();       return true;
    case IDM_SURF_FILLET:        surfaceFillet();        return true;
    case IDM_SURF_OFFSET:        surfaceOffset();        return true;
    case IDM_SURF_TRIM:          surfaceTrim();          return true;
    case IDM_SURF_UNTRIM:        surfaceUntrim();        return true;
    case IDM_SURF_EXTEND:        surfaceExtend();        return true;
    case IDM_SURF_FLAT_BOUNDARY: surfaceFlatBoundary();  return true;
    case IDM_SURF_SWEPT:         surfaceSwept();         return true;
    case IDM_SURF_NET:           surfaceNet();           return true;
    case IDM_SURF_FENCE:         surfaceFence();         return true;
    case IDM_SURF_DRAFT_SURF:    surfaceDraft();         return true;
    case IDM_SURF_TRIM_TO_SURF:  surfaceTrimToSurface(); return true;
    case IDM_SURF_FROM_SOLID:    surfaceFromSolid();     return true;
    default:
        return false;
    }
}
