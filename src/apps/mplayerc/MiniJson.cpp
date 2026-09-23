// MiniJson.cpp - see MiniJson.h for design notes.
#include "stdafx.h"
#include "MiniJson.h"

namespace
{
	// Encodes a Unicode code point as UTF-8 bytes appended to "out". We
	// keep all string values as UTF-8 in CStringA (matching the wire
	// format), and only convert to UTF-16 (CString) on demand in
	// CJsonValue::AsString(), via MultiByteToWideChar(CP_UTF8, ...).
	void AppendUtf8(CStringA& out, unsigned int code)
	{
		if(code <= 0x7F)
		{
			out += (char)code;
		}
		else if(code <= 0x7FF)
		{
			out += (char)(0xC0 | (code >> 6));
			out += (char)(0x80 | (code & 0x3F));
		}
		else if(code <= 0xFFFF)
		{
			out += (char)(0xE0 | (code >> 12));
			out += (char)(0x80 | ((code >> 6) & 0x3F));
			out += (char)(0x80 | (code & 0x3F));
		}
		else
		{
			out += (char)(0xF0 | (code >> 18));
			out += (char)(0x80 | ((code >> 12) & 0x3F));
			out += (char)(0x80 | ((code >> 6) & 0x3F));
			out += (char)(0x80 | (code & 0x3F));
		}
	}

	class JsonParser
	{
	public:
		JsonParser(const CStringA& text)
			: m_text(text), m_pos(0), m_len(text.GetLength()), m_ok(true)
		{
		}

		bool Parse(CJsonValue& out)
		{
			SkipWhitespace();
			if(!ParseValue(out))
				return false;
			SkipWhitespace();
			return m_ok;
		}

	private:
		const CStringA& m_text;
		int m_pos;
		int m_len;
		bool m_ok;

		char Peek()
		{
			return (m_pos < m_len) ? m_text[m_pos] : '\0';
		}

		char Next()
		{
			return (m_pos < m_len) ? m_text[m_pos++] : '\0';
		}

		void SkipWhitespace()
		{
			while(m_pos < m_len)
			{
				char c = m_text[m_pos];
				if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
					m_pos++;
				else
					break;
			}
		}

		bool Expect(char c)
		{
			if(Peek() != c) { m_ok = false; return false; }
			m_pos++;
			return true;
		}

		// Reads exactly 4 hex digits starting at m_pos and advances past
		// them. Returns -1 (and sets m_ok = false) on malformed input.
		int ParseHex4()
		{
			if(m_pos + 4 > m_len) { m_ok = false; return -1; }
			int code = 0;
			for(int i = 0; i < 4; i++)
			{
				char h = m_text[m_pos++];
				int v = 0;
				if(h >= '0' && h <= '9') v = h - '0';
				else if(h >= 'a' && h <= 'f') v = h - 'a' + 10;
				else if(h >= 'A' && h <= 'F') v = h - 'A' + 10;
				else { m_ok = false; return -1; }
				code = (code << 4) | v;
			}
			return code;
		}

		bool ParseValue(CJsonValue& out)
		{
			if(!m_ok) return false;
			SkipWhitespace();
			char c = Peek();
			if(c == '{') return ParseObject(out);
			if(c == '[') return ParseArray(out);
			if(c == '"') return ParseString(out);
			if(c == 't' || c == 'f') return ParseBool(out);
			if(c == 'n') return ParseNull(out);
			if(c == '-' || (c >= '0' && c <= '9')) return ParseNumber(out);
			m_ok = false;
			return false;
		}

		bool ParseObject(CJsonValue& out)
		{
			out.type = CJsonValue::Object;
			if(!Expect('{')) return false;
			SkipWhitespace();
			if(Peek() == '}') { m_pos++; return true; }

			for(;;)
			{
				SkipWhitespace();
				CJsonValue keyVal;
				if(Peek() != '"') { m_ok = false; return false; }
				if(!ParseString(keyVal)) return false;

				SkipWhitespace();
				if(!Expect(':')) return false;

				CStringA name = keyVal.stringValue;
				CJsonValue value;
				if(!ParseValue(value)) return false;
				out.AddMember(name, value);

				SkipWhitespace();
				char c = Next();
				if(c == ',') continue;
				if(c == '}') break;
				m_ok = false;
				return false;
			}
			return true;
		}

