#include "stdafx.h"
#include "mplayerc.h"
#include "PPageJellyfin.h"

IMPLEMENT_DYNAMIC(CPPageJellyfin, CPPageBase)

CPPageJellyfin::CPPageJellyfin()
	: CPPageBase(CPPageJellyfin::IDD, CPPageJellyfin::IDD)
	, m_streamingMode(CMPlayerCApp::JFSM_PROGRESSIVE_TRANSCODE)
	, m_videoBitrate(0)
	, m_audioBitrate(0)
	, m_maxStreamingBitrate(0)
	, m_maxWidth(0)
	, m_maxHeight(0)
	, m_maxFramerate(0)
{
}

CPPageJellyfin::~CPPageJellyfin()
{
}

void CPPageJellyfin::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
	DDX_CBString(pDX, IDC_JF_VCODEC, m_videoCodec);
	DDX_CBString(pDX, IDC_JF_ACODEC, m_audioCodec);
	DDX_CBString(pDX, IDC_JF_CONTAINER, m_container);
	DDX_Text(pDX, IDC_JF_VBITRATE, m_videoBitrate);
	DDX_Text(pDX, IDC_JF_ABITRATE, m_audioBitrate);
	DDX_Text(pDX, IDC_JF_MAXBITRATE, m_maxStreamingBitrate);
	DDX_Text(pDX, IDC_JF_MAXWIDTH, m_maxWidth);
	DDX_Text(pDX, IDC_JF_MAXHEIGHT, m_maxHeight);
	DDX_Text(pDX, IDC_JF_MAXFRAMERATE, m_maxFramerate);
}

BOOL CPPageJellyfin::OnInitDialog()
{
	__super::OnInitDialog();

	CComboBox* pMode = (CComboBox*)GetDlgItem(IDC_JF_STREAMMODE);
	pMode->AddString(_T("Direct play"));
	pMode->AddString(_T("Progressive transcode"));
	pMode->AddString(_T("HLS (not available yet)"));

	CComboBox* pVideo = (CComboBox*)GetDlgItem(IDC_JF_VCODEC);
	pVideo->AddString(_T("mpeg2video"));
	pVideo->AddString(_T("mpeg1video"));
	pVideo->AddString(_T("h264"));
	pVideo->AddString(_T("mpeg4"));

	CComboBox* pAudio = (CComboBox*)GetDlgItem(IDC_JF_ACODEC);
	pAudio->AddString(_T("mp2"));
	pAudio->AddString(_T("mp3"));
	pAudio->AddString(_T("aac"));

	CComboBox* pContainer = (CComboBox*)GetDlgItem(IDC_JF_CONTAINER);
	pContainer->AddString(_T("ts"));
	pContainer->AddString(_T("avi"));
	pContainer->AddString(_T("mp4"));

	AppSettings& s = AfxGetAppSettings();
	m_streamingMode = s.JellyfinStreamingMode;
	m_videoCodec = s.JellyfinVideoCodec;
	m_audioCodec = s.JellyfinAudioCodec;
	m_container = s.JellyfinContainer;
	m_videoBitrate = s.JellyfinVideoBitrate;
	m_audioBitrate = s.JellyfinAudioBitrate;
	m_maxStreamingBitrate = s.JellyfinMaxStreamingBitrate;
	m_maxWidth = s.JellyfinMaxWidth;
	m_maxHeight = s.JellyfinMaxHeight;
	m_maxFramerate = s.JellyfinMaxFramerate;
	pMode->SetCurSel(s.JellyfinStreamingMode - 1);

	UpdateData(FALSE);
	return TRUE;
}

BOOL CPPageJellyfin::OnApply()
{
	UpdateData();

	AppSettings& s = AfxGetAppSettings();
	CComboBox* pMode = (CComboBox*)GetDlgItem(IDC_JF_STREAMMODE);
	m_streamingMode = pMode->GetCurSel() + 1;
	s.JellyfinStreamingMode = m_streamingMode;
	s.JellyfinVideoCodec = m_videoCodec;
	s.JellyfinAudioCodec = m_audioCodec;
	s.JellyfinContainer = m_container;
	s.JellyfinVideoBitrate = max(0, m_videoBitrate);
	s.JellyfinAudioBitrate = max(0, m_audioBitrate);
	s.JellyfinMaxStreamingBitrate = max(0, m_maxStreamingBitrate);
	s.JellyfinMaxWidth = max(0, m_maxWidth);
	s.JellyfinMaxHeight = max(0, m_maxHeight);
	s.JellyfinMaxFramerate = max(0, m_maxFramerate);

	return __super::OnApply();
}

BEGIN_MESSAGE_MAP(CPPageJellyfin, CPPageBase)
END_MESSAGE_MAP()
