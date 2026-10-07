// Nbrb.h — клиент открытого API Национального банка Республики Беларусь
// Документация: https://www.nbrb.by/apihelp/exrates
#pragma once
#include <string>
#include <vector>

struct RateInfo {
    int curId = 0;               // Cur_ID — внутренний идентификатор валюты НБ РБ
    std::wstring abbr;           // USD, EUR, RUB ...
    int scale = 1;               // Cur_Scale — за сколько единиц валюты установлен курс
    double rate = 0.0;           // Cur_OfficialRate — курс в BYN за scale единиц
    std::wstring date;           // дата, на которую установлен курс (ГГГГ-ММ-ДД)
    bool fromCache = false;
};

struct RatePoint {
    std::wstring date;
    double rate = 0.0;
};

enum class FetchResult { Ok, NotFound, NetError };

// Официальный курс валюты на конкретную дату (без подбора соседних дат)
FetchResult FetchRate(const std::wstring& abbr, const std::wstring& isoDate, RateInfo& out, std::wstring& err);

// Динамика курса за период (не более 365 дней)
FetchResult FetchDynamics(int curId, const std::wstring& from, const std::wstring& to,
                          std::vector<RatePoint>& out, std::wstring& err);
