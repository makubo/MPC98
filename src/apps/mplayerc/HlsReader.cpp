#include "stdafx.h"
#include <afxinet.h>
#include "HlsReader.h"

#define HLS_INITIAL_BUFFER (512*1024)

void CHlsStream::Log(const CString& text) const
{
    TCHAR module[MAX_PATH];
    if(!GetModuleFileName(NULL, module, MAX_PATH)) return;
    CString path(module);
    int slash = path.ReverseFind(_T('\\'));
    if(slash < 0) return;
    path = path.Left(slash + 1) + _T("hls_debug.txt");
    FILE* f = _tfopen(path, _T("at"));
    if(f) { _ftprintf(f, _T("%s\n"), text); fclose(f); }
}

void CHlsStream::SaveDiagnosticCopy() const
{
    if(m_tempFile.IsEmpty()) return;
    TCHAR module[MAX_PATH];
    if(!GetModuleFileName(NULL, module, MAX_PATH)) return;
    CString path(module);
    int slash = path.ReverseFind(_T('\\'));
    if(slash < 0) return;
    path = path.Left(slash + 1) + _T("hls_last.ts");
    CopyFile(m_tempFile, path, FALSE);
}

CHlsReader::CHlsReader(IUnknown* pUnk, HRESULT* phr)
    : CAsyncReader(NAME("CHlsReader"), pUnk, &m_stream, phr, __uuidof(this))
{
    if(phr) *phr = S_OK;
}

CHlsReader::~CHlsReader()
{
}

STDMETHODIMP CHlsReader::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);
    return QI(IFileSourceFilter) __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CHlsReader::Load(LPCOLESTR pszFileName, const AM_MEDIA_TYPE* pmt)
{
    if(!m_stream.Load(pszFileName)) return E_FAIL;

    m_fn = pszFileName;
    CMediaType mt;
    mt.majortype = MEDIATYPE_Stream;
    mt.subtype = MEDIASUBTYPE_MPEG2_TRANSPORT;
    m_mt = mt;
    return S_OK;
}

STDMETHODIMP CHlsReader::GetCurFile(LPOLESTR* ppszFileName, AM_MEDIA_TYPE* pmt)
{
    if(!ppszFileName) return E_POINTER;
    if(!(*ppszFileName = (LPOLESTR)CoTaskMemAlloc((m_fn.GetLength()+1)*sizeof(WCHAR)))) return E_OUTOFMEMORY;
    wcscpy(*ppszFileName, m_fn);
    return S_OK;
}

CHlsStream::CHlsStream()
    : m_hFile(INVALID_HANDLE_VALUE), m_pos(0), m_len(0), m_startTicks(0), m_finished(false), m_failed(false)
{
}

CHlsStream::~CHlsStream()
{
    Clear();
}

void CHlsStream::Clear()
{
    if(CAMThread::ThreadExists())
    {
        CAMThread::CallWorker(CMD_EXIT);
        CAMThread::Close();
    }

    if(m_hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }
    if(!m_tempFile.IsEmpty()) DeleteFile(m_tempFile);
    m_tempFile.Empty();
    m_pos = m_len = 0;
    m_startTicks = 0;
    m_finished = m_failed = false;
}

CString CHlsStream::ResolveUrl(const CString& baseUrl, const CStringA& relativeA) const
{
    CString relative(relativeA);
    relative.Trim();
    if(relative.Find(_T("://")) >= 0) return relative;

    int query = baseUrl.Find(_T('?'));
    CString base = query >= 0 ? baseUrl.Left(query) : baseUrl;
    CString baseQuery = query >= 0 ? baseUrl.Mid(query) : _T("");

    if(relative.Left(1) == _T("/"))
    {
        int scheme = base.Find(_T("://"));
        int hostEnd = scheme >= 0 ? base.Find(_T('/'), scheme + 3) : -1;
        CString root = hostEnd >= 0 ? base.Left(hostEnd) : base;
        return root + relative + (relative.Find(_T('?')) >= 0 ? _T("") : baseQuery);
    }

    int slash = base.ReverseFind(_T('/'));
    CString dir = slash >= 0 ? base.Left(slash + 1) : base;
    return dir + relative + (relative.Find(_T('?')) >= 0 ? _T("") : baseQuery);
}

__int64 CHlsStream::GetRuntimeTicks(const CString& url) const
{
    int i = url.Find(_T("runtimeTicks="));
    if(i < 0) return -1;
    i += 13;
    int end = url.Find(_T('&'), i);
    return _tcstoi64(url.Mid(i, end < 0 ? url.GetLength()-i : end-i), NULL, 10);
}

