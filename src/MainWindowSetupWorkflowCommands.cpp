#include "MainWindow.h"

bool MainWindow::handleSetupWorkflowCommand(int id) {
    switch (id) {
    case IDM_SETUP_CONSTRAINTS:  setupConstraints();       return true;
    case IDM_SETUP_POST_PROFILE: setupPostProfile();       return true;
    case IDM_SETUP_TOOL_DB:      setupToolDatabase();      return true;
    case IDM_SETUP_PERF_MODE:    setupPerformanceMode();   return true;
    case IDM_SETUP_GUIDANCE:     showWorkflowGuidance();   return true;
    case IDM_SETUP_AUDIT_LOG:    showOperationAuditTrail(); return true;
    default:
        return false;
    }
}
