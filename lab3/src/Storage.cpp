// Storage.cpp — файлы данных: incomes.csv (книга доходов), rates.csv (кэш курсов),
// settings.ini (настройки), backups\ (резервные копии), logs\errors.log
#include "Storage.h"
#include "Util.h"
#include <windows.h>
#include <algorithm>
#include <cstdlib>

static const wchar_t* kIncomesHeader = L"date;cur;amount;rate;scale;rateDate;byn;doc;comment";

static std::wstring Num(double v, int decimals)
{
    wchar_t buf[64];
    swprintf_s(buf, L"%.*f", decimals, v);   // всегда с точкой: формат файла не зависит от локали
    return buf;
}

static double ToDouble(const std::wstring& s)
{
    return wcstod(s.c_str(), nullptr);
}

bool Storage::Init(std::wstring& err)
{
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        err = L"Не найдена папка LOCALAPPDATA";
        return false;
    }
    dir_ = std::wstring(buf) + L"\\RateTax";
    backupDir_ = dir_ + L"\\backups";
    std::wstring logDir = dir_ + L"\\logs";
    CreateDirectoryW(dir_.c_str(), nullptr);
    CreateDirectoryW(backupDir_.c_str(), nullptr);
    CreateDirectoryW(logDir.c_str(), nullptr);
    if (GetFileAttributesW(dir_.c_str()) == INVALID_FILE_ATTRIBUTES) {
        err = L"Не удалось создать папку данных " + dir_;
        return false;
    }
    incomesPath_ = dir_ + L"\\incomes.csv";
    cachePath_ = dir_ + L"\\rates.csv";
    settingsPath_ = dir_ + L"\\settings.ini";
    logPath_ = logDir + L"\\errors.log";

    std::wstring text;
    if (ReadAllUtf8(incomesPath_, text) && !ParseIncomes(text, incomes)) {
        err = L"Файл книги доходов повреждён: " + incomesPath_;
        return false;
    }
    SortIncomes();
    LoadCache();

    wchar_t rate[32];
    GetPrivateProfileStringW(L"tax", L"rate", L"6", rate, 32, settingsPath_.c_str());
    double r;
    if (ParseNumber(rate, r) && r >= 0 && r <= 100) taxRate = r;
    return true;
}

bool Storage::ParseIncomes(const std::wstring& text, std::vector<Income>& out)
{
    std::vector<Income> result;
    for (auto line : Split(text, L'\n')) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty() || line.compare(0, 5, L"date;") == 0) continue;
        auto f = Split(line, L';');
        if (f.size() < 9) return false;
        Income in;
        SYSTEMTIME st;
        if (!IsoToSystemTime(f[0], st)) return false;
        in.date = f[0];
        in.cur = f[1];
        in.amount = ToDouble(f[2]);
        in.rate = ToDouble(f[3]);
        in.scale = _wtoi(f[4].c_str());
        if (in.scale < 1) in.scale = 1;
        in.rateDate = f[5];
        in.byn = ToDouble(f[6]);
        in.doc = f[7];
        in.comment = f[8];
        result.push_back(in);
    }
    out = result;
    return true;
}

void Storage::SortIncomes()
{
    std::stable_sort(incomes.begin(), incomes.end(),
                     [](const Income& a, const Income& b) { return a.date < b.date; });
}

bool Storage::SaveIncomes()
{
    std::wstring text = std::wstring(kIncomesHeader) + L"\n";
    for (const auto& in : incomes) {
        text += in.date + L";" + in.cur + L";" + Num(in.amount, 2) + L";" + Num(in.rate, 4) + L";" +
                std::to_wstring(in.scale) + L";" + in.rateDate + L";" + Num(in.byn, 2) + L";" +
                CsvSafe(in.doc) + L";" + CsvSafe(in.comment) + L"\n";
    }
    if (!WriteAllUtf8Atomic(incomesPath_, text, false)) {
        Log(L"Ошибка записи " + incomesPath_);
        return false;
    }
    return true;
}