bool CHlsStream::DownloadUrl(CInternetSession& session, const CString& url, CStringA& text)
{
    text.Empty();
    try
    {
        CAutoPtr<CStdioFile> f(session.OpenURL(url, 1, INTERNET_FLAG_TRANSFER_BINARY | INTERNET_FLAG_EXISTING_CONNECT,
            m_authHeader.IsEmpty() ? NULL : (LPCTSTR)m_authHeader, m_authHeader.GetLength()));
        char buff[4096];
        for(int len; (len = f->Read(buff, sizeof(buff))) > 0; text += CStringA(buff, len));
        f->Close();
    }
    catch(CInternetException* e)
    {
        e->Delete();
        return false;
    }
    return true;
}

bool CHlsStream::AppendUrl(CInternetSession& session, const CString& url)
{
    try
    {
        CAutoPtr<CStdioFile> f(session.OpenURL(url, 1, INTERNET_FLAG_TRANSFER_BINARY | INTERNET_FLAG_EXISTING_CONNECT,
            m_authHeader.IsEmpty() ? NULL : (LPCTSTR)m_authHeader, m_authHeader.GetLength()));
        BYTE buff[64*1024];
        for(UINT len; (len = f->Read(buff, sizeof(buff))) > 0; )
        {
            CAutoLock lock(&m_csLock);
            if(m_hFile == INVALID_HANDLE_VALUE) return false;
            DWORD written = 0;
            SetFilePointer(m_hFile, 0, NULL, FILE_END);
            if(!WriteFile(m_hFile, buff, len, &written, NULL) || written != len) return false;
            m_len += len;
        }
        f->Close();
        CString line;
        line.Format(_T("segment complete: %d bytes total"), (int)m_len);
        Log(line);
    }
    catch(CInternetException* e)
    {
        e->Delete();
        return false;
    }
    return true;
}

bool CHlsStream::ParsePlaylist(const CString& playlistUrl, const CStringA& text, CAtlList<CString>& segments, CString& variantUrl)
{
    bool masterNext = false;
    int start = 0;
    while(start < text.GetLength())
    {
        int end = text.Find("\n", start);
        CStringA line = text.Mid(start, end < 0 ? text.GetLength()-start : end-start);
        line.Trim();
        if(line.Right(1) == "\r") line = line.Left(line.GetLength()-1);
        start = end < 0 ? text.GetLength() : end + 1;
        if(line.IsEmpty()) continue;

        if(line.Left(11) == "#EXT-X-KEY" || line.Left(11) == "#EXT-X-MAP" ||
           line.Left(16) == "#EXT-X-BYTERANGE" || line.Left(20) == "#EXT-X-DISCONTINUITY" ||
           line.Left(11) == "#EXT-X-PART")
            return false;
        if(line.Left(17) == "#EXT-X-STREAM-INF") { masterNext = true; continue; }
        if(line.Left(1) == "#") continue;

        if(masterNext)
        {
            variantUrl = ResolveUrl(playlistUrl, line);
            return true; // fixed first variant for the initial implementation
        }
        segments.AddTail(ResolveUrl(playlistUrl, line));
    }
    return !segments.IsEmpty();
}

