// PlayerJellyfinBar.cpp
#include "stdafx.h"
#include "mplayerc.h"
#include "mainfrm.h"
#include "PlayerJellyfinBar.h"

// CPlayerJellyfinBar

IMPLEMENT_DYNAMIC(CPlayerJellyfinBar, baseCPlayerJellyfinBar)
CPlayerJellyfinBar::CPlayerJellyfinBar()
{
}

CPlayerJellyfinBar::~CPlayerJellyfinBar()
{
}

BOOL CPlayerJellyfinBar::Create(CWnd* pParentWnd)
{
	if(!baseCPlayerJellyfinBar::Create(_T("Jellyfin Library"), pParentWnd, 0))
		return FALSE;

	m_dlg.Create(this);
	m_dlg.ShowWindow(SW_SHOWNORMAL);
	CRect client;
	GetClientRect(client);
	m_dlg.MoveWindow(client);

	CRect r;
	m_dlg.GetWindowRect(r);
	m_szMinVert = m_szVert = r.Size();
	m_szMinHorz = m_szHorz = r.Size();
	m_szMinFloat = m_szFloat = r.Size();

	return TRUE;
}

BOOL CPlayerJellyfinBar::PreTranslateMessage(MSG* pMsg)
{
	if(IsWindow(pMsg->hwnd) && IsVisible() && pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		if(IsDialogMessage(pMsg))
			return TRUE;
	}

	return __super::PreTranslateMessage(pMsg);
}

void CPlayerJellyfinBar::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	if(IsWindow(m_dlg))
	{
		CRect r;
		GetClientRect(&r);
		m_dlg.MoveWindow(r);
	}
}

BEGIN_MESSAGE_MAP(CPlayerJellyfinBar, baseCPlayerJellyfinBar)
	ON_WM_SIZE()
END_MESSAGE_MAP()
