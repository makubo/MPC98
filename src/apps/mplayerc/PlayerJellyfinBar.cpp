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

	CRect r;
	m_dlg.GetWindowRect(r);
	// Docked bars use these defaults, while a detached library keeps the
	// natural dialog size below.
	m_szVert = CSize(150, 90);
	m_szHorz = CSize(150, 90);
	m_szFloat = r.Size();
	// Five standard tree rows plus the dialog margins, with a compact dock width.
	m_szMinVert = m_szMinHorz = m_szMinFloat = CSize(150, 90);
	m_bFixedFloat = false;

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
	if(::IsWindow(m_dlg.m_hWnd) && cx > 0 && cy > 0)
	{
		CRect r;
		GetClientRect(r);
		m_dlg.MoveWindow(r);
	}
}

BEGIN_MESSAGE_MAP(CPlayerJellyfinBar, baseCPlayerJellyfinBar)
	ON_WM_SIZE()
END_MESSAGE_MAP()
