// JellyfinBrowserDialog.cpp
#include "stdafx.h"
#include "mplayerc.h"
#include "mainfrm.h"
#include "JellyfinBrowserDialog.h"
#include "JellyfinLoginDlg.h"

enum
{
	JF_PLAY_NOW = 1,
	JF_ADD_NEXT,
	JF_ADD_TO_PLAYLIST,
	JF_EXPAND_COLLAPSE,
	JF_PLAY_ALL,
	JF_ADD_ALL_TO_PLAYLIST
};

BEGIN_MESSAGE_MAP(CJellyfinTreeCtrl, CTreeCtrl)
	ON_WM_MBUTTONDOWN()
END_MESSAGE_MAP()

void CJellyfinTreeCtrl::OnMButtonDown(UINT nFlags, CPoint point)
{
	GetParent()->SendMessage(WM_JELLYFIN_TREE_MCLICK, 0, MAKELPARAM(point.x, point.y));
	CTreeCtrl::OnMButtonDown(nFlags, point);
}

CJellyfinBrowserDialog::CJellyfinBrowserDialog(CWnd* pParent)
	: CDialog(CJellyfinBrowserDialog::IDD, pParent)
{
}

CJellyfinBrowserDialog::~CJellyfinBrowserDialog()
{
	for(size_t i = 0; i < m_clients.GetCount(); i++) delete m_clients[i];
}

BOOL CJellyfinBrowserDialog::Create(CWnd* pParentWnd)
{
	return CDialog::Create(IDD, pParentWnd);
}

void CJellyfinBrowserDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_JELLYFIN_TREE, m_tree);
}

BOOL CJellyfinBrowserDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	ReloadServers();

	return TRUE;
}

void CJellyfinBrowserDialog::FreeItemData(HTREEITEM hItem, bool recurse)
{
	while(hItem)
	{
		CJellyfinTreeItemData* pData = (CJellyfinTreeItemData*)m_tree.GetItemData(hItem);
		delete pData;
		m_tree.SetItemData(hItem, 0);

		if(recurse)
			FreeItemData(m_tree.GetChildItem(hItem), true);

		hItem = m_tree.GetNextSiblingItem(hItem);
	}
}

void CJellyfinBrowserDialog::ClearTree()
{
	FreeItemData(m_tree.GetRootItem(), true);
	m_tree.DeleteAllItems();
}

void CJellyfinBrowserDialog::ReloadServers()
{
	ClearTree();
	for(size_t i = 0; i < m_clients.GetCount(); i++) delete m_clients[i];
	m_clients.RemoveAll();

	AppSettings& s = AfxGetAppSettings();
	for(size_t i = 0; i < s.JellyfinServers.GetCount(); i++)
	{
		const AppSettings::JellyfinServer& server = s.JellyfinServers[i];
		if(server.url.IsEmpty() || server.userId.IsEmpty() || server.accessToken.IsEmpty() || server.deviceId.IsEmpty()) continue;
		CJellyfinClient* client = new CJellyfinClient();
		client->SetServer(server.url);
		client->SetDeviceId(server.deviceId);
		client->SetAccessToken(server.userId, server.accessToken);
		m_clients.Add(client);
	}
	PopulateRoot();
}

CJellyfinClient* CJellyfinBrowserDialog::FindClient(const CString& serverUrl, const CString& deviceId) const
{
	CString url = serverUrl;
	url.TrimRight(_T("/"));

	for(size_t i = 0; i < m_clients.GetCount(); i++)
	{
		CJellyfinClient* client = m_clients[i];
		if(client && !client->GetServerUrl().CompareNoCase(url)
		&& !client->GetDeviceId().CompareNoCase(deviceId))
			return client;
	}

	return NULL;
}

