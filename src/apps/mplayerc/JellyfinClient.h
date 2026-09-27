// JellyfinClient.h - a small REST client for the Jellyfin media server
// (https://jellyfin.org) API, used to browse a Jellyfin library and to
// resolve playable stream URLs. See JellyfinClient.cpp for design notes.
#pragma once

#include <atlcoll.h>
#include <afxinet.h>
#include "MiniJson.h"

// One browsable entry (a library "view", a folder, a season, a movie, an
// episode, etc). Jellyfin distinguishes these via the "Type"/"IsFolder"
// JSON fields; we keep just what the UI needs to display/browse/play.
struct CJellyfinItem
{
	CString id;
	CString name;
	CString type;       // e.g. "CollectionFolder", "Series", "Season", "Episode", "Movie", "Folder"
	bool isFolder;
	CStringA mediaSourceId; // used when requesting PlaybackInfo/stream
	__int64 runtimeTicks;
	int childCount;

	CJellyfinItem() : isFolder(false), runtimeTicks(0), childCount(0) {}
};

struct CJellyfinPlaybackInfo
{
	CStringA mediaSourceId;
	CString container;
	bool supportsDirectPlay;

	CJellyfinPlaybackInfo() : supportsDirectPlay(false) {}
};

class CJellyfinClient
{
public:
	CJellyfinClient();
	virtual ~CJellyfinClient();

	// Server/credentials configuration. ServerUrl should look like
	// "http://myserver:8096" (no trailing slash).
	void SetServer(CString serverUrl);
	void SetDeviceId(CString deviceId) { if(!deviceId.IsEmpty()) m_deviceId = deviceId; }

	// Authenticates against /Users/AuthenticateByName. On success caches
	// the access token + user id for subsequent calls, and returns true.
	bool AuthenticateByName(CString username, CString password, CString& error);

	bool IsAuthenticated() const { return !m_accessToken.IsEmpty(); }

	// Top-level libraries (Movies, TV Shows, etc), from /Users/{id}/Views.
	bool GetLibraries(CAtlArray<CJellyfinItem>& items, CString& error);

	// Children of a folder/series/season, from /Users/{id}/Items?ParentId=...
	bool GetItems(CString parentId, CAtlArray<CJellyfinItem>& items, CString& error);
	bool GetPlaybackInfo(const CJellyfinItem& item, CJellyfinPlaybackInfo& info, CString& error);
	bool GetDirectPlayStreamUrl(const CJellyfinItem& item, const CJellyfinPlaybackInfo& info,
		CString& streamUrl, CString& playSessionId, CStringA& mediaSourceId, CString& error);
	bool GetHlsStreamUrl(const CJellyfinItem& item, CString& streamUrl,
		CString& playSessionId, CStringA& mediaSourceId, CString& error,
		__int64 startTimeTicks = 0);

	// Resolves a playable URL for an item. Per project requirements, this
	// ALWAYS forces the Jellyfin server to transcode the media into a
	// plain progressive HTTP stream (MPEG-2 video/MP2 audio in an MPEG-TS container by
	// default) rather than ever direct-playing the source file or using
	// segmented/adaptive HLS. This keeps the resulting URL a single plain
	// "http://" resource that MPC's existing RenderFile()-based playback
	// pipeline can open with zero changes, which matters a lot given the
	// old codecs available on a Windows 9x/2000 target.
	//
	// On success, fills streamUrl and playSessionId (needed later for
	// ReportPlaybackStart/Progress/Stopped) and mediaSourceId.
	bool GetTranscodedStreamUrl(const CJellyfinItem& item, CString& streamUrl,
		CString& playSessionId, CStringA& mediaSourceId, CString& error,
		__int64 startTimeTicks = 0);

	// Playback session reporting, so the Jellyfin server (a) shows accurate
	// "Now Playing" state and (b) tears down the ffmpeg transcode process
	// promptly on stop instead of waiting for its idle timeout.
	bool ReportPlaybackStart(CString itemId, CString playSessionId, CStringA mediaSourceId,
		__int64 positionTicks = 0);
	bool ReportPlaybackProgress(CString itemId, CString playSessionId, CStringA mediaSourceId,
		__int64 positionTicks, bool isPaused);
	bool ReportPlaybackStopped(CString itemId, CString playSessionId, CStringA mediaSourceId,
		__int64 positionTicks);

	CString GetDeviceId() const { return m_deviceId; }
	CString GetUserId() const { return m_userId; }
	CString GetAccessToken() const { return m_accessToken; }
	void SetAccessToken(CString userId, CString accessToken) { m_userId = userId; m_accessToken = accessToken; }

private:
	CString m_serverUrl;   // e.g. http://host:8096
	CString m_userId;
	CString m_accessToken;
	CString m_deviceId;    // stable per-install id, generated once and persisted by caller

	// Low level HTTP helper: issues a GET or POST request against
	// m_serverUrl + urlPath, with Jellyfin auth headers attached, an
	// optional JSON request body, and returns the raw response body.
	bool DoRequest(LPCTSTR verb, CString urlPath, const CStringA& jsonBody,
		CStringA& responseBody, CString& error, DWORD* pStatusCode = NULL);

	CString BuildAuthHeader() const;
	bool ParseItemList(const CJsonValue& root, CAtlArray<CJellyfinItem>& items);

	static CString NewGuid();
};