		bool ParseArray(CJsonValue& out)
		{
			out.type = CJsonValue::Array;
			if(!Expect('[')) return false;
			SkipWhitespace();
			if(Peek() == ']') { m_pos++; return true; }

			for(;;)
			{
				CJsonValue elem;
				if(!ParseValue(elem)) return false;
				out.arrayValue.Add(elem);

				SkipWhitespace();
				char c = Next();
				if(c == ',') continue;
				if(c == ']') break;
				m_ok = false;
				return false;
			}
			return true;
		}

		bool ParseString(CJsonValue& out)
		{
			out.type = CJsonValue::String;
			out.stringValue.Empty();
			if(!Expect('"')) return false;

			for(;;)
			{
				if(m_pos >= m_len) { m_ok = false; return false; }
				char c = m_text[m_pos++];
				if(c == '"')
					break;
				if(c == '\\')
				{
					if(m_pos >= m_len) { m_ok = false; return false; }
					char esc = m_text[m_pos++];
					switch(esc)
					{
					case '"': out.stringValue += '"'; break;
					case '\\': out.stringValue += '\\'; break;
					case '/': out.stringValue += '/'; break;
					case 'b': out.stringValue += '\b'; break;
					case 'f': out.stringValue += '\f'; break;
					case 'n': out.stringValue += '\n'; break;
					case 'r': out.stringValue += '\r'; break;
					case 't': out.stringValue += '\t'; break;
					case 'u':
						{
							// Full \uXXXX support, including surrogate
							// pairs for characters outside the BMP (e.g.
							// some emoji). The decoded code point is
							// re-encoded as UTF-8 bytes into
							// out.stringValue, matching how un-escaped
							// non-ASCII bytes already arrive from the
							// server (JSON permits both forms). Values
							// are only converted to UTF-16 (CString) on
							// demand in CJsonValue::AsString(), via
							// MultiByteToWideChar(CP_UTF8, ...), so this
							// keeps storage consistent throughout.
							int code = ParseHex4();
							if(code < 0) return false;

							if(code >= 0xD800 && code <= 0xDBFF)
							{
								int savedPos = m_pos;
								if(m_pos + 2 <= m_len && m_text[m_pos] == '\\' && m_text[m_pos + 1] == 'u')
								{
									m_pos += 2;
									int low = ParseHex4();
									if(low >= 0xDC00 && low <= 0xDFFF)
									{
										code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
									}
									else
									{
										// Not a valid low surrogate: rewind
										// and just emit the lone high
										// surrogate value as-is (best
										// effort, shouldn't happen with
										// well-formed input).
										m_pos = savedPos;
									}
								}
							}

							AppendUtf8(out.stringValue, (unsigned int)code);
						}
						break;
					default:
						m_ok = false;
						return false;
					}
				}
				else
				{
					out.stringValue += c;
				}
			}
			return true;
		}

		bool ParseBool(CJsonValue& out)
		{
			out.type = CJsonValue::Bool;
			if(m_len - m_pos >= 4 && strncmp(m_text.GetString() + m_pos, "true", 4) == 0)
			{
				out.boolValue = true;
				m_pos += 4;
				return true;
			}
			if(m_len - m_pos >= 5 && strncmp(m_text.GetString() + m_pos, "false", 5) == 0)
			{
				out.boolValue = false;
				m_pos += 5;
				return true;
			}
			m_ok = false;
			return false;
		}

		bool ParseNull(CJsonValue& out)
		{
			out.type = CJsonValue::Null;
			if(m_len - m_pos >= 4 && strncmp(m_text.GetString() + m_pos, "null", 4) == 0)
			{
				m_pos += 4;
				return true;
			}
			m_ok = false;
			return false;
		}

		bool ParseNumber(CJsonValue& out)
		{
			out.type = CJsonValue::Number;
			int start = m_pos;
			if(Peek() == '-') m_pos++;
			while(m_pos < m_len && isdigit((unsigned char)m_text[m_pos])) m_pos++;
			if(Peek() == '.')
			{
				m_pos++;
				while(m_pos < m_len && isdigit((unsigned char)m_text[m_pos])) m_pos++;
			}
			if(Peek() == 'e' || Peek() == 'E')
			{
				m_pos++;
				if(Peek() == '+' || Peek() == '-') m_pos++;
				while(m_pos < m_len && isdigit((unsigned char)m_text[m_pos])) m_pos++;
			}
			if(m_pos == start) { m_ok = false; return false; }
			CStringA num = m_text.Mid(start, m_pos - start);
			out.numberValue = atof(num);
			return true;
		}
	};
}

bool ParseJson(const CStringA& text, CJsonValue& out)
{
	JsonParser parser(text);
	return parser.Parse(out);
}
