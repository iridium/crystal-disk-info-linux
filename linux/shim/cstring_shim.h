// MFC CString / CArray look-alikes backed by the standard library.
#pragma once

#include <string>
#include <vector>
#include <cwchar>
#include <cwctype>
#include <cstdarg>
#include <time.h>
#include <cstring>
#include <algorithm>

class CString
{
public:
	CString() = default;
	CString(const wchar_t* s) : m_s(s ? s : L"") {}
	CString(const wchar_t* s, int n) : m_s(s ? std::wstring(s, wcsnlen(s, (size_t)n)) : L"") {}
	CString(const char* s) { assignNarrow(s, s ? strlen(s) : 0); }
	CString(const std::wstring& s) : m_s(s) {}
	CString(wchar_t c, int n = 1) : m_s((size_t)n, c) {}

	// Fixed-size CHAR arrays from IDENTIFY data are not NUL-terminated.
	template <size_t N>
	CString(const char (&a)[N]) { assignNarrow(a, strnlen(a, N)); }
	template <size_t N>
	CString& operator=(const char (&a)[N]) { assignNarrow(a, strnlen(a, N)); return *this; }

	template <size_t N>
	CString(const unsigned char (&a)[N]) { assignNarrow((const char*)a, strnlen((const char*)a, N)); }
	template <size_t N>
	CString& operator=(const unsigned char (&a)[N]) { assignNarrow((const char*)a, strnlen((const char*)a, N)); return *this; }

	CString& operator=(const wchar_t* s) { m_s = s ? s : L""; return *this; }
	CString& operator=(const char* s) { assignNarrow(s, s ? strlen(s) : 0); return *this; }
	CString& operator=(wchar_t c) { m_s.assign(1, c); return *this; }

	operator const wchar_t*() const { return m_s.c_str(); }
	const wchar_t* GetString() const { return m_s.c_str(); }
	const std::wstring& str() const { return m_s; }

	int GetLength() const { return (int)m_s.size(); }
	bool IsEmpty() const { return m_s.empty(); }
	void Empty() { m_s.clear(); }
	wchar_t GetAt(int i) const { return (i >= 0 && i < GetLength()) ? m_s[(size_t)i] : 0; }
	void SetAt(int i, wchar_t c) { if (i >= 0 && i < GetLength()) m_s[(size_t)i] = c; }
	wchar_t operator[](int i) const { return GetAt(i); }

	CString& operator+=(const CString& o) { m_s += o.m_s; return *this; }
	CString& operator+=(const wchar_t* o) { if (o) m_s += o; return *this; }
	CString& operator+=(wchar_t c) { m_s += c; return *this; }
	void Append(const wchar_t* o) { if (o) m_s += o; }
	void AppendChar(wchar_t c) { m_s += c; }

	friend CString operator+(const CString& a, const CString& b) { return CString(a.m_s + b.m_s); }
	friend CString operator+(const CString& a, const wchar_t* b) { return CString(a.m_s + (b ? b : L"")); }
	friend CString operator+(const wchar_t* a, const CString& b) { return CString(std::wstring(a ? a : L"") + b.m_s); }
	friend CString operator+(const CString& a, wchar_t c) { return CString(a.m_s + c); }

	friend bool operator==(const CString& a, const CString& b) { return a.m_s == b.m_s; }
	friend bool operator==(const CString& a, const wchar_t* b) { return a.m_s == (b ? b : L""); }
	friend bool operator!=(const CString& a, const CString& b) { return a.m_s != b.m_s; }
	friend bool operator!=(const CString& a, const wchar_t* b) { return a.m_s != (b ? b : L""); }
	friend bool operator<(const CString& a, const CString& b) { return a.m_s < b.m_s; }

	int Compare(const wchar_t* o) const { return wcscmp(m_s.c_str(), o ? o : L""); }
	int CompareNoCase(const wchar_t* o) const { return wcscasecmp(m_s.c_str(), o ? o : L""); }