void CJellyfinBrowserDialog::PopulateRoot()
{
	ClearTree();

	AppSettings& s = AfxGetAppSettings();
	for(size_t serverIndex = 0, clientIndex = 0; serverIndex < s.JellyfinServers.GetCount(); serverIndex++)
	{
		const AppSettings::JellyfinServer& server = s.JellyfinServers[serverIndex];
		if(server.url.IsEmpty() || server.userId.IsEmpty() || server.accessToken.IsEmpty() || server.deviceId.IsEmpty()) continue;
		if(clientIndex >= m_clients.GetCount()) break;

		CString label = server.url;
		int scheme = label.Find(_T("://"));
		if(scheme >= 0) label = label.Mid(scheme + 3);
		int slash = label.Find(_T('/'));
		if(slash >= 0) label = label.Left(slash);
		if(label.IsEmpty()) label = server.name;
		HTREEITEM root = m_tree.InsertItem(label, TVI_ROOT, TVI_LAST);
		CJellyfinTreeItemData* rootData = new CJellyfinTreeItemData();
		rootData->serverIndex = clientIndex;
		rootData->childrenLoaded = true;
		m_tree.SetItemData(root, (DWORD_PTR)rootData);

		CAtlArray<CJellyfinItem> items;
		CString error;
		if(!m_clients[clientIndex]->GetLibraries(items, error))
		{
			m_tree.InsertItem(_T("Failed to load libraries"), root, TVI_LAST);
			clientIndex++;
			continue;
		}
		for(size_t i = 0; i < items.GetCount(); i++)
		{
			if(items[i].isFolder && items[i].childCount == 0) continue;
			CJellyfinTreeItemData* pData = new CJellyfinTreeItemData();
			pData->item = items[i];
			pData->item.isFolder = true;
			pData->serverIndex = clientIndex;
			HTREEITEM hItem = m_tree.InsertItem(items[i].name, root, TVI_LAST);
			m_tree.SetItemData(hItem, (DWORD_PTR)pData);
			if(pData->item.childCount > 0) m_tree.InsertItem(_T("Loading..."), hItem, TVI_LAST);
		}
		m_tree.Expand(root, TVE_EXPAND);
		clientIndex++;
	}
}

void CJellyfinBrowserDialog::PopulateChildren(HTREEITEM hParent)
{
	CJellyfinTreeItemData* pData = (CJellyfinTreeItemData*)m_tree.GetItemData(hParent);
	if(!pData || pData->childrenLoaded)
		return;

	// Remove the placeholder node(s).
	FreeItemData(m_tree.GetChildItem(hParent), true);
	HTREEITEM hChild;
	while((hChild = m_tree.GetChildItem(hParent)) != NULL)
		m_tree.DeleteItem(hChild);

	CAtlArray<CJellyfinItem> items;
	CString error;
	if(pData->serverIndex < 0 || pData->serverIndex >= (int)m_clients.GetCount() || !m_clients[pData->serverIndex]->GetItems(pData->item.id, items, error))
	{
		AfxMessageBox(_T("Failed to load Jellyfin items: ") + error);
		return;
	}

	for(size_t i = 0; i < items.GetCount(); i++)
	{
		if(items[i].isFolder && items[i].childCount == 0) continue;
		CJellyfinTreeItemData* pChildData = new CJellyfinTreeItemData();
		pChildData->item = items[i];
		pChildData->serverIndex = pData->serverIndex;

		HTREEITEM hItem = m_tree.InsertItem(items[i].name, hParent, TVI_LAST);
		m_tree.SetItemData(hItem, (DWORD_PTR)pChildData);

		if(items[i].isFolder && items[i].childCount > 0)
			m_tree.InsertItem(_T("Loading..."), hItem, TVI_LAST);
	}

	pData->childrenLoaded = true;
}

void CJellyfinBrowserDialog::PlaySelectedItem()
{
	HTREEITEM hItem = m_tree.GetSelectedItem();
	if(!hItem)
		return;

	CJellyfinTreeItemData* pData = (CJellyfinTreeItemData*)m_tree.GetItemData(hItem);
	if(!pData)
		return;

	if(pData->item.isFolder || pData->item.id.IsEmpty())
	{
		m_tree.Expand(hItem, m_tree.GetItemState(hItem, TVIS_EXPANDED) & TVIS_EXPANDED ? TVE_COLLAPSE : TVE_EXPAND);
		return;
	}

	CMainFrame* pFrame = (CMainFrame*)AfxGetMainWnd();
	if(pFrame && pData->serverIndex >= 0 && pData->serverIndex < (int)m_clients.GetCount())
		pFrame->OpenJellyfinItem(m_clients[pData->serverIndex], pData->item);
}

