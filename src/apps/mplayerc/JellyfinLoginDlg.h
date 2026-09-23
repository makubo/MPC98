// JellyfinLoginDlg.h
#pragma once

#include "resource.h"

class CJellyfinLoginDlg : public CDialog
{
public:
	CJellyfinLoginDlg(CWnd* pParent = NULL);
	virtual ~CJellyfinLoginDlg();

	enum { IDD = IDD_JELLYFIN_LOGIN };

	CString m_server;
	CString m_username;
	CString m_password;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();

	DECLARE_MESSAGE_MAP()
};
