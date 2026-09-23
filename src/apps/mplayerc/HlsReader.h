#pragma once

#include "..\..\filters\reader\asyncreader\asyncio.h"
#include "..\..\filters\reader\asyncreader\asyncrdr.h"

// Minimal HLS reader for Jellyfin's unencrypted MPEG-TS playlists. It
// presents downloaded segments as one growing, sequential IAsyncReader
// stream so the existing internal MPEG splitter can do the demuxing.
class CHlsStream : public CAsyncStream, public CAMThread
{
    CCritSec m_csLock;
    CStringW m_playlistUrl;
    CString m_authHeader;
    CString m_tempFile;
    HANDLE m_hFile;
    __int64 m_pos, m_len;
    __int64 m_startTicks;
    bool m_finished, m_failed;

    enum { CMD_EXIT, CMD_RUN };

    void Clear();
    bool DownloadUrl(CInternetSession& session, const CString& url, CStringA& text);
    bool AppendUrl(CInternetSession& session, const CString& url);
    bool ParsePlaylist(const CString& playlistUrl, const CStringA& text, CAtlList<CString>& segments, CString& variantUrl);
    CString ResolveUrl(const CString& baseUrl, const CStringA& relative) const;
    __int64 GetRuntimeTicks(const CString& url) const;
    void Log(const CString& text) const;
    void SaveDiagnosticCopy() const;
    DWORD ThreadProc();

public:
    CHlsStream();
    virtual ~CHlsStream();

    bool Load(const WCHAR* url);
    HRESULT SetPointer(LONGLONG llPos);
    HRESULT Read(PBYTE pbBuffer, DWORD dwBytesToRead, BOOL bAlign, LPDWORD pdwBytesRead);
    LONGLONG Size(LONGLONG* pSizeAvailable);
    DWORD Alignment();
    void Lock();
    void Unlock();
};

[uuid("C6572A0C-2F1B-4A7E-A3A1-DC5BEE3D99D9")]
class CHlsReader : public CAsyncReader, public IFileSourceFilter
{
    CHlsStream m_stream;
    CStringW m_fn;

public:
    CHlsReader(IUnknown* pUnk, HRESULT* phr);
    virtual ~CHlsReader();

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);
    STDMETHODIMP Load(LPCOLESTR pszFileName, const AM_MEDIA_TYPE* pmt);
    STDMETHODIMP GetCurFile(LPOLESTR* ppszFileName, AM_MEDIA_TYPE* pmt);
};