	int Find(const wchar_t* sub, int start = 0) const
	{
		if (start < 0 || start > GetLength()) return -1;
		size_t p = m_s.find(sub ? sub : L"", (size_t)start);
		return p == std::wstring::npos ? -1 : (int)p;
	}
	int Find(wchar_t c, int start = 0) const
	{
		if (start < 0 || start > GetLength()) return -1;
		size_t p = m_s.find(c, (size_t)start);
		return p == std::wstring::npos ? -1 : (int)p;
	}
	int ReverseFind(wchar_t c) const
	{
		size_t p = m_s.rfind(c);
		return p == std::wstring::npos ? -1 : (int)p;
	}
	int FindOneOf(const wchar_t* set) const
	{
		size_t p = m_s.find_first_of(set ? set : L"");
		return p == std::wstring::npos ? -1 : (int)p;
	}

	CString Mid(int first) const { return Mid(first, GetLength()); }
	CString Mid(int first, int count) const
	{
		if (first < 0) first = 0;
		if (first >= GetLength() || count <= 0) return CString();
		return CString(m_s.substr((size_t)first, (size_t)count));
	}
	CString Left(int count) const { return Mid(0, count); }
	CString Right(int count) const
	{
		if (count <= 0) return CString();
		if (count >= GetLength()) return *this;
		return CString(m_s.substr(m_s.size() - (size_t)count));
	}

	int Replace(const wchar_t* from, const wchar_t* to)
	{
		std::wstring f = from ? from : L"", t = to ? to : L"";
		if (f.empty()) return 0;
		int n = 0;
		size_t p = 0;
		while ((p = m_s.find(f, p)) != std::wstring::npos) { m_s.replace(p, f.size(), t); p += t.size(); ++n; }
		return n;
	}
	int Replace(wchar_t from, wchar_t to)
	{
		int n = 0;
		for (auto& c : m_s) if (c == from) { c = to; ++n; }
		return n;
	}
	int Remove(wchar_t c)
	{
		size_t before = m_s.size();
		m_s.erase(std::remove(m_s.begin(), m_s.end(), c), m_s.end());
		return (int)(before - m_s.size());
	}
	int Insert(int i, const wchar_t* s) { if (i < 0) i = 0; if (i > GetLength()) i = GetLength(); m_s.insert((size_t)i, s ? s : L""); return GetLength(); }
	int Delete(int i, int n = 1) { if (i >= 0 && i < GetLength() && n > 0) m_s.erase((size_t)i, (size_t)n); return GetLength(); }

	CString& TrimRight(const wchar_t* set = L" \t\r\n")
	{
		size_t e = m_s.find_last_not_of(set);
		m_s.erase(e == std::wstring::npos ? 0 : e + 1);
		return *this;
	}
	CString& TrimRight(wchar_t c) { wchar_t s[2] = { c, 0 }; return TrimRight(s); }
	CString& TrimLeft(const wchar_t* set = L" \t\r\n")
	{
		size_t b = m_s.find_first_not_of(set);
		m_s.erase(0, b == std::wstring::npos ? m_s.size() : b);
		return *this;
	}
	CString& TrimLeft(wchar_t c) { wchar_t s[2] = { c, 0 }; return TrimLeft(s); }
	CString& Trim(const wchar_t* set = L" \t\r\n") { TrimRight(set); return TrimLeft(set); }
	CString& Trim(wchar_t c) { TrimRight(c); return TrimLeft(c); }

	CString& MakeUpper() { for (auto& c : m_s) c = (wchar_t)towupper((wint_t)c); return *this; }
	CString& MakeLower() { for (auto& c : m_s) c = (wchar_t)towlower((wint_t)c); return *this; }

	CString Tokenize(const wchar_t* delims, int& pos) const
	{
		if (pos < 0 || pos >= GetLength()) { pos = -1; return CString(); }
		size_t b = m_s.find_first_not_of(delims, (size_t)pos);
		if (b == std::wstring::npos) { pos = -1; return CString(); }
		size_t e = m_s.find_first_of(delims, b);
		if (e == std::wstring::npos) e = m_s.size();
		pos = (int)e + 1;
		return CString(m_s.substr(b, e - b));
	}

	wchar_t* GetBuffer(int minLen = 0)
	{
		if ((int)m_buf.size() < std::max(minLen, GetLength()) + 1) m_buf.resize((size_t)std::max(minLen, GetLength()) + 1);
		std::fill(m_buf.begin(), m_buf.end(), L'\0');
		std::copy(m_s.begin(), m_s.end(), m_buf.begin());
		return m_buf.data();
	}
	void ReleaseBuffer(int len = -1)
	{
		if (m_buf.empty()) return;
		m_s = len < 0 ? std::wstring(m_buf.data()) : std::wstring(m_buf.data(), (size_t)len);
	}

