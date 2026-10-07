// Util.cpp — вспомогательные функции: строки, числа, даты
#include "Util.h"
#include <cmath>
#include <cwchar>

std::wstring Utf8ToW(const std::string& s)
{
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

std::string WToUtf8(const std::wstring& w)
{
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

double Round2(double v)
{
    return std::round(v * 100.0) / 100.0;
}

std::wstring FormatPlain(double v, int decimals)
{
    wchar_t buf[64];
    swprintf_s(buf, L"%.*f", decimals, v);
    std::wstring s = buf;
    for (auto& c : s)
        if (c == L'.') c = L',';
    return s;
}

std::wstring FormatNumber(double v, int decimals)
{
    std::wstring s = FormatPlain(v, decimals);
    size_t comma = s.find(L',');
    std::wstring intPart = comma == std::wstring::npos ? s : s.substr(0, comma);
    std::wstring frac = comma == std::wstring::npos ? L"" : s.substr(comma);
    bool neg = !intPart.empty() && intPart[0] == L'-';
    if (neg) intPart.erase(0, 1);

    std::wstring grouped;
    int count = 0;
    for (int i = (int)intPart.size() - 1; i >= 0; --i) {
        grouped.insert(grouped.begin(), intPart[i]);
        if (++count % 3 == 0 && i > 0) grouped.insert(grouped.begin(), L' ');
    }
    return (neg ? L"-" : L"") + grouped + frac;
}

bool ParseNumber(const std::wstring& text, double& out)
{
    std::wstring t;
    for (wchar_t c : text) {
        if (c == L' ' || c == L' ' || c == L'\t') continue;
        t += (c == L',') ? L'.' : c;
    }
    if (t.empty()) return false;
    wchar_t* end = nullptr;
    double v = wcstod(t.c_str(), &end);
    if (end != t.c_str() + t.size() || !std::isfinite(v)) return false;
    out = v;
    return true;
}

std::wstring SystemTimeToIso(const SYSTEMTIME& st)
{
    wchar_t buf[16];
    swprintf_s(buf, L"%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
    return buf;
}

bool IsoToSystemTime(const std::wstring& iso, SYSTEMTIME& st)
{
    int y = 0, m = 0, d = 0;
    if (swscanf_s(iso.c_str(), L"%d-%d-%d", &y, &m, &d) != 3) return false;
    if (y < 1900 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    ZeroMemory(&st, sizeof(st));
    st.wYear = (WORD)y;
    st.wMonth = (WORD)m;
    st.wDay = (WORD)d;
    return true;
}

std::wstring IsoToRu(const std::wstring& iso)
{
    if (iso.size() < 10) return iso;
    return iso.substr(8, 2) + L"." + iso.substr(5, 2) + L"." + iso.substr(0, 4);
}

static bool IsoToTicks(const std::wstring& iso, ULONGLONG& ticks)
{
    SYSTEMTIME st;
    FILETIME ft;
    if (!IsoToSystemTime(iso, st) || !SystemTimeToFileTime(&st, &ft)) return false;
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    ticks = u.QuadPart;
    return true;
}

static const ULONGLONG kTicksPerDay = 864000000000ULL;

std::wstring AddDays(const std::wstring& iso, int days)
{
    ULONGLONG t;
    if (!IsoToTicks(iso, t)) return iso;
    if (days >= 0) t += kTicksPerDay * (ULONGLONG)days;
    else t -= kTicksPerDay * (ULONGLONG)(-days);
    ULARGE_INTEGER u;
    u.QuadPart = t;
    FILETIME ft{ u.LowPart, u.HighPart };
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    return SystemTimeToIso(st);
}

int DaysBetween(const std::wstring& from, const std::wstring& to)
{
    ULONGLONG a, b;
    if (!IsoToTicks(from, a) || !IsoToTicks(to, b)) return 0;
    return (int)(((LONGLONG)b - (LONGLONG)a) / (LONGLONG)kTicksPerDay);
}

std::wstring TodayIso()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    return SystemTimeToIso(st);
}

std::wstring NowStamp()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32];
    swprintf_s(buf, L"%04u%02u%02u-%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep)
{
    std::vector<std::wstring> parts;
    size_t start = 0;
    for (;;) {
        size_t p = s.find(sep, start);
        if (p == std::wstring::npos) {
            parts.push_back(s.substr(start));
            break;
        }
        parts.push_back(s.substr(start, p - start));
        start = p + 1;
    }
    return parts;
}

std::wstring CsvSafe(const std::wstring& s)
{
    std::wstring r = s;
    for (auto& c : r) {
        if (c == L';') c = L',';
        else if (c == L'\r' || c == L'\n' || c == L'\t') c = L' ';
    }
    return r;
}

bool ReadAllUtf8(const std::wstring& path, std::wstring& out)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size) || size.QuadPart > 64LL * 1024 * 1024) {
        CloseHandle(h);
        return false;
    }
    std::string data((size_t)size.QuadPart, '\0');
    DWORD read = 0;
    BOOL ok = data.empty() ? TRUE : ReadFile(h, &data[0], (DWORD)data.size(), &read, nullptr);
    CloseHandle(h);
    if (!ok) return false;
    data.resize(read);
    if (data.size() >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF)
        data.erase(0, 3);
    out = Utf8ToW(data);
    return true;
}

// Запись во временный файл с последующей заменой — исходный файл
// не повреждается при сбое во время записи (REQ-REL-001).
bool WriteAllUtf8Atomic(const std::wstring& path, const std::wstring& text, bool bom)
{
    std::wstring tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    std::string data = WToUtf8(text);
    if (bom) data.insert(0, "\xEF\xBB\xBF");
    DWORD written = 0;
    BOOL ok = data.empty() ? TRUE : WriteFile(h, data.data(), (DWORD)data.size(), &written, nullptr);
    ok = ok && FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok || written != data.size()) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}
