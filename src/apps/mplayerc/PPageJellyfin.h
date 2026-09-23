#pragma once

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

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();

	DECLARE_MESSAGE_MAP()
};
