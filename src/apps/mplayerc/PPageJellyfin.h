#pragma once

#include "mplayerc.h"
#include "PPageBase.h"

class CPPageJellyfin : public CPPageBase
{
	DECLARE_DYNAMIC(CPPageJellyfin)

public:
	CPPageJellyfin();
	virtual ~CPPageJellyfin();

	enum { IDD = IDD_PPAGEJELLYFIN };

	int m_streamingMode;
	CString m_videoCodec, m_audioCodec, m_container;
	int m_videoBitrate, m_audioBitrate, m_maxStreamingBitrate;
	int m_maxWidth, m_maxHeight, m_maxFramerate;
	int m_audioSampleRate, m_audioChannels;
	CListBox m_servers;
	CAtlArray<AppSettings::JellyfinServer> m_serverProfiles;

	void RefreshServers();
	bool EditServer(int index);
	void UpdateStreamControls();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();

	afx_msg void OnAddServer();
	afx_msg void OnEditServer();
	afx_msg void OnRemoveServer();
	afx_msg void OnUpdateServerButtons(CCmdUI* pCmdUI);
	afx_msg void OnStreamModeChanged();
	afx_msg void OnBitrateChanged();

	DECLARE_MESSAGE_MAP()
};
