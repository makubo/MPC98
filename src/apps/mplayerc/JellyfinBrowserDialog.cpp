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

	AppSettings& s = AfxGetAppSettings();
	if(!s.JellyfinServerUrl.IsEmpty())
	{
		m_client.SetServer(s.JellyfinServerUrl);

		// Jellyfin access tokens are associated with the client DeviceId.
		// Reusing a saved token under a newly generated DeviceId makes
		// Jellyfin 12 return a misleading HTTP 500. Older installs have no
		// stored device ID, so require a one-time re-login rather than
		// attempting that invalid token/device pairing.
		if(!s.JellyfinDeviceId.IsEmpty())
			m_client.SetDeviceId(s.JellyfinDeviceId);

		if(!s.JellyfinDeviceId.IsEmpty() && !s.JellyfinUserId.IsEmpty() && !s.JellyfinAccessToken.IsEmpty())
		{
			m_client.SetAccessToken(s.JellyfinUserId, s.JellyfinAccessToken);
			PopulateRoot();
		}
	}

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

void CJellyfinBrowserDialog::PopulateRoot()
{
	ClearTree();

	CAtlArray<CJellyfinItem> items;
	CString error;
	if(!m_client.GetLibraries(items, error))
	{
		AfxMessageBox(_T("Failed to load Jellyfin libraries: ") + error);
		return;
	}

	for(size_t i = 0; i < items.GetCount(); i++)
	{
		CJellyfinTreeItemData* pData = new CJellyfinTreeItemData();
		pData->item = items[i];
		pData->item.isFolder = true; // library "Views" are always browsable folders

		HTREEITEM hItem = m_tree.InsertItem(items[i].name, TVI_ROOT, TVI_LAST);
		m_tree.SetItemData(hItem, (DWORD_PTR)pData);

		// Placeholder child so the expand glyph shows up; replaced with
		// real children (or removed) on first expand.
		m_tree.InsertItem(_T("Loading..."), hItem, TVI_LAST);
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
	if(!m_client.GetItems(pData->item.id, items, error))
	{
		AfxMessageBox(_T("Failed to load Jellyfin items: ") + error);
		return;
	}

	for(size_t i = 0; i < items.GetCount(); i++)
	{
		CJellyfinTreeItemData* pChildData = new CJellyfinTreeItemData();
		pChildData->item = items[i];

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
	if(pFrame)
		pFrame->OpenJellyfinItem(&m_client, pData->item);
}

void CJellyfinBrowserDialog::OnLogin()
{
	CJellyfinLoginDlg dlg;
	AppSettings& s = AfxGetAppSettings();
	dlg.m_server = s.JellyfinServerUrl;
	dlg.m_username = s.JellyfinUsername;

	if(dlg.DoModal() != IDOK)
		return;

	m_client.SetServer(dlg.m_server);

	CString error;
	if(!m_client.AuthenticateByName(dlg.m_username, dlg.m_password, error))
	{
		AfxMessageBox(_T("Jellyfin login failed: ") + error);
		return;
	}

	s.JellyfinServerUrl = dlg.m_server;
	s.JellyfinUsername = dlg.m_username;
	s.JellyfinUserId = m_client.GetUserId();
	s.JellyfinAccessToken = m_client.GetAccessToken();
	s.JellyfinDeviceId = m_client.GetDeviceId();
	s.UpdateData(true);

	PopulateRoot();
}

void CJellyfinBrowserDialog::OnPlay()
{
	PlaySelectedItem();
}

void CJellyfinBrowserDialog::OnDblClk(NMHDR* pNMHDR, LRESULT* pResult)
{
	PlaySelectedItem();
	*pResult = 0;
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
	ON_BN_CLICKED(IDC_JELLYFIN_LOGIN, OnLogin)
	ON_BN_CLICKED(IDC_JELLYFIN_PLAY, OnPlay)
	ON_NOTIFY(NM_DBLCLK, IDC_JELLYFIN_TREE, OnDblClk)
	ON_NOTIFY(TVN_ITEMEXPANDING, IDC_JELLYFIN_TREE, OnItemExpanding)
	ON_WM_DESTROY()
END_MESSAGE_MAP()