bool CJellyfinBrowserDialog::MakePlaylistItem(CJellyfinClient* client, const CJellyfinItem& item, CPlaylistItem& pli, CString& error)
{
	CString url, playSessionId;
	CStringA mediaSourceId;
	if(!client || !client->GetTranscodedStreamUrl(item, url, playSessionId, mediaSourceId, error))
		return false;

	pli.m_fJellyfin = true;
	pli.m_label = item.name;
	pli.m_fns.AddTail(url);
	pli.m_jellyfinServerUrl = client->GetServerUrl();
	pli.m_jellyfinDeviceId = client->GetDeviceId();
	pli.m_jellyfinItemId = item.id;
	pli.m_jellyfinMediaSourceId = CString(mediaSourceId);
	pli.m_jellyfinRuntimeTicks = item.runtimeTicks;
	pli.m_duration = item.runtimeTicks;
	return true;
}

void CJellyfinBrowserDialog::CollectPlaylistItems(CJellyfinClient* client, const CJellyfinItem& item, CAtlArray<CPlaylistItem>& items, CString& errors)
{
	if(!item.isFolder && !item.id.IsEmpty())
	{
		CPlaylistItem pli;
		CString error;
		if(MakePlaylistItem(client, item, pli, error)) items.Add(pli);
		else errors += (errors.IsEmpty() ? _T("") : _T("\n")) + item.name + _T(": ") + error;
		return;
	}

	CAtlArray<CJellyfinItem> children;
	CString error;
	bool ok = item.id.IsEmpty() ? client->GetLibraries(children, error) : client->GetItems(item.id, children, error);
	if(!ok)
	{
		errors += (errors.IsEmpty() ? _T("") : _T("\n")) + item.name + _T(": ") + error;
		return;
	}

	for(size_t i = 0; i < children.GetCount(); i++)
		CollectPlaylistItems(client, children[i], items, errors);
}

void CJellyfinBrowserDialog::AppendSelectedItem(bool fNext)
{
	HTREEITEM hItem = m_tree.GetSelectedItem();
	CJellyfinTreeItemData* pData = hItem ? (CJellyfinTreeItemData*)m_tree.GetItemData(hItem) : NULL;
	if(!pData || pData->item.isFolder || pData->item.id.IsEmpty()
	|| pData->serverIndex < 0 || pData->serverIndex >= (int)m_clients.GetCount()) return;

	CPlaylistItem pli;
	CString error;
	if(!MakePlaylistItem(m_clients[pData->serverIndex], pData->item, pli, error))
	{
		AfxMessageBox(_T("Failed to add Jellyfin item: ") + error);
		return;
	}

	CMainFrame* pFrame = (CMainFrame*)AfxGetMainWnd();
	if(!pFrame) return;
	if(fNext) pFrame->InsertJellyfinItemNext(pli);
	else
	{
		CAtlArray<CPlaylistItem> items;
		items.Add(pli);
		pFrame->AppendJellyfinItems(items);
	}
}

void CJellyfinBrowserDialog::PlayOrAppendFolder(bool fPlay)
{
	HTREEITEM hItem = m_tree.GetSelectedItem();
	CJellyfinTreeItemData* pData = hItem ? (CJellyfinTreeItemData*)m_tree.GetItemData(hItem) : NULL;
	if(!pData || (!pData->item.isFolder && !pData->item.id.IsEmpty())
	|| pData->serverIndex < 0 || pData->serverIndex >= (int)m_clients.GetCount()) return;

	CAtlArray<CPlaylistItem> items;
	CString errors;
	CollectPlaylistItems(m_clients[pData->serverIndex], pData->item, items, errors);

	CMainFrame* pFrame = (CMainFrame*)AfxGetMainWnd();
	if(pFrame && items.GetCount())
	{
		if(fPlay) pFrame->PlayJellyfinItems(items);
		else pFrame->AppendJellyfinItems(items);
	}

	if(!errors.IsEmpty()) AfxMessageBox(_T("Some Jellyfin items could not be added:\n") + errors);
}

