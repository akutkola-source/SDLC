// Storage.h — локальное хранение данных в %LOCALAPPDATA%\RateTax
#pragma once
#include "Nbrb.h"
#include <map>
#include <string>
#include <vector>

// Запись книги учёта доходов
struct Income {
    std::wstring date;       // дата поступления (ГГГГ-ММ-ДД)
    std::wstring cur;        // код валюты
    double amount = 0.0;     // сумма в валюте
    double rate = 1.0;       // официальный курс НБ РБ
    int scale = 1;           // масштаб курса
    std::wstring rateDate;   // дата, на которую установлен применённый курс
    double byn = 0.0;        // сумма в BYN
    std::wstring doc;        // номер платёжного документа
    std::wstring comment;
};

class Storage {
public:
    bool Init(std::wstring& err);
    const std::wstring& Dir() const { return dir_; }

    std::vector<Income> incomes;
    double taxRate = 6.0;

    bool SaveIncomes();
    void SortIncomes();

    bool GetCachedRate(const std::wstring& abbr, const std::wstring& date, RateInfo& out) const;
    void PutCachedRate(const std::wstring& date, const RateInfo& info);

    bool SaveSettings();

    bool Backup(const std::wstring& folder, std::wstring& file, std::wstring& err);
    bool Restore(const std::wstring& file, std::wstring& err);
    void AutoBackup();

    void Log(const std::wstring& message);

private:
    static bool ParseIncomes(const std::wstring& text, std::vector<Income>& out);
    void LoadCache();
    bool SaveCache();

    std::wstring dir_, incomesPath_, cachePath_, settingsPath_, logPath_, backupDir_;
    std::map<std::wstring, RateInfo> cache_;   // ключ: "USD|ГГГГ-ММ-ДД"
};
