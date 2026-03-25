#pragma once
#ifndef RESOURCE_H
#define RESOURCE_H

// Application icon IDs
#define IDI_APP_ICON    100
#define IDI_APP_ICON_SM 101

// String IDs
#define IDS_APP_TITLE   200

// -------------------------------------------------------------------------
// Dialog IDs
// -------------------------------------------------------------------------
#define IDD_PROMPT_SINGLE   300   // one-value input dialog
#define IDD_PROMPT_DOUBLE   301   // two-value input dialog (radius + height)
#define IDD_PROMPT_TRIPLE   302   // three-value input dialog (dx, dy, dz)

// Control IDs used inside the input dialogs
// (chosen to avoid collision with MainWindow control IDs 100-105)
#define IDC_PROMPT_LABEL1   410
#define IDC_PROMPT_EDIT1    411
#define IDC_PROMPT_LABEL2   412
#define IDC_PROMPT_EDIT2    413
#define IDC_PROMPT_LABEL3   414
#define IDC_PROMPT_EDIT3    415

#endif // RESOURCE_H
