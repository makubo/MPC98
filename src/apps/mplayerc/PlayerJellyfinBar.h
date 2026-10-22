// PlayerJellyfinBar.h - docking panel for browsing/playing a Jellyfin
// media server library. Modeled on PlayerCaptureBar.h.
#pragma once

#include "JellyfinBrowserDialog.h"

#ifndef baseCPlayerJellyfinBar
#define baseCPlayerJellyfinBar CSizingControlBarG
#endif

// CPlayerJellyfinBar

class CPlayerJellyfinBar : public baseCPlayerJellyfinBar
{
	DECLARE_DYNAMIC(CPlayerJellyfinBar)

public:
	CPlayerJellyfinBar();
	virtual ~CPlayerJellyfinBar();

	BOOL Create(CWnd* pParentWnd);

public:
	CJellyfinBrowserDialog m_dlg;

protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnSize(UINT nType, int cx, int cy);

	DECLARE_MESSAGE_MAP()
};
