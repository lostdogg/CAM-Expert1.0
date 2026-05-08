#include "MainWindow.h"

#include <commctrl.h>

bool MainWindow::handleSolidCommand(int id) {
    switch (id) {
    case IDM_SOLID_EXTRUDE:  createSolidBox();      return true;
    case IDM_SOLID_REVOLVE:  createSolidCylinder(); return true;
    case IDM_SOLID_SPHERE:   createSolidSphere();   return true;
    case IDM_SOLID_SWEEP:    createSolidSweep();    return true;
    case IDM_SOLID_LOFT:     createSolidLoft();     return true;
    case IDM_SOLID_THICKEN:  createSolidThicken();  return true;
    case IDM_SOLID_BLOCK:    createSolidBlock();    return true;
    case IDM_SOLID_CYLINDER: createSolidCylinder(); return true;
    case IDM_SOLID_CONE:     createSolidCone();     return true;
    case IDM_SOLID_TORUS:    createSolidTorus();    return true;
    case IDM_SOLID_FILLET:
    case IDM_SOLID_CHAMFER:
    case IDM_SOLID_SHELL:
    case IDM_SOLID_DRAFT:
    case IDM_SOLID_TRIM:
        solidModify(id);
        return true;
    case IDM_SOLID_UNION:
    case IDM_SOLID_SUBTRACT:
    case IDM_SOLID_INTERSECT:
        solidBooleanOp(id);
        return true;
    case IDM_SOLID_HOLE:
        solidHole();
        return true;
    case IDM_SOLID_IMPRESS:
        solidImpression();
        return true;
    case IDM_SOLID_FROM_SURF:
        solidFromSurfaces();
        return true;
    case IDM_SOLID_TREE_EDIT:
    case IDM_SOLID_TREE_SUPPRESS:
    case IDM_SOLID_TREE_DELETE:
        if (m_hSolidsTree) {
            HTREEITEM hSel = TreeView_GetSelection(m_hSolidsTree);
            if (hSel) {
                TVITEMW tvi = {};
                tvi.mask = TVIF_PARAM;
                tvi.hItem = hSel;
                TreeView_GetItem(m_hSolidsTree, &tvi);
                int solidIdx = static_cast<int>((tvi.lParam >> 16) & 0xFFFF);
                int featureIdx = static_cast<int>(tvi.lParam & 0xFFFF);
                if (featureIdx != 0xFFFF && m_solidsMgr) {
                    if (id == IDM_SOLID_TREE_EDIT) {
                        solidEditFeature(solidIdx, featureIdx);
                    } else if (id == IDM_SOLID_TREE_SUPPRESS) {
                        bool suppressed =
                            m_solidsMgr->getFeature(solidIdx, featureIdx).suppressed;
                        m_solidsMgr->suppressFeature(solidIdx, featureIdx, !suppressed);
                    } else {
                        m_solidsMgr->removeFeature(solidIdx, featureIdx);
                    }
                }
            }
        }
        return true;
    default:
        return false;
    }
}
