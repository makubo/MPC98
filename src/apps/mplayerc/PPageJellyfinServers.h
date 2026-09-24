#pragma once

#include "mplayerc.h"
#include "PPageBase.h"

class CPPageJellyfinServers : public CPPageBase
{
    DECLARE_DYNAMIC(CPPageJellyfinServers)

    CListBox m_list;
    CAtlArray<AppSettings::JellyfinServer> m_servers;
    int m_active;

    void RefreshList();
    bool EditServer(int index);

public:
    CPPageJellyfinServers();
    virtual ~CPPageJellyfinServers();
    enum { IDD = IDD_PPAGEJELLYFINSERVERS };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual BOOL OnApply();

    afx_msg void OnAdd();
    afx_msg void OnEdit();
    afx_msg void OnRemove();
    afx_msg void OnActive();
    afx_msg void OnUpdateServerButtons(CCmdUI* pCmdUI);
    DECLARE_MESSAGE_MAP()
};
