#include "MainWindow.h"

bool MainWindow::handleWireframeCommand(int id) {
    switch (id) {
    case IDM_WF_POINT:
    case IDM_WF_POINT_DYNAMIC:
    case IDM_WF_POINT_NODE:
    case IDM_WF_POINT_SEGMENT:
    case IDM_WF_LINE:
    case IDM_WF_LINE_CLOSEST:
    case IDM_WF_LINE_BISECT:
    case IDM_WF_LINE_PERP:
    case IDM_WF_LINE_PARALLEL:
    case IDM_WF_LINE_NORMAL:
    case IDM_WF_ARC:
    case IDM_WF_CIRCLE:
    case IDM_WF_CIRCLE_EDGE:
    case IDM_WF_ARC_TANGENT:
    case IDM_WF_ARC_ENDPOINTS:
    case IDM_WF_ARC_POLAR:
    case IDM_WF_SPLINE:
    case IDM_WF_SPLINE_AUTO:
    case IDM_WF_SPLINE_BLENDED:
    case IDM_WF_RECTANGLE:
    case IDM_WF_RECT_SHAPES:
    case IDM_WF_POLYGON:
    case IDM_WF_ELLIPSE:
    case IDM_WF_HELIX:
    case IDM_WF_BBOX:
    case IDM_WF_CURVE_ONE_EDGE:
    case IDM_WF_CURVE_ALL_EDGES:
    case IDM_WF_SILHOUETTE:
    case IDM_WF_CURVE_SLICE_PLN:
    case IDM_WF_CURVE_SLICE_CRV:
    case IDM_WF_CURVE_FLOWLINE:
    case IDM_WF_CURVE_INTERSECT:
    case IDM_WF_MOD_FILLET:
    case IDM_WF_MOD_CHAMFER:
    case IDM_WF_MOD_DYN_TRIM:
    case IDM_WF_MOD_BREAK_TWO:
    case IDM_WF_MOD_BREAK_INT:
    case IDM_WF_MOD_JOIN:
    case IDM_WF_MOD_INTERSECT:
    case IDM_WF_MOD_PROJECT:
    case IDM_WF_MOD_OFFSET:
    case IDM_WF_MOD_ROLL:
        createWireframe(id);
        return true;
    case IDM_WF_SET_CPLANE:
        wfCycleCplane();
        return true;
    case IDM_WF_SET_ZDEPTH:
        wfSetZDepth();
        return true;
    default:
        return false;
    }
}
