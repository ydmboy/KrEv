#include "KrEvClient.h"
#include "KrEvClientDlg.h"

BEGIN_MESSAGE_MAP(CKrEvClientApp, CWinApp)
END_MESSAGE_MAP()

CKrEvClientApp theApp;

BOOL CKrEvClientApp::InitInstance()
{
    INITCOMMONCONTROLSEX initControls;

    initControls.dwSize = sizeof(initControls);
    initControls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&initControls);

    CWinApp::InitInstance();
    AfxEnableControlContainer();

    CKrEvClientDlg dlg;
    m_pMainWnd = &dlg;
    dlg.DoModal();

    return FALSE;
}
