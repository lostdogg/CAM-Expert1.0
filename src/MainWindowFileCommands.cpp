#include "MainWindow.h"

bool MainWindow::handleFileCommand(int id) {
    switch (id) {
    case IDM_FILE_NEW:    fileNew();            return true;
    case IDM_FILE_OPEN:   fileOpen();           return true;
    case IDM_FILE_SAVE:
    case IDM_FILE_SAVEAS: fileSave();           return true;
    case IDM_FILE_IMPORT: fileImport();         return true;
    case IDM_FILE_EXIT:   DestroyWindow(m_hwnd); return true;
    default: return false;
    }
}
