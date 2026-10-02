#include "stdafx.h"
#include "mplayerc.h"
#include "JellyfinClient.h"
#include "JellyfinLoginDlg.h"
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
	DDX_Text(pDX, IDC_JF_ASAMPLERATE, m_audioSampleRate);
	DDX_Text(pDX, IDC_JF_ACHANNELS, m_audioChannels);
	DDX_Control(pDX, IDC_JF_SERVERLIST, m_servers);
}

void CPPageJellyfin::RefreshServers()
{
	m_servers.ResetContent();
	for(size_t i = 0; i < m_serverProfiles.GetCount(); i++)
		m_servers.AddString(m_serverProfiles[i].name.IsEmpty() ? m_serverProfiles[i].url : m_serverProfiles[i].name);
	if(!m_serverProfiles.IsEmpty()) m_servers.SetCurSel(0);
}

bool CPPageJellyfin::EditServer(int index)
{
	CJellyfinLoginDlg dlg;
	if(index >= 0 && index < (int)m_serverProfiles.GetCount())
	{
		dlg.m_server = m_serverProfiles[index].url;
		dlg.m_username = m_serverProfiles[index].username;
	}
	if(dlg.DoModal() != IDOK) return false;

	CJellyfinClient client;
	client.SetServer(dlg.m_server);
	if(index >= 0 && index < (int)m_serverProfiles.GetCount() && !m_serverProfiles[index].deviceId.IsEmpty())
		client.SetDeviceId(m_serverProfiles[index].deviceId);

	CString error;
	if(!client.AuthenticateByName(dlg.m_username, dlg.m_password, error))
	{
		AfxMessageBox(_T("Jellyfin login failed: ") + error);
		return false;
	}

	AppSettings::JellyfinServer server;
	server.name = dlg.m_server;
	server.url = dlg.m_server;
	server.username = dlg.m_username;
	server.userId = client.GetUserId();
	server.accessToken = client.GetAccessToken();
	server.deviceId = client.GetDeviceId();
	if(index >= 0 && index < (int)m_serverProfiles.GetCount()) m_serverProfiles[index] = server;
	else m_serverProfiles.Add(server);
	RefreshServers();
	SetModified();
	return true;
}

void CPPageJellyfin::UpdateStreamControls()
{
	bool enable = ((CComboBox*)GetDlgItem(IDC_JF_STREAMMODE))->GetCurSel() + 1 != CMPlayerCApp::JFSM_DIRECTPLAY_ONLY;
	const UINT controls[] = {
		IDC_JF_CONTAINER, IDC_JF_VCODEC, IDC_JF_ACODEC,
		IDC_JF_VBITRATE, IDC_JF_ABITRATE, IDC_JF_MAXBITRATE,
		IDC_JF_MAXWIDTH, IDC_JF_MAXHEIGHT, IDC_JF_MAXFRAMERATE,
		IDC_JF_ACHANNELS, IDC_JF_ASAMPLERATE
	};
	for(int i = 0; i < countof(controls); i++) GetDlgItem(controls[i])->EnableWindow(enable);

	if(enable) OnBitrateChanged();
}

BOOL CPPageJellyfin::OnInitDialog()
{
	__super::OnInitDialog();

	CComboBox* pMode = (CComboBox*)GetDlgItem(IDC_JF_STREAMMODE);
	pMode->AddString(_T("Direct play"));
	pMode->AddString(_T("Progressive transcode"));
	pMode->AddString(_T("HLS"));

	CComboBox* pVideo = (CComboBox*)GetDlgItem(IDC_JF_VCODEC);
	pVideo->AddString(_T("mpeg2video"));
	pVideo->AddString(_T("mpeg1video"));

	CComboBox* pAudio = (CComboBox*)GetDlgItem(IDC_JF_ACODEC);
	pAudio->AddString(_T("mp2"));
	pAudio->AddString(_T("mp3"));
	pAudio->AddString(_T("aac"));
	pAudio->AddString(_T("ac3"));

	CComboBox* pContainer = (CComboBox*)GetDlgItem(IDC_JF_CONTAINER);
	pContainer->AddString(_T("ts"));
	pContainer->AddString(_T("mpeg"));

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
	m_audioSampleRate = s.JellyfinAudioSampleRate;
	m_audioChannels = s.JellyfinAudioChannels;
	m_serverProfiles.Copy(s.JellyfinServers);
	RefreshServers();
	pMode->SetCurSel(s.JellyfinStreamingMode - 1);

	UpdateData(FALSE);
	UpdateStreamControls();
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
	s.JellyfinAudioSampleRate = max(0, m_audioSampleRate);
	s.JellyfinAudioChannels = max(0, m_audioChannels);
	s.JellyfinServers.Copy(m_serverProfiles);
	s.JellyfinServerUrl.Empty();
	s.JellyfinUsername.Empty();
	s.JellyfinUserId.Empty();
	s.JellyfinAccessToken.Empty();
	s.JellyfinDeviceId.Empty();
	s.UpdateData(true);

	return __super::OnApply();
}

void CPPageJellyfin::OnAddServer() { EditServer(-1); }
void CPPageJellyfin::OnEditServer() { EditServer(m_servers.GetCurSel()); }
void CPPageJellyfin::OnRemoveServer()
{
	int index = m_servers.GetCurSel();
	if(index < 0 || index >= (int)m_serverProfiles.GetCount()) return;
	m_serverProfiles.RemoveAt(index);
	RefreshServers();
	SetModified();
}

void CPPageJellyfin::OnUpdateServerButtons(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(m_servers.GetCurSel() >= 0);
}

void CPPageJellyfin::OnStreamModeChanged()
{
	UpdateStreamControls();
}

void CPPageJellyfin::OnBitrateChanged()
{
	BOOL videoOK = FALSE, audioOK = FALSE;
	UINT video = GetDlgItemInt(IDC_JF_VBITRATE, &videoOK, FALSE);
	UINT audio = GetDlgItemInt(IDC_JF_ABITRATE, &audioOK, FALSE);
	CWnd* maxBitrate = GetDlgItem(IDC_JF_MAXBITRATE);
	if(videoOK && audioOK && video > 0 && audio > 0)
	{
		SetDlgItemInt(IDC_JF_MAXBITRATE, video + audio, FALSE);
		maxBitrate->EnableWindow(FALSE);
	}
	else
	{
		maxBitrate->EnableWindow(((CComboBox*)GetDlgItem(IDC_JF_STREAMMODE))->GetCurSel() + 1 != CMPlayerCApp::JFSM_DIRECTPLAY_ONLY);
	}
}

BEGIN_MESSAGE_MAP(CPPageJellyfin, CPPageBase)
	ON_BN_CLICKED(IDC_JF_SERVERADD, OnAddServer)
	ON_BN_CLICKED(IDC_JF_SERVEREDIT, OnEditServer)
	ON_BN_CLICKED(IDC_JF_SERVERREMOVE, OnRemoveServer)
	ON_UPDATE_COMMAND_UI(IDC_JF_SERVEREDIT, OnUpdateServerButtons)
	ON_UPDATE_COMMAND_UI(IDC_JF_SERVERREMOVE, OnUpdateServerButtons)
	ON_CBN_SELCHANGE(IDC_JF_STREAMMODE, OnStreamModeChanged)
	ON_EN_CHANGE(IDC_JF_VBITRATE, OnBitrateChanged)
	ON_EN_CHANGE(IDC_JF_ABITRATE, OnBitrateChanged)
END_MESSAGE_MAP()