	void Format(const wchar_t* fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		FormatV(fmt, ap);
		va_end(ap);
	}
	void AppendFormat(const wchar_t* fmt, ...)
	{
		CString tmp;
		va_list ap;
		va_start(ap, fmt);
		tmp.FormatV(fmt, ap);
		va_end(ap);
		m_s += tmp.m_s;
	}
	void FormatV(const wchar_t* fmt, va_list ap)
	{
		std::wstring f = translateFormat(fmt ? fmt : L"");
		std::vector<wchar_t> buf(256);
		for (;;)
		{
			va_list cp;
			va_copy(cp, ap);
			int n = vswprintf(buf.data(), buf.size(), f.c_str(), cp);
			va_end(cp);
			if (n >= 0 && (size_t)n < buf.size()) { m_s.assign(buf.data(), (size_t)n); return; }
			if (buf.size() > (1u << 20)) { m_s.clear(); return; }
			buf.resize(buf.size() * 2);
		}
	}

	std::string toUtf8() const;

private:
	void assignNarrow(const char* s, size_t n)
	{
		m_s.clear();
		m_s.reserve(n);
		for (size_t i = 0; i < n; ++i) m_s += (wchar_t)(unsigned char)s[i];
	}

	// MSVC: %s in a wide format means a wide string, %I64 means 64-bit.
	static std::wstring translateFormat(const std::wstring& in)
	{
		std::wstring out;
		out.reserve(in.size() + 8);
		for (size_t i = 0; i < in.size(); ++i)
		{
			out += in[i];
			if (in[i] != L'%') continue;
			if (i + 1 < in.size() && in[i + 1] == L'%') { out += L'%'; ++i; continue; }
			size_t j = i + 1;
			while (j < in.size() && wcschr(L"-+ #0123456789.*", in[j])) out += in[j++];
			if (j + 2 < in.size() && in.compare(j, 3, L"I64") == 0) { out += L"ll"; j += 3; }
			if (j < in.size())
			{
				wchar_t c = in[j];
				if (c == L's') out += L"ls";
				else if (c == L'S') out += L's';
				else if (c == L'c') out += L"lc";
				else out += c;
			}
			i = j;
		}
		return out;
	}

	std::wstring m_s;
	std::vector<wchar_t> m_buf;
};

template <class T, class ARG = const T&>
class CArray
{
public:
	INT_PTR GetCount() const { return (INT_PTR)m_v.size(); }
	INT_PTR GetSize() const { return (INT_PTR)m_v.size(); }
	INT_PTR GetUpperBound() const { return (INT_PTR)m_v.size() - 1; }
	bool IsEmpty() const { return m_v.empty(); }
	INT_PTR Add(ARG v) { m_v.push_back(v); return (INT_PTR)m_v.size() - 1; }
	T& operator[](INT_PTR i) { return m_v[(size_t)i]; }
	const T& operator[](INT_PTR i) const { return m_v[(size_t)i]; }
	T& ElementAt(INT_PTR i) { return m_v[(size_t)i]; }
	const T& GetAt(INT_PTR i) const { return m_v[(size_t)i]; }
	void SetAt(INT_PTR i, ARG v) { m_v[(size_t)i] = v; }
	void RemoveAll() { m_v.clear(); }
	void RemoveAt(INT_PTR i, INT_PTR n = 1) { m_v.erase(m_v.begin() + i, m_v.begin() + i + n); }
	void InsertAt(INT_PTR i, ARG v) { m_v.insert(m_v.begin() + i, v); }
	void SetSize(INT_PTR n) { m_v.resize((size_t)n); }
	T* GetData() { return m_v.data(); }
	auto begin() { return m_v.begin(); }
	auto end() { return m_v.end(); }
	auto begin() const { return m_v.begin(); }
	auto end() const { return m_v.end(); }
private:
	std::vector<T> m_v;
};

class CStringArray : public CArray<CString, const CString&> {};
