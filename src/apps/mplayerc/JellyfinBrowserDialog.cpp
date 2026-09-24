// JellyfinBrowserDialog.cpp
#include "stdafx.h"
#include "mplayerc.h"
#include "mainfrm.h"
#include "JellyfinBrowserDialog.h"
#include "JellyfinLoginDlg.h"

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
			CJellyfinTreeItemData* pData = new CJellyfinTreeItemData();
			pData->item = items[i];
			pData->item.isFolder = true;
			pData->serverIndex = clientIndex;
			HTREEITEM hItem = m_tree.InsertItem(items[i].name, root, TVI_LAST);
			m_tree.SetItemData(hItem, (DWORD_PTR)pData);
			m_tree.InsertItem(_T("Loading..."), hItem, TVI_LAST);
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
		CJellyfinTreeItemData* pChildData = new CJellyfinTreeItemData();
		pChildData->item = items[i];
		pChildData->serverIndex = pData->serverIndex;

		HTREEITEM hItem = m_tree.InsertItem(items[i].name, hParent, TVI_LAST);
		m_tree.SetItemData(hItem, (DWORD_PTR)pChildData);

		if(items[i].isFolder)
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

	if(pData->item.isFolder)
	{
		m_tree.Expand(hItem, m_tree.GetItemState(hItem, TVIS_EXPANDED) & TVIS_EXPANDED ? TVE_COLLAPSE : TVE_EXPAND);
		return;
	}

	CMainFrame* pFrame = (CMainFrame*)AfxGetMainWnd();
	if(pFrame && pData->serverIndex >= 0 && pData->serverIndex < (int)m_clients.GetCount())
		pFrame->OpenJellyfinItem(m_clients[pData->serverIndex], pData->item);
}

void CJellyfinBrowserDialog::OnDblClk(NMHDR* pNMHDR, LRESULT* pResult)
{
	PlaySelectedItem();
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
	ON_NOTIFY(TVN_ITEMEXPANDING, IDC_JELLYFIN_TREE, OnItemExpanding)
	ON_WM_SIZE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()
