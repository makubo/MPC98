// JellyfinLoginDlg.cpp
#include "stdafx.h"
#include "JellyfinLoginDlg.h"

CJellyfinLoginDlg::CJellyfinLoginDlg(CWnd* pParent)
	: CDialog(CJellyfinLoginDlg::IDD, pParent)
{
}

CJellyfinLoginDlg::~CJellyfinLoginDlg()
{
}

void CJellyfinLoginDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_JF_SERVER, m_server);
	DDX_Text(pDX, IDC_EDIT_JF_USER, m_username);
	DDX_Text(pDX, IDC_EDIT_JF_PASS, m_password);
}

BOOL CJellyfinLoginDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	return TRUE;
}

void CJellyfinLoginDlg::OnOK()
{
	UpdateData(TRUE);
	CDialog::OnOK();
}

BEGIN_MESSAGE_MAP(CJellyfinLoginDlg, CDialog)
END_MESSAGE_MAP()
