// JellyfinClient.cpp
//
// HTTP transport notes:
// This codebase has no vendored HTTP client library (no libcurl, no
// WinHTTP wrapper) beyond what MFC/ATL already provide. The only existing
// precedent (ISDb.cpp's OpenUrl()) uses CInternetSession::OpenURL(), which
// is GET-only and does not support custom request headers, so it can't be
// reused as-is for Jellyfin's authenticated JSON API (which needs
// "X-Emby-Authorization"/"X-MediaBrowser-Token" headers and POST bodies).
//
// Instead we use the lower-level MFC WinInet wrapper trio
// CInternetSession -> CHttpConnection -> CHttpFile, which supports custom
// verbs, custom headers and request bodies while still being part of the
// MFC that ships with VS2005 and works down to old Windows releases (same
// WinInet.dll family already linked into this app for ISDb.cpp).
#include "stdafx.h"
#include "mplayerc.h"
#include "JellyfinClient.h"
#include <atlenc.h>
#include <atlutil.h>

namespace
{
	CString UrlEncode(LPCTSTR s)
	{
		CStringA in(s);
		CString out;
		for(int i = 0; i < in.GetLength(); i++)
		{
			unsigned char c = (unsigned char)in[i];
			if(isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
				out += (TCHAR)c;
			else
			{
				CString hex;
				hex.Format(_T("%%%02X"), c);
				out += hex;
			}
		}
		return out;
	}
}

CJellyfinClient::CJellyfinClient()
{
	m_deviceId = NewGuid();
}

CJellyfinClient::~CJellyfinClient()
{
}

void CJellyfinClient::SetServer(CString serverUrl)
{
	m_serverUrl = serverUrl;
	m_serverUrl.TrimRight(_T("/"));
}

CString CJellyfinClient::NewGuid()
{
	GUID guid;
	CoCreateGuid(&guid);
	CString s;
	s.Format(_T("%08lx%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x"),
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	return s;
}

CString CJellyfinClient::BuildAuthHeader() const
{
	// Client identification + auth header. Historically Jellyfin/Emby
	// accepted "X-Emby-Authorization" (with the token in a separate
	// "X-MediaBrowser-Token" header), but modern Jellyfin servers (tested
	// against Jellyfin 12.0.0) reject "X-Emby-Authorization" outright
	// (HTTP 400) and instead require the single standard "Authorization"
	// header, with the token embedded directly in it via Token="...".
	// Verified live against a real server: X-Emby-Authorization -> 400,
	// Authorization (with embedded Token) -> 200.
	CString h;
	if(m_accessToken.IsEmpty())
	{
		h.Format(
			_T("Authorization: MediaBrowser Client=\"MPC98\", Device=\"Windows\", DeviceId=\"%s\", Version=\"") MPC98_VERSION _T("\"\r\n"),
			m_deviceId);
	}
	else
	{
		h.Format(
			_T("Authorization: MediaBrowser Client=\"MPC98\", Device=\"Windows\", DeviceId=\"%s\", Version=\"") MPC98_VERSION _T("\", Token=\"%s\"\r\n"),
			m_deviceId, m_accessToken);
	}
	return h;
}

bool CJellyfinClient::DoRequest(LPCTSTR verb, CString urlPath, const CStringA& jsonBody,
	CStringA& responseBody, CString& error, DWORD* pStatusCode)
{
	responseBody.Empty();
	error.Empty();

	CUrl url;
	if(!url.CrackUrl(m_serverUrl))
	{
		error = _T("Invalid server URL");
		return false;
	}

	bool https = (_tcsicmp(url.GetSchemeName(), _T("https")) == 0);
	INTERNET_PORT port = url.GetPortNumber();
	if(port == 0) port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

	try
	{
		CInternetSession sess(_T("MPC98 Jellyfin Client"));
		CAutoPtr<CHttpConnection> pConn(sess.GetHttpConnection(url.GetHostName(), port));

		DWORD flags = INTERNET_FLAG_EXISTING_CONNECT | INTERNET_FLAG_TRANSFER_BINARY | INTERNET_FLAG_NO_CACHE_WRITE;
		if(https) flags |= INTERNET_FLAG_SECURE;

		CAutoPtr<CHttpFile> pFile(pConn->OpenRequest(verb, urlPath, NULL, 1, NULL, NULL, flags));

		CString headers = BuildAuthHeader();
		headers += _T("Accept: application/json\r\n");
		if(!jsonBody.IsEmpty())
			headers += _T("Content-Type: application/json\r\n");

		pFile->AddRequestHeaders(headers);

		BOOL ok = pFile->SendRequest(NULL, 0,
			jsonBody.IsEmpty() ? NULL : (LPVOID)(LPCSTR)jsonBody,
			jsonBody.GetLength());

		if(!ok)
		{
			error = _T("Request failed to send");
			return false;
		}

		DWORD status = 0;
		pFile->QueryInfoStatusCode(status);
		if(pStatusCode) *pStatusCode = status;

		char buff[4096];
		UINT len;
		while((len = pFile->Read(buff, sizeof(buff))) > 0)
			responseBody += CStringA(buff, len);

		pFile->Close();

		if(status < 200 || status >= 300)
		{
			// Include a snippet of the response body: Jellyfin usually
			// returns a helpful plain-text/JSON message explaining what
			// was wrong (e.g. "Error processing request."), which is
			// much more actionable than the bare status code alone.
			CString body(responseBody.Left(200));
			body.Replace(_T("\r"), _T(" "));
			body.Replace(_T("\n"), _T(" "));
			if(body.IsEmpty())
				error.Format(_T("HTTP status %lu"), status);
			else
				error.Format(_T("HTTP status %lu: %s"), status, (LPCTSTR)body);
			return false;
		}
	}
	catch(CInternetException* ie)
	{
		TCHAR msg[512] = {0};
		ie->GetErrorMessage(msg, 512);
		error = msg;
		ie->Delete();
		return false;
	}

	return true;
}

bool CJellyfinClient::AuthenticateByName(CString username, CString password, CString& error)
{
	CStringA body;
	body.Format("{\"Username\":\"%s\",\"Pw\":\"%s\"}",
		(LPCSTR)CStringA(username), (LPCSTR)CStringA(password));

	CStringA response;
	DWORD status = 0;
	if(!DoRequest(_T("POST"), _T("/Users/AuthenticateByName"), body, response, error, &status))
		return false;

	CJsonValue root;
	if(!ParseJson(response, root) || !root.IsObject())
	{
		error = _T("Unexpected response from server");
		return false;
	}

	CString token = root["AccessToken"].AsString();
	CString userId = root["User"]["Id"].AsString();
	if(token.IsEmpty() || userId.IsEmpty())
	{
		error = _T("Login failed (invalid username or password)");
		return false;
	}

	m_accessToken = token;
	m_userId = userId;
	return true;
}

bool CJellyfinClient::ParseItemList(const CJsonValue& root, CAtlArray<CJellyfinItem>& items)
{
	const CJsonValue& arr = root.IsArray() ? root : root["Items"];
	if(!arr.IsArray())
		return false;

	for(size_t i = 0; i < arr.arrayValue.GetCount(); i++)
	{
		const CJsonValue& e = arr.arrayValue[i];
		CJellyfinItem item;
		item.id = e["Id"].AsString();
		item.name = e["Name"].AsString();
		item.type = e["Type"].AsString();
		item.isFolder = e["IsFolder"].AsBool(false);
		item.runtimeTicks = e["RunTimeTicks"].AsInt64();
		item.childCount = (int)e["ChildCount"].AsNumber(0);
		if(e.HasMember("MediaSources") && e["MediaSources"].IsArray() && e["MediaSources"].arrayValue.GetCount() > 0)
			item.mediaSourceId = e["MediaSources"].arrayValue[0]["Id"].AsStringA();
		items.Add(item);
	}
	return true;
}

bool CJellyfinClient::GetLibraries(CAtlArray<CJellyfinItem>& items, CString& error)
{
	items.RemoveAll();
	CString path;
	path.Format(_T("/Users/%s/Views?Fields=ChildCount"), m_userId);

	CStringA response;
	if(!DoRequest(_T("GET"), path, "", response, error))
		return false;

	CJsonValue root;
	if(!ParseJson(response, root))
	{
		error = _T("Failed to parse library list");
		return false;
	}
	return ParseItemList(root, items);
}

bool CJellyfinClient::GetItems(CString parentId, CAtlArray<CJellyfinItem>& items, CString& error)
{
	items.RemoveAll();
	CString path;
	path.Format(_T("/Users/%s/Items?ParentId=%s&SortBy=SortName&Fields=MediaSources,RunTimeTicks,ChildCount"),
		m_userId, UrlEncode(parentId));

	CStringA response;
	if(!DoRequest(_T("GET"), path, "", response, error))
		return false;

	CJsonValue root;
	if(!ParseJson(response, root))
	{
		error = _T("Failed to parse item list");
		return false;
	}
	return ParseItemList(root, items);
}

bool CJellyfinClient::GetPlaybackInfo(const CJellyfinItem& item, CJellyfinPlaybackInfo& info, CString& error)
{
	info = CJellyfinPlaybackInfo();
	CString path;
	path.Format(_T("/Items/%s/PlaybackInfo?UserId=%s"), item.id, m_userId);

	CStringA response;
	if(!DoRequest(_T("GET"), path, "", response, error))
		return false;

	CJsonValue root;
	if(!ParseJson(response, root) || !root["MediaSources"].IsArray() || root["MediaSources"].arrayValue.GetCount() == 0)
	{
		error = _T("PlaybackInfo did not include a media source");
		return false;
	}

	const CJsonValue& source = root["MediaSources"].arrayValue[0];
	info.mediaSourceId = source["Id"].AsStringA();
	info.container = source["Container"].AsString();
	info.supportsDirectPlay = source["SupportsDirectPlay"].AsBool(false);
	return true;
}

bool CJellyfinClient::GetDirectPlayStreamUrl(const CJellyfinItem& item, const CJellyfinPlaybackInfo& info,
	CString& streamUrl, CString& playSessionId, CStringA& mediaSourceId, CString& error)
{
	if(!info.supportsDirectPlay)
	{
		error = _T("Jellyfin does not allow direct play for this item");
		return false;
	}

	if(playSessionId.IsEmpty())
		playSessionId = NewGuid();
	mediaSourceId = info.mediaSourceId;

	CString container = info.container;
	container.MakeLower();
	if(container == _T("matroska")) container = _T("mkv");
	if(container.IsEmpty()) container = _T("ts");

	CString path;
	path.Format(_T("/Videos/%s/stream.%s?static=true&PlaySessionId=%s&api_key=%s&MPCJellyfin=1"),
		item.id, container, playSessionId, m_accessToken);
	if(!mediaSourceId.IsEmpty())
		path += _T("&MediaSourceId=") + CString(mediaSourceId);

	streamUrl = m_serverUrl + path;
	return true;
}

bool CJellyfinClient::GetHlsStreamUrl(const CJellyfinItem& item, CString& streamUrl,
	CString& playSessionId, CStringA& mediaSourceId, CString& error, __int64 startTimeTicks)
{
    if(playSessionId.IsEmpty()) playSessionId = NewGuid();
    mediaSourceId = item.mediaSourceId;
    if(mediaSourceId.IsEmpty())
    {
        CJellyfinPlaybackInfo info;
        if(!GetPlaybackInfo(item, info, error)) return false;
        mediaSourceId = info.mediaSourceId;
    }
    if(mediaSourceId.IsEmpty())
    {
        error = _T("Jellyfin HLS requires a media source ID");
        return false;
    }
	AppSettings& s = AfxGetAppSettings();

	CString path;
    path.Format(_T("/Videos/%s/master.m3u8?MediaSourceId=%s&VideoCodec=%s&AudioCodec=%s&Container=ts&SegmentContainer=ts&PlaySessionId=%s"),
		item.id, CString(mediaSourceId), s.JellyfinVideoCodec, s.JellyfinAudioCodec, playSessionId);
	if(s.JellyfinVideoBitrate > 0) { CString q; q.Format(_T("&VideoBitRate=%d"), s.JellyfinVideoBitrate * 1000); path += q; }
	if(s.JellyfinAudioBitrate > 0) { CString q; q.Format(_T("&AudioBitRate=%d"), s.JellyfinAudioBitrate * 1000); path += q; }
	if(s.JellyfinMaxStreamingBitrate > 0) { CString q; q.Format(_T("&MaxStreamingBitrate=%d"), s.JellyfinMaxStreamingBitrate * 1000); path += q; }
	if(s.JellyfinMaxWidth > 0) { CString q; q.Format(_T("&MaxWidth=%d"), s.JellyfinMaxWidth); path += q; }
	if(s.JellyfinMaxHeight > 0) { CString q; q.Format(_T("&MaxHeight=%d"), s.JellyfinMaxHeight); path += q; }
	if(s.JellyfinMaxFramerate > 0) { CString q; q.Format(_T("&MaxFramerate=%d"), s.JellyfinMaxFramerate); path += q; }
	if(!s.JellyfinAudioCodec.CompareNoCase(_T("mp2")) || !s.JellyfinAudioCodec.CompareNoCase(_T("mp3")))
		path += _T("&MaxAudioChannels=2&TranscodingMaxAudioChannels=2");
    if(m_serverUrl.Left(7).CompareNoCase(_T("http://")) != 0)
	{
		error = _T("HLS currently requires an http:// Jellyfin server URL");
		return false;
	}
    streamUrl = _T("hls://") + m_serverUrl.Mid(7) + path + _T("&MPCAuthToken=") + m_accessToken + _T("&MPCDeviceId=") + m_deviceId;
    if(startTimeTicks > 0)
    {
        CString q;
        q.Format(_T("&MPCStartTimeTicks=%I64d"), startTimeTicks);
        streamUrl += q;
    }
	return true;
}

bool CJellyfinClient::GetTranscodedStreamUrl(const CJellyfinItem& item, CString& streamUrl,
	CString& playSessionId, CStringA& mediaSourceId, CString& error, __int64 startTimeTicks)
{
	// Ask the server to always transcode: Static is intentionally omitted
	// (defaults to false) and we specify a codec/container pair that MPC
	// (even on very old Windows builds) can be expected to decode via its
	// own bundled DirectShow filters. This means the *server* always does
	// the decode/encode work, and MPC only ever receives a plain
	// progressive byte stream - never the original source codec, and
	// never an .m3u8/HLS segment list. MPEG-2/MP2 is deliberately used
	// instead of H.264/AAC: the real Win2K test machine's old external
	// ffdshow crashes while decoding the H.264 stream, whereas the
	// built-in/legacy DirectShow MPEG-2 path is appropriate for the
	// project's Win9x/Win2K compatibility goal. MPEG-4 was tested in
	// both MPEG-TS and AVI; the former yielded audio without video and
	// the latter cannot be rendered through the non-seekable HTTP route.
	if(playSessionId.IsEmpty())
		playSessionId = NewGuid();
	mediaSourceId = item.mediaSourceId;

	CString path;
	path.Format(
		_T("/Videos/%s/stream.%s?VideoCodec=%s&AudioCodec=%s&Container=%s&PlaySessionId=%s&StartTimeTicks=%I64d&api_key=%s&MPCJellyfin=1"),
		item.id, AfxGetAppSettings().JellyfinContainer, AfxGetAppSettings().JellyfinVideoCodec,
		AfxGetAppSettings().JellyfinAudioCodec, AfxGetAppSettings().JellyfinContainer,
		playSessionId, startTimeTicks, m_accessToken);

	AppSettings& s = AfxGetAppSettings();
	if(s.JellyfinVideoBitrate > 0) { CString q; q.Format(_T("&VideoBitRate=%d"), s.JellyfinVideoBitrate * 1000); path += q; }
	if(s.JellyfinAudioBitrate > 0) { CString q; q.Format(_T("&AudioBitRate=%d"), s.JellyfinAudioBitrate * 1000); path += q; }
	if(s.JellyfinMaxStreamingBitrate > 0) { CString q; q.Format(_T("&MaxStreamingBitrate=%d"), s.JellyfinMaxStreamingBitrate * 1000); path += q; }
	if(s.JellyfinMaxWidth > 0) { CString q; q.Format(_T("&MaxWidth=%d"), s.JellyfinMaxWidth); path += q; }
	if(s.JellyfinMaxHeight > 0) { CString q; q.Format(_T("&MaxHeight=%d"), s.JellyfinMaxHeight); path += q; }
	if(s.JellyfinMaxFramerate > 0) { CString q; q.Format(_T("&MaxFramerate=%d"), s.JellyfinMaxFramerate); path += q; }

	// MPEG audio layer II and III are legacy stereo formats. Jellyfin otherwise
	// inherits a BD source's 5.1 channel count and invokes ffmpeg with `-ac 6`,
	// which the MP2/MP3 encoders reject (ffmpeg EINVAL / Jellyfin exit code 234).
	// Keep the legacy profile playable by explicitly downmixing these codecs.
	if(!s.JellyfinAudioCodec.CompareNoCase(_T("mp2")) || !s.JellyfinAudioCodec.CompareNoCase(_T("mp3")))
		path += _T("&MaxAudioChannels=2&TranscodingMaxAudioChannels=2");

	if(!mediaSourceId.IsEmpty())
		path += _T("&MediaSourceId=") + CString(mediaSourceId);

	streamUrl = m_serverUrl + path;
	return true;
}

bool CJellyfinClient::ReportPlaybackStart(CString itemId, CString playSessionId, CStringA mediaSourceId,
	__int64 positionTicks)
{
	CStringA body;
	body.Format(
		"{\"ItemId\":\"%s\",\"PlaySessionId\":\"%s\",\"MediaSourceId\":\"%s\",\"PositionTicks\":%I64d,\"CanSeek\":true,\"IsPaused\":false,\"PlayMethod\":\"Transcode\"}",
		(LPCSTR)CStringA(itemId), (LPCSTR)CStringA(playSessionId), (LPCSTR)mediaSourceId, positionTicks);

	CStringA response;
	CString error;
	return DoRequest(_T("POST"), _T("/Sessions/Playing"), body, response, error);
}

bool CJellyfinClient::ReportPlaybackProgress(CString itemId, CString playSessionId, CStringA mediaSourceId,
	__int64 positionTicks, bool isPaused)
{
	CStringA body;
	body.Format(
		"{\"ItemId\":\"%s\",\"PlaySessionId\":\"%s\",\"MediaSourceId\":\"%s\",\"PositionTicks\":%I64d,\"IsPaused\":%s,\"PlayMethod\":\"Transcode\"}",
		(LPCSTR)CStringA(itemId), (LPCSTR)CStringA(playSessionId), (LPCSTR)mediaSourceId,
		positionTicks, isPaused ? "true" : "false");

	CStringA response;
	CString error;
	return DoRequest(_T("POST"), _T("/Sessions/Playing/Progress"), body, response, error);
}

bool CJellyfinClient::ReportPlaybackStopped(CString itemId, CString playSessionId, CStringA mediaSourceId,
	__int64 positionTicks)
{
	CStringA body;
	body.Format(
		"{\"ItemId\":\"%s\",\"PlaySessionId\":\"%s\",\"MediaSourceId\":\"%s\",\"PositionTicks\":%I64d}",
		(LPCSTR)CStringA(itemId), (LPCSTR)CStringA(playSessionId), (LPCSTR)mediaSourceId, positionTicks);

	CStringA response;
	CString error;
	return DoRequest(_T("POST"), _T("/Sessions/Playing/Stopped"), body, response, error);
}
