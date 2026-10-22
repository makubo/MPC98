// MiniJson.h - a minimal, dependency-free JSON parser for use in MPC98.
//
// Written specifically to avoid any features that are problematic on the
// old VS2005 / Windows 9x-2000 toolchain used by this project:
//  - no STL containers (uses ATL CAtlList / CAtlArray instead, which are
//    already used elsewhere in this codebase)
//  - no C++ exceptions required (parse errors are reported via a bool
//    return value)
//  - ANSI (CStringA) based, since the wire format (UTF-8/HTTP JSON) is
//    naturally byte-oriented; callers can convert individual fields to
//    CString/UNICODE as needed with CString(CStringA) conversions.
//
// This is *not* a general purpose, fully spec-compliant JSON library. It
// supports enough of JSON to parse typical Jellyfin REST API responses:
// objects, arrays, strings (with the common escape sequences), numbers,
// true/false/null.
#pragma once

#include <atlcoll.h>

// Converts a UTF-8 byte string (as produced by the JSON parser, see
// MiniJson.cpp) into the project's TCHAR-based CString. Using
// MultiByteToWideChar(CP_UTF8, ...) here (rather than relying on
// CString's default CStringA -> CStringW conversion, which uses the
// current ANSI code page) is essential: Jellyfin names/titles are
// frequently non-ASCII (e.g. Cyrillic), and the default ANSI-codepage
// conversion mangles them into "?" characters.
inline CString Utf8ToTString(const CStringA& utf8)
{
	if(utf8.IsEmpty())
		return CString();

	int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	if(wlen <= 0)
		return CString(utf8); // best-effort fallback

	CAtlArray<WCHAR> wbuf;
	wbuf.SetCount(wlen);
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wbuf.GetData(), wlen);

#ifdef _UNICODE
	return CString(wbuf.GetData());
#else
	// ANSI/MBCS build: re-encode the UTF-16 buffer into the current ANSI
	// code page so it can be stored in a CStringA-based CString. Any
	// characters not representable in the ANSI code page will be
	// substituted by Windows (typically with '?'), which is an
	// unavoidable limitation of non-Unicode builds, not a bug in this
	// conversion.
	int alen = WideCharToMultiByte(CP_ACP, 0, wbuf.GetData(), -1, NULL, 0, NULL, NULL);
	if(alen <= 0)
		return CString(utf8);
	CString result;
	WideCharToMultiByte(CP_ACP, 0, wbuf.GetData(), -1, result.GetBufferSetLength(alen - 1), alen, NULL, NULL);
	result.ReleaseBuffer();
	return result;
#endif
}

class CJsonValue
{
public:
	enum Type { Null, Bool, Number, String, Array, Object };

	Type type;
	bool boolValue;
	double numberValue;
	CStringA stringValue;
	CAtlArray<CJsonValue> arrayValue;

	// Object storage as two parallel arrays instead of a nested
	// name/value struct: a nested struct holding a CJsonValue *by value*
	// would need CJsonValue to be a complete type at the point the
	// nested struct is defined, but CJsonValue is still being defined at
	// that point (self-reference through an incomplete type), which old
	// VC8/VS2005 correctly rejects (C2079/C2440). Template members like
	// CAtlArray<CJsonValue> are fine because template instantiation is
	// deferred until first use, by which point the class is complete.
	CAtlArray<CStringA> objectNames;
	CAtlArray<CJsonValue> objectValues;

	CJsonValue() : type(Null), boolValue(false), numberValue(0) {}

	// CAtlArray intentionally has a private/inaccessible copy constructor
	// (to avoid silent, expensive deep copies) and instead provides an
	// explicit Copy() method. Since CJsonValue embeds CAtlArray members
	// (and is itself stored *by value* in CAtlArray<CJsonValue>, and
	// returned by value from the parser), it needs its own explicit deep
	// copy/assignment implemented in terms of CAtlArray::Copy(), or the
	// compiler-generated copy constructor/assignment would be implicitly
	// deleted/inaccessible (C2248) as soon as any CAtlArray<CJsonValue>
	// tries to copy-construct an element.
	CJsonValue(const CJsonValue& other)
		: type(Null), boolValue(false), numberValue(0)
	{
		*this = other;
	}

	CJsonValue& operator=(const CJsonValue& other)
	{
		if(this == &other)
			return *this;

		type = other.type;
		boolValue = other.boolValue;
		numberValue = other.numberValue;
		stringValue = other.stringValue;

		arrayValue.RemoveAll();
		arrayValue.Copy(other.arrayValue);

		objectNames.RemoveAll();
		objectNames.Copy(other.objectNames);

		objectValues.RemoveAll();
		objectValues.Copy(other.objectValues);

		return *this;
	}

	bool IsNull() const { return type == Null; }
	bool IsObject() const { return type == Object; }
	bool IsArray() const { return type == Array; }
	bool IsString() const { return type == String; }
	bool IsNumber() const { return type == Number; }
	bool IsBool() const { return type == Bool; }

	void AddMember(const CStringA& name, const CJsonValue& value)
	{
		objectNames.Add(name);
		objectValues.Add(value);
	}

	// Object member lookup helpers. Returns a Null CJsonValue if not found.
	const CJsonValue& operator[](LPCSTR name) const
	{
		static CJsonValue nullValue;
		if(type != Object)
			return nullValue;
		for(size_t i = 0; i < objectNames.GetCount(); i++)
		{
			if(objectNames[i].CompareNoCase(name) == 0)
				return objectValues[i];
		}
		return nullValue;
	}

	bool HasMember(LPCSTR name) const
	{
		if(type != Object)
			return false;
		for(size_t i = 0; i < objectNames.GetCount(); i++)
		{
			if(objectNames[i].CompareNoCase(name) == 0)
				return true;
		}
		return false;
	}

	CStringA AsStringA(LPCSTR def = "") const
	{
		if(type == String) return stringValue;
		if(type == Number) { CStringA s; s.Format("%g", numberValue); return s; }
		if(type == Bool) return boolValue ? "true" : "false";
		return def;
	}

	CString AsString(LPCTSTR def = _T("")) const
	{
		if(type != String) return def;
		return Utf8ToTString(stringValue);
	}

	double AsNumber(double def = 0) const
	{
		return type == Number ? numberValue : def;
	}

	__int64 AsInt64(__int64 def = 0) const
	{
		return type == Number ? (__int64)numberValue : def;
	}

	bool AsBool(bool def = false) const
	{
		return type == Bool ? boolValue : def;
	}
};

// Parses a JSON document from a UTF-8/ASCII buffer. Returns true on success
// and fills "out" with the root value. On failure returns false; "out" is
// left in whatever partial state parsing reached.
bool ParseJson(const CStringA& text, CJsonValue& out);