void CJellyfinBrowserDialog::OnDblClk(NMHDR* pNMHDR, LRESULT* pResult)
{
	PlaySelectedItem();
	*pResult = 0;
}

LRESULT CJellyfinBrowserDialog::OnTreeMiddleClick(WPARAM wParam, LPARAM lParam)
{
	CPoint point((short)LOWORD(lParam), (short)HIWORD(lParam));
	HTREEITEM hItem = m_tree.HitTest(point);
	if(hItem) m_tree.SelectItem(hItem);
	AppendSelectedItem(false);
	return 0;
}

void CJellyfinBrowserDialog::OnRightClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	CPoint point;
	GetCursorPos(&point);
	CPoint clientPoint = point;
	m_tree.ScreenToClient(&clientPoint);
	HTREEITEM hItem = m_tree.HitTest(clientPoint);
	if(!hItem) { *pResult = 0; return; }
	m_tree.SelectItem(hItem);

	CJellyfinTreeItemData* pData = (CJellyfinTreeItemData*)m_tree.GetItemData(hItem);
	if(!pData || pData->serverIndex < 0 || pData->serverIndex >= (int)m_clients.GetCount()) { *pResult = 0; return; }

	bool fFolder = pData->item.isFolder || pData->item.id.IsEmpty();
	CMenu menu;
	menu.CreatePopupMenu();
	if(fFolder)
	{
		menu.AppendMenu(MF_STRING, JF_EXPAND_COLLAPSE, m_tree.GetItemState(hItem, TVIS_EXPANDED) & TVIS_EXPANDED ? _T("Collapse") : _T("Expand"));
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING, JF_PLAY_ALL, _T("Play All"));
		menu.AppendMenu(MF_STRING, JF_ADD_ALL_TO_PLAYLIST, _T("Add All to Playlist"));
	}
	else
	{
		menu.AppendMenu(MF_STRING, JF_PLAY_NOW, _T("Play Now"));
		menu.AppendMenu(MF_STRING, JF_ADD_NEXT, _T("Add as Next"));
		menu.AppendMenu(MF_STRING, JF_ADD_TO_PLAYLIST, _T("Add to Playlist"));
	}

	UINT command = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
	if(command == JF_PLAY_NOW) PlaySelectedItem();
	else if(command == JF_ADD_NEXT) AppendSelectedItem(true);
	else if(command == JF_ADD_TO_PLAYLIST) AppendSelectedItem(false);
	else if(command == JF_EXPAND_COLLAPSE)
		m_tree.Expand(hItem, m_tree.GetItemState(hItem, TVIS_EXPANDED) & TVIS_EXPANDED ? TVE_COLLAPSE : TVE_EXPAND);
	else if(command == JF_PLAY_ALL) PlayOrAppendFolder(true);
	else if(command == JF_ADD_ALL_TO_PLAYLIST) PlayOrAppendFolder(false);

	*pResult = 0;
}

void CJellyfinBrowserDialog::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
	if(IsWindow(m_tree))
		m_tree.MoveWindow(4, 4, max(0, cx - 8), max(0, cy - 8));
}

void CJellyfinBrowserDialog::OnItemExpanding(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMTREEVIEW* pNMTreeView = (NMTREEVIEW*)pNMHDR;
	if(pNMTreeView->action == TVE_EXPAND)
		PopulateChildren(pNMTreeView->itemNew.hItem);
	*pResult = 0;
}

void CJellyfinBrowserDialog::OnDestroy()
{
	ClearTree();
	CDialog::OnDestroy();
}

BEGIN_MESSAGE_MAP(CJellyfinBrowserDialog, CDialog)
	ON_NOTIFY(NM_DBLCLK, IDC_JELLYFIN_TREE, OnDblClk)
	ON_NOTIFY(NM_RCLICK, IDC_JELLYFIN_TREE, OnRightClick)
	ON_MESSAGE(WM_JELLYFIN_TREE_MCLICK, OnTreeMiddleClick)
	ON_NOTIFY(TVN_ITEMEXPANDING, IDC_JELLYFIN_TREE, OnItemExpanding)
	ON_WM_SIZE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()
