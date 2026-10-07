// Nbrb.cpp — HTTPS-запросы к api.nbrb.by средствами WinINet
#include "Nbrb.h"
#include "Util.h"
#include <windows.h>
#include <wininet.h>
#include <cstdlib>

#pragma comment(lib, "wininet.lib")

static const wchar_t* kBaseUrl = L"https://api.nbrb.by/exrates";

// GET-запрос. Сертификат сервера проверяется системой (флаги игнорирования не задаются).
static bool HttpGet(const std::wstring& url, std::string& body, DWORD& status, std::wstring& err)
{
    HINTERNET hNet = InternetOpenW(L"RateTax/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hNet) {
        err = L"InternetOpen: ошибка " + std::to_wstring(GetLastError());
        return false;
    }
    DWORD timeout = 10000; // 10 секунд
    InternetSetOptionW(hNet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionW(hNet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionW(hNet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

    HINTERNET hUrl = InternetOpenUrlW(hNet, url.c_str(), L"Accept: application/json\r\n", (DWORD)-1L,
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
    if (!hUrl) {
        err = L"Нет соединения с api.nbrb.by (код " + std::to_wstring(GetLastError()) + L")";
        InternetCloseHandle(hNet);
        return false;
    }

    status = 0;
    DWORD len = sizeof(status);
    HttpQueryInfoW(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &len, nullptr);

    body.clear();
    char buf[4096];
    DWORD read = 0;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &read) && read > 0)
        body.append(buf, read);

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hNet);
    return true;
}

// --- Минимальный разбор JSON-ответов НБ РБ (плоские объекты) ---

static bool JsonNumber(const std::string& obj, const char* key, double& out)
{
    std::string k = std::string("\"") + key + "\":";
    size_t p = obj.find(k);
    if (p == std::string::npos) return false;
    p += k.size();
    while (p < obj.size() && obj[p] == ' ') ++p;
    if (obj.compare(p, 4, "null") == 0) return false;
    const char* start = obj.c_str() + p;
    char* end = nullptr;
    out = strtod(start, &end);
    return end != start;
}

static bool JsonString(const std::string& obj, const char* key, std::string& out)
{
    std::string k = std::string("\"") + key + "\":\"";
    size_t p = obj.find(k);
    if (p == std::string::npos) return false;
    p += k.size();
    size_t e = obj.find('"', p);
    if (e == std::string::npos) return false;
    out = obj.substr(p, e - p);
    return true;
}

static bool ParseRateObject(const std::string& obj, RateInfo& out)
{
    double rate = 0, scale = 1, id = 0;
    std::string date;
    if (!JsonNumber(obj, "Cur_OfficialRate", rate) || rate <= 0) return false;
    JsonNumber(obj, "Cur_Scale", scale);
    JsonNumber(obj, "Cur_ID", id);
    if (JsonString(obj, "Date", date) && date.size() >= 10) out.date = Utf8ToW(date.substr(0, 10));
    out.rate = rate;
    out.scale = scale >= 1 ? (int)scale : 1;
    out.curId = (int)id;
    return true;
}

FetchResult FetchRate(const std::wstring& abbr, const std::wstring& isoDate, RateInfo& out, std::wstring& err)
{
    std::wstring url = std::wstring(kBaseUrl) + L"/rates/" + abbr + L"?parammode=2&ondate=" + isoDate;
    std::string body;
    DWORD status = 0;
    if (!HttpGet(url, body, status, err)) return FetchResult::NetError;
    if (status == 404 || status == 204) return FetchResult::NotFound;
    if (status != 200) {
        err = L"Сервер НБ РБ вернул код " + std::to_wstring(status);
        return FetchResult::NetError;
    }
    RateInfo info;
    info.abbr = abbr;
    if (!ParseRateObject(body, info)) return FetchResult::NotFound;
    if (info.date.empty()) info.date = isoDate;
    out = info;
    return FetchResult::Ok;
}

FetchResult FetchDynamics(int curId, const std::wstring& from, const std::wstring& to,
                          std::vector<RatePoint>& out, std::wstring& err)
{
    std::wstring url = std::wstring(kBaseUrl) + L"/rates/dynamics/" + std::to_wstring(curId) +
                       L"?startdate=" + from + L"&enddate=" + to;
    std::string body;
    DWORD status = 0;
    if (!HttpGet(url, body, status, err)) return FetchResult::NetError;
    if (status == 404) return FetchResult::NotFound;
    if (status != 200) {
        err = L"Сервер НБ РБ вернул код " + std::to_wstring(status);
        return FetchResult::NetError;
    }
    out.clear();
    size_t pos = 0;
    for (;;) {
        size_t b = body.find('{', pos);
        if (b == std::string::npos) break;
        size_t e = body.find('}', b);
        if (e == std::string::npos) break;
        std::string obj = body.substr(b, e - b + 1);
        double rate = 0;
        std::string date;
        if (JsonNumber(obj, "Cur_OfficialRate", rate) && JsonString(obj, "Date", date) && date.size() >= 10)
            out.push_back({ Utf8ToW(date.substr(0, 10)), rate });
        pos = e + 1;
    }
    return out.empty() ? FetchResult::NotFound : FetchResult::Ok;
}