void Storage::LoadCache()
{
    std::wstring text;
    if (!ReadAllUtf8(cachePath_, text)) return;
    for (auto line : Split(text, L'\n')) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        auto f = Split(line, L';');
        if (f.size() < 6) continue;
        RateInfo r;
        r.abbr = f[0];
        r.rate = ToDouble(f[2]);
        r.scale = _wtoi(f[3].c_str());
        r.date = f[4];
        r.curId = _wtoi(f[5].c_str());
        if (r.rate > 0 && r.scale > 0) cache_[f[0] + L"|" + f[1]] = r;
    }
}

bool Storage::SaveCache()
{
    std::wstring text;
    for (const auto& kv : cache_) {
        auto key = Split(kv.first, L'|');
        const RateInfo& r = kv.second;
        text += key[0] + L";" + key[1] + L";" + Num(r.rate, 4) + L";" + std::to_wstring(r.scale) + L";" +
                r.date + L";" + std::to_wstring(r.curId) + L"\n";
    }
    return WriteAllUtf8Atomic(cachePath_, text, false);
}

bool Storage::GetCachedRate(const std::wstring& abbr, const std::wstring& date, RateInfo& out) const
{
    auto it = cache_.find(abbr + L"|" + date);
    if (it == cache_.end()) return false;
    out = it->second;
    out.fromCache = true;
    return true;
}

void Storage::PutCachedRate(const std::wstring& date, const RateInfo& info)
{
    RateInfo r = info;
    r.fromCache = false;
    cache_[info.abbr + L"|" + date] = r;
    SaveCache();
}

bool Storage::SaveSettings()
{
    return WritePrivateProfileStringW(L"tax", L"rate", Num(taxRate, 1).c_str(), settingsPath_.c_str()) != FALSE;
}

bool Storage::Backup(const std::wstring& folder, std::wstring& file, std::wstring& err)
{
    if (!SaveIncomes()) {
        err = L"Не удалось сохранить данные перед копированием";
        return false;
    }
    file = folder + L"\\RateTax-backup-" + NowStamp() + L".csv";
    if (!CopyFileW(incomesPath_.c_str(), file.c_str(), TRUE)) {
        err = L"Не удалось создать файл " + file + L" (код " + std::to_wstring(GetLastError()) + L")";
        return false;
    }
    return true;
}

bool Storage::Restore(const std::wstring& file, std::wstring& err)
{
    std::wstring text;
    std::vector<Income> restored;
    if (!ReadAllUtf8(file, text)) {
        err = L"Не удалось прочитать файл " + file;
        return false;
    }
    if (!ParseIncomes(text, restored)) {
        err = L"Файл не является резервной копией КурсНалог";
        return false;
    }
    incomes = restored;
    SortIncomes();
    if (!SaveIncomes()) {
        err = L"Не удалось сохранить восстановленные данные";
        return false;
    }
    return true;
}

// Автоматическая копия раз в 7 дней, хранятся 5 последних (REQ-REL-002)
void Storage::AutoBackup()
{
    if (incomes.empty()) return;
    wchar_t last[32];
    GetPrivateProfileStringW(L"backup", L"last", L"", last, 32, settingsPath_.c_str());
    std::wstring lastIso = last;
    if (!lastIso.empty() && DaysBetween(lastIso, TodayIso()) < 7) return;

    std::wstring file, err;
    if (!Backup(backupDir_, file, err)) {
        Log(L"Автоматическое резервное копирование: " + err);
        return;
    }
    WritePrivateProfileStringW(L"backup", L"last", TodayIso().c_str(), settingsPath_.c_str());

    std::vector<std::wstring> names;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((backupDir_ + L"\\RateTax-backup-*.csv").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do names.push_back(fd.cFileName);
        while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(names.begin(), names.end());
    while (names.size() > 5) {
        DeleteFileW((backupDir_ + L"\\" + names.front()).c_str());
        names.erase(names.begin());
    }
}

// Журнал ошибок: дата, адрес запроса, код ответа. Суммы пользователя не записываются (REQ-OBS-001).
void Storage::Log(const std::wstring& message)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t stamp[32];
    swprintf_s(stamp, L"%04u-%02u-%02u %02u:%02u:%02u ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::string line = WToUtf8(stamp + message + L"\r\n");
    HANDLE h = CreateFileW(logPath_.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, line.data(), (DWORD)line.size(), &written, nullptr);
    CloseHandle(h);
}
