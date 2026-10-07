// Util.h — вспомогательные функции: строки, числа, даты
#pragma once
#include <windows.h>
#include <string>
#include <vector>

std::wstring Utf8ToW(const std::string& s);
std::string WToUtf8(const std::wstring& w);

// Округление денежной суммы до копеек
double Round2(double v);
// "1 234,56" — с разделителем разрядов (для экрана)
std::wstring FormatNumber(double v, int decimals);
// "1234,56" — без разделителя разрядов (для CSV)
std::wstring FormatPlain(double v, int decimals);
// Разбор числа, введённого пользователем: допускаются пробелы, запятая или точка
bool ParseNumber(const std::wstring& text, double& out);

// Даты хранятся в формате ISO "ГГГГ-ММ-ДД"
std::wstring SystemTimeToIso(const SYSTEMTIME& st);
bool IsoToSystemTime(const std::wstring& iso, SYSTEMTIME& st);
std::wstring IsoToRu(const std::wstring& iso);      // "ДД.ММ.ГГГГ"
std::wstring AddDays(const std::wstring& iso, int days);
int DaysBetween(const std::wstring& from, const std::wstring& to);
std::wstring TodayIso();
std::wstring NowStamp();                              // "ГГГГММДД-ЧЧММСС"

std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep);
std::wstring CsvSafe(const std::wstring& s);         // убирает ';' и переводы строк

bool ReadAllUtf8(const std::wstring& path, std::wstring& out);
bool WriteAllUtf8Atomic(const std::wstring& path, const std::wstring& text, bool bom);
