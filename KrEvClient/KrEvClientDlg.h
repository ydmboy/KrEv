#pragma once

#include <afxwin.h>
#include <afxdlgs.h>

class CKrEvClientDlg : public CDialogEx
{
public:
    explicit CKrEvClientDlg(CWnd* pParent = nullptr);

protected:
    BOOL OnInitDialog() override;
    afx_msg void OnBnClickedQuery();

    DECLARE_MESSAGE_MAP()
};
