// JellyfinBrowserDialog.h - the embedded browsing UI for the Jellyfin
// docking bar (CPlayerJellyfinBar). Modeled on CPlayerCaptureDialog's
// relationship to CPlayerCaptureBar.
#pragma once

#include "resource.h"
#include "JellyfinClient.h"

// Per-tree-item bookkeeping. Stored via CTreeCtrl::SetItemData as a
// pointer to a heap-allocated instance owned by the dialog (freed in
// OnDestroy / whenever the tree is cleared).
struct CJellyfinTreeItemData
{
	CJellyfinItem item;
	int serverIndex;
	bool childrenLoaded;
	CJellyfinTreeItemData() : serverIndex(-1), childrenLoaded(false) {}
};

class CJellyfinBrowserDialog : public CDialog
{
public:
	CJellyfinBrowserDialog(CWnd* pParent = NULL);
	virtual ~CJellyfinBrowserDialog();

	enum { IDD = IDD_JELLYFIN_BAR };

	BOOL Create(CWnd* pParentWnd);
	void ReloadServers();

	CAtlArray<CJellyfinClient*> m_clients;

protected:
	CTreeCtrl m_tree;

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

	afx_msg void OnDblClk(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemExpanding(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();

	DECLARE_MESSAGE_MAP()

private:
	void ClearTree();
	void PopulateRoot();
	void PopulateChildren(HTREEITEM hParent);
	void PlaySelectedItem();
	void FreeItemData(HTREEITEM hItem, bool recurse);
};
