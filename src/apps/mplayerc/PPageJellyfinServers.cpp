#include "stdafx.h"
#include "mplayerc.h"
#include "JellyfinClient.h"
#include "JellyfinLoginDlg.h"
#include "PPageJellyfinServers.h"

IMPLEMENT_DYNAMIC(CPPageJellyfinServers, CPPageBase)

CPPageJellyfinServers::CPPageJellyfinServers()
    : CPPageBase(CPPageJellyfinServers::IDD, CPPageJellyfinServers::IDD)
    , m_active(0)
{
}

CPPageJellyfinServers::~CPPageJellyfinServers()
{
}

void CPPageJellyfinServers::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_JF_SERVERLIST, m_list);
}

void CPPageJellyfinServers::RefreshList()
{
    m_list.ResetContent();
    for(size_t i = 0; i < m_servers.GetCount(); i++)
    {
        CString text = m_servers[i].name.IsEmpty() ? m_servers[i].url : m_servers[i].name;
        if((int)i == m_active) text = _T("* ") + text;
        m_list.AddString(text);
    }
    if(!m_servers.IsEmpty()) m_list.SetCurSel(min(max(0, m_active), (int)m_servers.GetCount()-1));
}

BOOL CPPageJellyfinServers::OnInitDialog()
{
    __super::OnInitDialog();
    AppSettings& s = AfxGetAppSettings();
    m_servers.Copy(s.JellyfinServers);
    m_active = s.JellyfinActiveServer;
    RefreshList();
    return TRUE;
}

bool CPPageJellyfinServers::EditServer(int index)
{
    CJellyfinLoginDlg dlg;
    if(index >= 0 && index < (int)m_servers.GetCount())
    {
        dlg.m_server = m_servers[index].url;
        dlg.m_username = m_servers[index].username;
    }
    if(dlg.DoModal() != IDOK) return false;

    CJellyfinClient client;
    client.SetServer(dlg.m_server);
    if(index >= 0 && index < (int)m_servers.GetCount() && !m_servers[index].deviceId.IsEmpty())
        client.SetDeviceId(m_servers[index].deviceId);

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
    if(index >= 0 && index < (int)m_servers.GetCount()) m_servers[index] = server;
    else { m_servers.Add(server); m_active = m_servers.GetCount()-1; }
    RefreshList();
    SetModified();
    return true;
}

void CPPageJellyfinServers::OnAdd() { EditServer(-1); }
void CPPageJellyfinServers::OnEdit() { EditServer(m_list.GetCurSel()); }
void CPPageJellyfinServers::OnRemove()
{
    int i = m_list.GetCurSel();
    if(i < 0 || i >= (int)m_servers.GetCount()) return;
    m_servers.RemoveAt(i);
    if(i < m_active) m_active--;
    if(m_active >= (int)m_servers.GetCount()) m_active = max(0, (int)m_servers.GetCount()-1);
    RefreshList();
    SetModified();
}
void CPPageJellyfinServers::OnActive()
{
    int i = m_list.GetCurSel();
    if(i >= 0) { m_active = i; RefreshList(); SetModified(); }
}
void CPPageJellyfinServers::OnUpdateServerButtons(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_list.GetCurSel() >= 0);
}

BOOL CPPageJellyfinServers::OnApply()
{
    AppSettings& s = AfxGetAppSettings();
    s.JellyfinServers.Copy(m_servers);
    s.JellyfinActiveServer = m_active;
    if(!s.JellyfinServers.IsEmpty())
    {
        const AppSettings::JellyfinServer& server = s.JellyfinServers[s.JellyfinActiveServer];
        s.JellyfinServerUrl = server.url;
        s.JellyfinUsername = server.username;
        s.JellyfinUserId = server.userId;
        s.JellyfinAccessToken = server.accessToken;
        s.JellyfinDeviceId = server.deviceId;
    }
    else
    {
        // Clear the legacy single-server fields as well. Otherwise the
        // startup migration recreates a profile the user just removed.
        s.JellyfinServerUrl.Empty();
        s.JellyfinUsername.Empty();
        s.JellyfinUserId.Empty();
        s.JellyfinAccessToken.Empty();
        s.JellyfinDeviceId.Empty();
        s.JellyfinActiveServer = 0;
    }
    s.UpdateData(true);
    return __super::OnApply();
}

BEGIN_MESSAGE_MAP(CPPageJellyfinServers, CPPageBase)
    ON_BN_CLICKED(IDC_JF_SERVERADD, OnAdd)
    ON_BN_CLICKED(IDC_JF_SERVEREDIT, OnEdit)
    ON_BN_CLICKED(IDC_JF_SERVERREMOVE, OnRemove)
    ON_BN_CLICKED(IDC_JF_SERVERACTIVE, OnActive)
    ON_UPDATE_COMMAND_UI(IDC_JF_SERVEREDIT, OnUpdateServerButtons)
    ON_UPDATE_COMMAND_UI(IDC_JF_SERVERREMOVE, OnUpdateServerButtons)
    ON_UPDATE_COMMAND_UI(IDC_JF_SERVERACTIVE, OnUpdateServerButtons)
END_MESSAGE_MAP()
