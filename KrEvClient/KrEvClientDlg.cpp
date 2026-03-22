#include "KrEvClientDlg.h"
#include "resource.h"
#include "..\shared\KrEvShared.h"

#include <winioctl.h>

BEGIN_MESSAGE_MAP(CKrEvClientDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_QUERY, &CKrEvClientDlg::OnBnClickedQuery)
END_MESSAGE_MAP()

CKrEvClientDlg::CKrEvClientDlg(CWnd* pParent)
    : CDialogEx(IDD_KREVCLIENT_DIALOG, pParent)
{
}

BOOL CKrEvClientDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetWindowText(L"KrEv Driver Client");
    SetDlgItemText(IDC_EDIT_OUTPUT, L"Click 'Read Driver Message' to query the driver.");
    return TRUE;
}

void CKrEvClientDlg::OnBnClickedQuery()
{
    HANDLE deviceHandle;
    KREV_MESSAGE_RESPONSE response;
    DWORD bytesReturned;
    CString text;

    deviceHandle = CreateFileW(
        KREV_WIN32_DEVICE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (deviceHandle == INVALID_HANDLE_VALUE) {
        text.Format(L"Open device failed. Win32 error: %lu", GetLastError());
        SetDlgItemText(IDC_EDIT_OUTPUT, text);
        return;
    }

    ZeroMemory(&response, sizeof(response));
    bytesReturned = 0;

    if (!DeviceIoControl(
            deviceHandle,
            IOCTL_KREV_GET_MESSAGE,
            NULL,
            0,
            &response,
            (DWORD)sizeof(response),
            &bytesReturned,
            NULL)) {
        text.Format(L"DeviceIoControl failed. Win32 error: %lu", GetLastError());
        CloseHandle(deviceHandle);
        SetDlgItemText(IDC_EDIT_OUTPUT, text);
        return;
    }

    CloseHandle(deviceHandle);

    text.Format(
        L"Driver Version: %lu\r\nReturned Bytes: %lu\r\nMessage: %s",
        response.Version,
        bytesReturned,
        response.Message);
    SetDlgItemText(IDC_EDIT_OUTPUT, text);
}