bool CHlsStream::Load(const WCHAR* url)
{
    Clear();
    m_playlistUrl = url;
    if(m_playlistUrl.Left(6).CompareNoCase(L"hls://") == 0)
        m_playlistUrl = L"http://" + m_playlistUrl.Mid(6);

    CString playlist(m_playlistUrl);
    CString token, device;
    int i = playlist.Find(_T("MPCAuthToken="));
    if(i >= 0)
    {
        i += 13;
        int end = playlist.Find(_T('&'), i);
        token = playlist.Mid(i, end < 0 ? playlist.GetLength()-i : end-i);
    }
    i = playlist.Find(_T("MPCDeviceId="));
    if(i >= 0)
    {
        i += 12;
        int end = playlist.Find(_T('&'), i);
        device = playlist.Mid(i, end < 0 ? playlist.GetLength()-i : end-i);
    }
    i = playlist.Find(_T("MPCStartTimeTicks="));
    if(i >= 0)
    {
        i += 18;
        int end = playlist.Find(_T('&'), i);
        m_startTicks = _tcstoi64(playlist.Mid(i, end < 0 ? playlist.GetLength()-i : end-i), NULL, 10);
    }
    if(!token.IsEmpty() && !device.IsEmpty())
    {
        m_authHeader.Format(_T("Authorization: MediaBrowser Client=\"MPC98\", Device=\"Windows\", DeviceId=\"%s\", Version=\"6.4.9.1\", Token=\"%s\"\r\n"), device, token);
    }
    Log(_T("HLS load started"));

    TCHAR path[MAX_PATH], file[MAX_PATH];
    if(!GetTempPath(MAX_PATH, path) || !GetTempFileName(path, _T("MPC"), 0, file)) return false;
    m_tempFile = file;
    m_hFile = CreateFile(m_tempFile, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if(m_hFile == INVALID_HANDLE_VALUE) return false;

    CAMThread::Create();
    if(FAILED(CAMThread::CallWorker(CMD_RUN))) { Clear(); return false; }

    DWORD start = GetTickCount();
    while((DWORD)(GetTickCount() - start) < 30000 && m_len < HLS_INITIAL_BUFFER && !m_finished && !m_failed)
        Sleep(100);

    // CMpegSplitterFile probes several PAT/PMT/PES regions while creating
    // outputs. Do not expose a single short HLS segment as a source; wait
    // for a stable startup cache unless a complete small VOD has ended.
    CString status;
    status.Format(_T("HLS startup: available=%I64d finished=%d failed=%d"), m_len, m_finished, m_failed);
    Log(status);
    SaveDiagnosticCopy();
    return (m_len >= HLS_INITIAL_BUFFER || m_finished && m_len > 0) && !m_failed;
}

HRESULT CHlsStream::SetPointer(LONGLONG llPos)
{
    CAutoLock lock(&m_csLock);
    if(llPos < 0 || llPos > m_len) return E_FAIL;
    m_pos = llPos;
    return S_OK;
}

HRESULT CHlsStream::Read(PBYTE pbBuffer, DWORD dwBytesToRead, BOOL bAlign, LPDWORD pdwBytesRead)
{
    CAutoLock lock(&m_csLock);
    DWORD done = 0;
    if(m_hFile != INVALID_HANDLE_VALUE && m_pos < m_len)
    {
        DWORD want = (DWORD)min((__int64)dwBytesToRead, m_len - m_pos);
        LONG high = (LONG)(m_pos >> 32);
        SetFilePointer(m_hFile, (LONG)m_pos, &high, FILE_BEGIN);
        ReadFile(m_hFile, pbBuffer, want, &done, NULL);
        m_pos += done;
    }
    if(pdwBytesRead) *pdwBytesRead = done;
    return S_OK;
}

LONGLONG CHlsStream::Size(LONGLONG* pSizeAvailable)
{
    CAutoLock lock(&m_csLock);
    if(pSizeAvailable) *pSizeAvailable = m_len;
    // While downloading, advertise a live/growing stream. Once a VOD
    // playlist is complete, expose the known final length so splitters can
    // finish their initial random-access probe for short one-segment media.
    return m_finished ? m_len : 0;
}

DWORD CHlsStream::Alignment() { return 1; }
void CHlsStream::Lock() { m_csLock.Lock(); }
void CHlsStream::Unlock() { m_csLock.Unlock(); }

DWORD CHlsStream::ThreadProc()
{
    while(1)
    {
        DWORD cmd = GetRequest();
        if(cmd == CMD_EXIT)
        {
            Reply(S_OK);
            return 0;
        }
        if(cmd == CMD_RUN)
        {
            Reply(S_OK);
            CInternetSession session(_T("MPC98 HLS"));
            CString playlist(m_playlistUrl);
            CStringA text;
            CAtlList<CString> segments;
            CString variant;
            if(!DownloadUrl(session, playlist, text) || !ParsePlaylist(playlist, text, segments, variant))
            {
                Log(_T("HLS master playlist download/parse failed"));
                m_failed = true;
                continue;
            }
            if(!variant.IsEmpty())
            {
                segments.RemoveAll();
                if(!DownloadUrl(session, variant, text) || !ParsePlaylist(variant, text, segments, variant))
                {
                    Log(_T("HLS variant playlist download/parse failed"));
                    m_failed = true;
                    continue;
                }
            }
            if(m_startTicks > 0 && !segments.IsEmpty())
            {
                // Jellyfin's HLS segment endpoint rejects StartTimeTicks.
                // Select the first segment whose media runtime reaches the
                // requested position instead of forwarding that parameter.
                POSITION first = segments.GetHeadPosition();
                POSITION pos = segments.GetHeadPosition();
                while(pos)
                {
                    POSITION current = pos;
                    CString url = segments.GetNext(pos);
                    __int64 ticks = GetRuntimeTicks(url);
                    if(ticks >= m_startTicks)
                    {
                        first = current;
                        break;
                    }
                }
                while(segments.GetHeadPosition() != first)
                    segments.RemoveHead();
            }
            CString status;
            status.Format(_T("HLS playlist parsed: %d segments"), (int)segments.GetCount());
            Log(status);
            POSITION pos = segments.GetHeadPosition();
            while(pos)
            {
                if(CheckRequest(NULL)) break;
                if(!AppendUrl(session, segments.GetNext(pos))) { Log(_T("HLS segment download failed")); m_failed = true; break; }
            }
            m_finished = true;
        }
    }
}
