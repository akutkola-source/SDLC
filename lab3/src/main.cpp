// main.cpp — «КурсНалог» (RateTax): архивные курсы НБ РБ для расчёта налога ИП.
// Графический интерфейс — классический WinAPI: класс окна, CreateWindowEx,
// оконная процедура, цикл сообщений, стандартные элементы управления, GDI.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "Nbrb.h"
#include "Storage.h"
#include "Util.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ---------- Идентификаторы элементов управления ----------
enum : int {
    IDC_TAB = 100, IDC_STATUS,
    // вкладка «Курс на дату»
    IDC_RATE_CUR = 200, IDC_RATE_DATE, IDC_RATE_GET, IDC_RATE_RESULT, IDC_RATE_INFO,
    IDC_DYN_FROM, IDC_DYN_TO, IDC_DYN_BUILD, IDC_CHART,
    IDC_CONV_AMOUNT, IDC_CONV_BTN, IDC_CONV_RESULT,
    // вкладка «Книга доходов»
    IDC_INC_DATE = 300, IDC_INC_CUR, IDC_INC_AMOUNT, IDC_INC_DOC, IDC_INC_COMMENT, IDC_INC_ADD, IDC_INC_CANCEL,
    IDC_INC_PERIOD, IDC_INC_EDIT, IDC_INC_DELETE, IDC_INC_EXPORT, IDC_INC_LIST, IDC_INC_TOTAL,
    // вкладка «Расчёт налога»
    IDC_TAX_PERIOD = 400, IDC_TAX_RATE, IDC_TAX_CALC, IDC_TAX_EXPORT, IDC_TAX_PRINT, IDC_TAX_LIST,
    // вкладка «Настройки»
    IDC_SET_RATE = 500, IDC_SET_SAVE, IDC_SET_BACKUP, IDC_SET_RESTORE,
};

struct Currency { const wchar_t* abbr; const wchar_t* name; };
static const Currency kCurrencies[] = {
    { L"USD", L"Доллар США" },
    { L"EUR", L"Евро" },
    { L"RUB", L"Российский рубль" },
    { L"CNY", L"Китайский юань" },
    { L"PLN", L"Польский злотый" },
    { L"BYN", L"Белорусский рубль" },   // только для книги доходов
};
static const int kForeignCount = 5;

struct Period { std::wstring name, from, to; };

struct TaxSummary {
    double foreign = 0, byn = 0, base = 0, rate = 0, tax = 0;
    int count = 0;
    std::map<std::wstring, double> byCurrency;
};

// ---------- Глобальное состояние ----------
static HINSTANCE g_hInst;
static HWND g_hMain, g_hTab, g_hStatus;
static HWND g_pages[4];
static HFONT g_font, g_fontBold, g_fontBig;
static int g_dpi = 96;
static Storage g_store;
static RateInfo g_lastRate;
static bool g_haveRate = false;
static std::vector<RatePoint> g_chart;
static std::wstring g_chartTitle;
static int g_editIndex = -1;
static std::vector<Period> g_periods;

static int S(int v) { return MulDiv(v, g_dpi, 96); }
static HWND Item(int id) { for (HWND p : g_pages) { HWND h = GetDlgItem(p, id); if (h) return h; } return nullptr; }

static std::wstring GetText(int id)
{
    HWND h = Item(id);
    int len = GetWindowTextLengthW(h);
    std::wstring s(len + 1, L'\0');
    GetWindowTextW(h, &s[0], len + 1);
    s.resize(len);
    return s;
}
static void SetText(int id, const std::wstring& s) { SetWindowTextW(Item(id), s.c_str()); }
static void Status(const std::wstring& s) { SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)s.c_str()); }
static void Error(const std::wstring& s) { MessageBoxW(g_hMain, s.c_str(), L"КурсНалог", MB_ICONWARNING | MB_OK); }

static HWND Ctl(HWND parent, LPCWSTR cls, LPCWSTR text, DWORD style, int x, int y, int w, int h, int id, HFONT font = nullptr)
{
    HWND hwnd = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h),
                                parent, (HMENU)(INT_PTR)id, g_hInst, nullptr);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM)(font ? font : g_font), TRUE);
    return hwnd;
}
static HWND Label(HWND p, LPCWSTR t, int x, int y, int w, int h = 20, int id = -1) { return Ctl(p, L"STATIC", t, 0, x, y, w, h, id); }
static HWND Button(HWND p, LPCWSTR t, int x, int y, int w, int id) { return Ctl(p, L"BUTTON", t, WS_TABSTOP | BS_PUSHBUTTON, x, y, w, 28, id); }
static HWND Edit(HWND p, int x, int y, int w, int id) { return Ctl(p, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, x, y, w, 24, id); }

static HWND Combo(HWND p, int x, int y, int w, int id)
{
    return Ctl(p, WC_COMBOBOXW, L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, x, y, w, 300, id);
}

static HWND DatePicker(HWND p, int x, int y, int w, int id)
{
    HWND h = Ctl(p, DATETIMEPICK_CLASSW, L"", WS_TABSTOP | DTS_SHORTDATEFORMAT, x, y, w, 24, id);
    SYSTEMTIME range[2] = {};
    GetLocalTime(&range[1]);
    DateTime_SetRange(h, GDTR_MAX, range);   // будущие даты недоступны
    return h;
}

static std::wstring PickerIso(int id)
{
    SYSTEMTIME st;
    DateTime_GetSystemtime(Item(id), &st);
    return SystemTimeToIso(st);
}
static void SetPicker(int id, const std::wstring& iso)
{
    SYSTEMTIME st;
    if (IsoToSystemTime(iso, st)) DateTime_SetSystemtime(Item(id), GDT_VALID, &st);
}

static HWND ListView(HWND p, int x, int y, int w, int h, int id, const std::vector<std::pair<LPCWSTR, int>>& cols)
{
    HWND lv = Ctl(p, WC_LISTVIEWW, L"", WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL, x, y, w, h, id);
    ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    for (size_t i = 0; i < cols.size(); ++i) {
        LVCOLUMNW c = {};
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        c.fmt = (i == 0 || cols[i].second > 0) ? LVCFMT_LEFT : LVCFMT_RIGHT;
        c.cx = S(cols[i].second > 0 ? cols[i].second : -cols[i].second);
        c.pszText = (LPWSTR)cols[i].first;
        ListView_InsertColumn(lv, (int)i, &c);
    }
    return lv;
}

static void AddRow(HWND lv, const std::vector<std::wstring>& cells, LPARAM data)
{
    LVITEMW it = {};
    it.mask = LVIF_TEXT | LVIF_PARAM;
    it.iItem = ListView_GetItemCount(lv);
    it.pszText = (LPWSTR)cells[0].c_str();
    it.lParam = data;
    int row = ListView_InsertItem(lv, &it);
    for (size_t i = 1; i < cells.size(); ++i)
        ListView_SetItemText(lv, row, (int)i, (LPWSTR)cells[i].c_str());
}

static std::wstring ComboCurrency(int id)
{
    int i = (int)SendMessageW(Item(id), CB_GETCURSEL, 0, 0);
    return (i >= 0 && i < (int)(sizeof(kCurrencies) / sizeof(kCurrencies[0]))) ? kCurrencies[i].abbr : L"USD";
}

static const Period& ComboPeriod(int id)
{
    int i = (int)SendMessageW(Item(id), CB_GETCURSEL, 0, 0);
    if (i < 0 || i >= (int)g_periods.size()) i = 0;
    return g_periods[i];
}

// ---------- Получение курса с учётом выходных дней и кэша ----------
// Если на дату курс не установлен (выходной/праздник), берётся последний
// установленный курс, действующий на эту дату (REQ-FUNC-011).
static bool ResolveRate(const std::wstring& abbr, const std::wstring& date, RateInfo& out, std::wstring& err)
{
    if (abbr == L"BYN") {
        out = RateInfo();
        out.abbr = L"BYN";
        out.rate = 1.0;
        out.date = date;
        return true;
    }
    if (g_store.GetCachedRate(abbr, date, out)) return true;

    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    bool ok = false;
    for (int back = 0; back < 10; ++back) {
        std::wstring d = AddDays(date, -back);
        RateInfo info;
        FetchResult r = FetchRate(abbr, d, info, err);
        if (r == FetchResult::Ok) {
            g_store.PutCachedRate(date, info);
            out = info;
            ok = true;
            break;
        }
        if (r == FetchResult::NetError) {
            g_store.Log(L"GET /exrates/rates/" + abbr + L"?ondate=" + d + L" -> " + err);
            break;
        }
    }
    SetCursor(old);
    if (!ok && err.empty()) err = L"НБ РБ не публиковал курс " + abbr + L" на " + IsoToRu(date);
    return ok;
}

static std::wstring RateText(const RateInfo& r)
{
    return std::to_wstring(r.scale) + L" " + r.abbr + L" = " + FormatNumber(r.rate, 4) + L" BYN";
}

// ---------- Периоды ----------
static void BuildPeriods()
{
    SYSTEMTIME now;
    GetLocalTime(&now);
    g_periods.clear();
    g_periods.push_back({ L"Все записи", L"0000-01-01", L"9999-12-31" });
    static const wchar_t* q[] = { L"I", L"II", L"III", L"IV" };
    static const wchar_t* qFrom[] = { L"-01-01", L"-04-01", L"-07-01", L"-10-01" };
    static const wchar_t* qTo[] = { L"-03-31", L"-06-30", L"-09-30", L"-12-31" };
    for (int y = now.wYear; y >= now.wYear - 1; --y) {
        std::wstring ys = std::to_wstring(y);
        for (int i = 3; i >= 0; --i)
            g_periods.push_back({ std::wstring(q[i]) + L" квартал " + ys, ys + qFrom[i], ys + qTo[i] });
        g_periods.push_back({ L"Год " + ys, ys + L"-01-01", ys + L"-12-31" });
    }
}

static int CurrentQuarterIndex()
{
    SYSTEMTIME now;
    GetLocalTime(&now);
    return 1 + (3 - (now.wMonth - 1) / 3);   // порядок: IV, III, II, I текущего года
}

static void FillPeriodCombo(int id, int select)
{
    HWND h = Item(id);
    for (const auto& p : g_periods) SendMessageW(h, CB_ADDSTRING, 0, (LPARAM)p.name.c_str());
    SendMessageW(h, CB_SETCURSEL, select, 0);
}

static bool InPeriod(const Income& in, const Period& p) { return in.date >= p.from && in.date <= p.to; }

// ---------- Вкладка «Курс на дату» ----------
static void OnGetRate()
{
    std::wstring abbr = ComboCurrency(IDC_RATE_CUR), date = PickerIso(IDC_RATE_DATE), err;
    RateInfo r;
    if (!ResolveRate(abbr, date, r, err)) {
        SetText(IDC_RATE_RESULT, L"Курс недоступен");
        SetText(IDC_RATE_INFO, err);
        Status(L"Ошибка: " + err);
        return;
    }
    g_lastRate = r;
    g_haveRate = true;
    SetText(IDC_RATE_RESULT, RateText(r));
    std::wstring info = L"Официальный курс НБ РБ на " + IsoToRu(date);
    if (r.date != date) info += L" (установлен " + IsoToRu(r.date) + L")";
    if (r.fromCache) info += L" · из кэша";
    SetText(IDC_RATE_INFO, info);
    Status(L"Источник: api.nbrb.by" + std::wstring(r.fromCache ? L" · данные из локального кэша" : L" · онлайн"));
}

static void OnBuildChart()
{
    std::wstring abbr = ComboCurrency(IDC_RATE_CUR), from = PickerIso(IDC_DYN_FROM), to = PickerIso(IDC_DYN_TO), err;
    int days = DaysBetween(from, to);
    if (days <= 0 || days > 365) {
        Error(L"Период графика должен быть от 2 до 366 дней (дата «с» раньше даты «по»).");
        return;
    }
    RateInfo r;
    if (!ResolveRate(abbr, to, r, err) || r.curId == 0) {
        Error(L"Не удалось определить валюту для графика: " + err);
        return;
    }
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    std::vector<RatePoint> pts;
    FetchResult res = FetchDynamics(r.curId, from, to, pts, err);
    SetCursor(old);
    if (res != FetchResult::Ok) {
        if (res == FetchResult::NetError) g_store.Log(L"GET /exrates/rates/dynamics/" + std::to_wstring(r.curId) + L" -> " + err);
        Error(L"Не удалось получить динамику курса: " + (err.empty() ? std::wstring(L"нет данных за период") : err));
        return;
    }
    g_chart = pts;
    g_chartTitle = L"Официальный курс НБ РБ: " + std::to_wstring(r.scale) + L" " + abbr + L", BYN";
    InvalidateRect(Item(IDC_CHART), nullptr, TRUE);
    Status(L"График построен: " + std::to_wstring(pts.size()) + L" значений");
}

static void OnConvert()
{
    if (!g_haveRate) {
        Error(L"Сначала получите курс кнопкой «Получить курс».");
        return;
    }
    double amount;
    if (!ParseNumber(GetText(IDC_CONV_AMOUNT), amount) || amount < 0) {
        Error(L"Введите сумму числом, например 1500,00");
        return;
    }
    double byn = Round2(amount * g_lastRate.rate / g_lastRate.scale);
    SetText(IDC_CONV_RESULT, FormatNumber(amount, 2) + L" " + g_lastRate.abbr + L" = " + FormatNumber(byn, 2) + L" BYN");
}

static LRESULT CALLBACK ChartProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_ERASEBKGND) return 1;
    if (msg != WM_PAINT) return DefWindowProcW(hwnd, msg, wp, lp);

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);
    FillRect(mem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    FrameRect(mem, &rc, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
    SetBkMode(mem, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(mem, g_font);

    if (g_chart.size() < 2) {
        SetTextColor(mem, RGB(100, 116, 139));
        DrawTextW(mem, L"Выберите валюту и период, затем нажмите «Построить график»", -1, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        int left = S(64), right = rc.right - S(16), top = S(32), bottom = rc.bottom - S(32);
        double lo = g_chart[0].rate, hi = lo;
        for (const auto& p : g_chart) { lo = (std::min)(lo, p.rate); hi = (std::max)(hi, p.rate); }
        double pad = (hi - lo) * 0.1;
        if (pad < 1e-4) pad = lo * 0.01 + 1e-4;
        lo -= pad;
        hi += pad;

        SetTextColor(mem, RGB(30, 41, 59));
        SelectObject(mem, g_fontBold);
        TextOutW(mem, S(10), S(8), g_chartTitle.c_str(), (int)g_chartTitle.size());
        SelectObject(mem, g_font);

        HPEN grid = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        HPEN axis = CreatePen(PS_SOLID, 1, RGB(100, 116, 139));
        HPEN line = CreatePen(PS_SOLID, S(2), RGB(37, 99, 235));
        HGDIOBJ oldPen = SelectObject(mem, grid);
        SetTextColor(mem, RGB(100, 116, 139));
        for (int i = 0; i <= 4; ++i) {
            int y = bottom - (bottom - top) * i / 4;
            MoveToEx(mem, left, y, nullptr);
            LineTo(mem, right, y);
            std::wstring label = FormatNumber(lo + (hi - lo) * i / 4, 4);
            RECT lr = { 0, y - S(8), left - S(6), y + S(8) };
            DrawTextW(mem, label.c_str(), -1, &lr, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        }
        SelectObject(mem, axis);
        MoveToEx(mem, left, top, nullptr);
        LineTo(mem, left, bottom);
        LineTo(mem, right, bottom);

        size_t n = g_chart.size();
        auto px = [&](size_t i) { return left + (int)((right - left) * (double)i / (n - 1)); };
        auto py = [&](double v) { return bottom - (int)((bottom - top) * (v - lo) / (hi - lo)); };
        for (size_t i : { (size_t)0, n / 2, n - 1 }) {
            std::wstring d = IsoToRu(g_chart[i].date).substr(0, 5);
            RECT lr = { px(i) - S(40), bottom + S(6), px(i) + S(40), bottom + S(26) };
            DrawTextW(mem, d.c_str(), -1, &lr, DT_CENTER | DT_SINGLELINE);
        }
        SelectObject(mem, line);
        std::vector<POINT> pts(n);
        for (size_t i = 0; i < n; ++i) pts[i] = { px(i), py(g_chart[i].rate) };
        Polyline(mem, pts.data(), (int)n);
        HBRUSH dot = CreateSolidBrush(RGB(37, 99, 235));
        HGDIOBJ oldBrush = SelectObject(mem, dot);
        Ellipse(mem, pts[n - 1].x - S(4), pts[n - 1].y - S(4), pts[n - 1].x + S(4), pts[n - 1].y + S(4));
        SelectObject(mem, oldBrush);
        SelectObject(mem, oldPen);
        DeleteObject(dot);
        DeleteObject(grid);
        DeleteObject(axis);
        DeleteObject(line);
    }

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldFont);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
    return 0;
}

// ---------- Вкладка «Книга доходов» ----------
static void RefreshIncomes()
{
    HWND lv = Item(IDC_INC_LIST);
    SendMessageW(lv, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(lv);
    const Period& p = ComboPeriod(IDC_INC_PERIOD);
    double total = 0;
    int count = 0;
    for (size_t i = 0; i < g_store.incomes.size(); ++i) {
        const Income& in = g_store.incomes[i];
        if (!InPeriod(in, p)) continue;
        std::wstring rate = in.cur == L"BYN" ? L"—" : FormatNumber(in.rate, 4);
        if (in.scale > 1) rate += L" / " + std::to_wstring(in.scale);
        if (in.cur != L"BYN" && in.rateDate != in.date) rate += L" (на " + IsoToRu(in.rateDate).substr(0, 5) + L")";
        AddRow(lv, { IsoToRu(in.date), in.doc, in.cur, FormatNumber(in.amount, 2), rate, FormatNumber(in.byn, 2), in.comment }, (LPARAM)i);
        total += in.byn;
        ++count;
    }
    SendMessageW(lv, WM_SETREDRAW, TRUE, 0);
    SetText(IDC_INC_TOTAL, L"Итого за период «" + p.name + L"»: " + std::to_wstring(count) + L" записей, " +
                               FormatNumber(Round2(total), 2) + L" BYN");
}

static void ResetIncomeForm()
{
    g_editIndex = -1;
    SetText(IDC_INC_AMOUNT, L"");
    SetText(IDC_INC_DOC, L"");
    SetText(IDC_INC_COMMENT, L"");
    SetText(IDC_INC_ADD, L"Добавить");
    ShowWindow(Item(IDC_INC_CANCEL), SW_HIDE);
}

static void OnSaveIncome()
{
    Income in;
    in.date = PickerIso(IDC_INC_DATE);
    in.cur = ComboCurrency(IDC_INC_CUR);
    in.doc = GetText(IDC_INC_DOC);
    in.comment = GetText(IDC_INC_COMMENT);
    if (!ParseNumber(GetText(IDC_INC_AMOUNT), in.amount) || in.amount <= 0) {
        Error(L"Сумма должна быть положительным числом, например 1500,00");
        SetFocus(Item(IDC_INC_AMOUNT));
        return;
    }
    if (in.date > TodayIso()) {
        Error(L"Дата поступления не может быть позже текущей.");
        return;
    }
    RateInfo r;
    std::wstring err;
    if (!ResolveRate(in.cur, in.date, r, err)) {
        Error(L"Не удалось получить курс НБ РБ: " + err + L"\nЗапись не сохранена.");
        return;
    }
    in.rate = r.rate;
    in.scale = r.scale;
    in.rateDate = r.date;
    in.byn = Round2(in.amount * r.rate / r.scale);   // REQ-FUNC-003

    if (g_editIndex >= 0 && g_editIndex < (int)g_store.incomes.size()) g_store.incomes[g_editIndex] = in;
    else g_store.incomes.push_back(in);
    g_store.SortIncomes();
    if (!g_store.SaveIncomes()) Error(L"Не удалось сохранить книгу доходов на диск.");
    ResetIncomeForm();
    RefreshIncomes();
    Status(L"Сохранено: " + IsoToRu(in.date) + L", " + FormatNumber(in.amount, 2) + L" " + in.cur + L" = " +
           FormatNumber(in.byn, 2) + L" BYN");
}

static int SelectedIncome()
{
    HWND lv = Item(IDC_INC_LIST);
    int row = ListView_GetNextItem(lv, -1, LVNI_SELECTED);
    if (row < 0) return -1;
    LVITEMW it = {};
    it.mask = LVIF_PARAM;
    it.iItem = row;
    ListView_GetItem(lv, &it);
    return (int)it.lParam;
}

static void OnEditIncome()
{
    int i = SelectedIncome();
    if (i < 0) { Error(L"Выберите запись в таблице."); return; }
    const Income& in = g_store.incomes[i];
    SetPicker(IDC_INC_DATE, in.date);
    for (int c = 0; c < (int)(sizeof(kCurrencies) / sizeof(kCurrencies[0])); ++c)
        if (in.cur == kCurrencies[c].abbr) SendMessageW(Item(IDC_INC_CUR), CB_SETCURSEL, c, 0);
    SetText(IDC_INC_AMOUNT, FormatPlain(in.amount, 2));
    SetText(IDC_INC_DOC, in.doc);
    SetText(IDC_INC_COMMENT, in.comment);
    g_editIndex = i;
    SetText(IDC_INC_ADD, L"Сохранить");
    ShowWindow(Item(IDC_INC_CANCEL), SW_SHOW);
}

static void OnDeleteIncome()
{
    int i = SelectedIncome();
    if (i < 0) { Error(L"Выберите запись в таблице."); return; }
    if (MessageBoxW(g_hMain, L"Удалить выбранное поступление?", L"КурсНалог", MB_ICONQUESTION | MB_YESNO) != IDYES) return;
    g_store.incomes.erase(g_store.incomes.begin() + i);
    g_store.SaveIncomes();
    ResetIncomeForm();
    RefreshIncomes();
}

// ---------- Расчёт налога ----------
static TaxSummary Calculate(const Period& p, double rate)
{
    TaxSummary s;
    s.rate = rate;
    for (const auto& in : g_store.incomes) {
        if (!InPeriod(in, p)) continue;
        ++s.count;
        if (in.cur == L"BYN") s.byn += in.byn;
        else s.foreign += in.byn;
        s.byCurrency[in.cur] += in.byn;
    }
    s.foreign = Round2(s.foreign);
    s.byn = Round2(s.byn);
    s.base = Round2(s.foreign + s.byn);
    s.tax = Round2(s.base * rate / 100.0);   // REQ-FUNC-005
    return s;
}

static bool ReadTaxRate(double& rate)
{
    if (!ParseNumber(GetText(IDC_TAX_RATE), rate) || rate < 0 || rate > 100) {
        Error(L"Ставка налога должна быть числом от 0 до 100.");
        return false;
    }
    return true;
}

static void OnCalcTax()
{
    double rate;
    if (!ReadTaxRate(rate)) return;
    const Period& p = ComboPeriod(IDC_TAX_PERIOD);
    TaxSummary s = Calculate(p, rate);
    HWND lv = Item(IDC_TAX_LIST);
    ListView_DeleteAllItems(lv);
    AddRow(lv, { L"Период", p.name }, 0);
    AddRow(lv, { L"Количество поступлений", std::to_wstring(s.count) }, 0);
    AddRow(lv, { L"Выручка в иностранной валюте (пересчитано в BYN)", FormatNumber(s.foreign, 2) }, 0);
    for (const auto& kv : s.byCurrency)
        if (kv.first != L"BYN") AddRow(lv, { L"    в т.ч. " + kv.first, FormatNumber(Round2(kv.second), 2) }, 0);
    AddRow(lv, { L"Выручка в BYN", FormatNumber(s.byn, 2) }, 0);
    AddRow(lv, { L"Налоговая база за период", FormatNumber(s.base, 2) }, 0);
    AddRow(lv, { L"Ставка налога, %", FormatNumber(s.rate, 1) }, 0);
    AddRow(lv, { L"СУММА НАЛОГА К УПЛАТЕ", FormatNumber(s.tax, 2) }, 0);
    Status(L"Налог за период «" + p.name + L"»: " + FormatNumber(s.tax, 2) + L" BYN");
}

// ---------- Экспорт и печать ----------
static bool AskSavePath(const std::wstring& suggested, std::wstring& path)
{
    wchar_t file[MAX_PATH];
    wcsncpy_s(file, suggested.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMain;
    ofn.lpstrFilter = L"CSV (разделитель ;)\0*.csv\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"csv";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return false;
    path = file;
    return true;
}

static std::wstring IncomesCsv(const Period& p)
{
    std::wstring t = L"Дата;Документ;Валюта;Сумма;Курс НБ РБ;Масштаб;Дата курса;Сумма, BYN;Комментарий\r\n";
    for (const auto& in : g_store.incomes) {
        if (!InPeriod(in, p)) continue;
        t += IsoToRu(in.date) + L";" + CsvSafe(in.doc) + L";" + in.cur + L";" + FormatPlain(in.amount, 2) + L";" +
             FormatPlain(in.rate, 4) + L";" + std::to_wstring(in.scale) + L";" + IsoToRu(in.rateDate) + L";" +
             FormatPlain(in.byn, 2) + L";" + CsvSafe(in.comment) + L"\r\n";
    }
    return t;
}

static void SaveCsv(const std::wstring& text, const std::wstring& suggested)
{
    std::wstring path;
    if (!AskSavePath(suggested, path)) return;
    // UTF-8 с BOM — Excel корректно открывает кириллицу
    if (WriteAllUtf8Atomic(path, text, true)) Status(L"Файл сохранён: " + path);
    else Error(L"Не удалось записать файл " + path);
}

static void OnExportIncomes()
{
    SaveCsv(IncomesCsv(ComboPeriod(IDC_INC_PERIOD)), L"Книга_доходов.csv");
}

static void OnExportTax()
{
    double rate;
    if (!ReadTaxRate(rate)) return;
    const Period& p = ComboPeriod(IDC_TAX_PERIOD);
    TaxSummary s = Calculate(p, rate);
    std::wstring t = L"Расчёт налога;" + p.name + L"\r\n\r\n" + IncomesCsv(p) + L"\r\n";
    t += L"Выручка в иностранной валюте (пересчитано);" + FormatPlain(s.foreign, 2) + L"\r\n";
    t += L"Выручка в BYN;" + FormatPlain(s.byn, 2) + L"\r\n";
    t += L"Налоговая база;" + FormatPlain(s.base, 2) + L"\r\n";
    t += L"Ставка, %;" + FormatPlain(s.rate, 1) + L"\r\n";
    t += L"Сумма налога;" + FormatPlain(s.tax, 2) + L"\r\n";
    SaveCsv(t, L"Расчёт_налога.csv");
}

// Печать средствами GDI; для PDF выбирается принтер «Microsoft Print to PDF»
static void OnPrint()
{
    double rate;
    if (!ReadTaxRate(rate)) return;
    const Period& p = ComboPeriod(IDC_TAX_PERIOD);
    TaxSummary s = Calculate(p, rate);

    PRINTDLGW pd = {};
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = g_hMain;
    pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION;
    if (!PrintDlgW(&pd)) return;
    HDC dc = pd.hDC;
    int dpiX = GetDeviceCaps(dc, LOGPIXELSX), dpiY = GetDeviceCaps(dc, LOGPIXELSY);
    int pageH = GetDeviceCaps(dc, VERTRES);
    HFONT font = CreateFontW(-MulDiv(10, dpiY, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Arial");
    HFONT bold = CreateFontW(-MulDiv(13, dpiY, 72), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"Arial");
    int lineH = MulDiv(16, dpiY, 72), margin = dpiX * 2 / 3, y = margin;

    DOCINFOW di = {};
    di.cbSize = sizeof(di);
    di.lpszDocName = L"КурсНалог — расчёт налога";
    StartDocW(dc, &di);
    StartPage(dc);
    auto text = [&](int xInch10, const std::wstring& t) { TextOutW(dc, margin + dpiX * xInch10 / 10, y, t.c_str(), (int)t.size()); };
    auto newLine = [&]() {
        y += lineH;
        if (y > pageH - margin) { EndPage(dc); StartPage(dc); SelectObject(dc, font); y = margin; }
    };

    SelectObject(dc, bold);
    text(0, L"КурсНалог — книга учёта доходов и расчёт налога");
    y += lineH * 2;
    SelectObject(dc, font);
    text(0, L"Период: " + p.name + L"     Дата формирования: " + IsoToRu(TodayIso()));
    newLine(); newLine();
    text(0, L"Дата"); text(10, L"Документ"); text(26, L"Валюта"); text(34, L"Сумма"); text(46, L"Курс НБ РБ"); text(58, L"Сумма, BYN");
    newLine();
    for (const auto& in : g_store.incomes) {
        if (!InPeriod(in, p)) continue;
        text(0, IsoToRu(in.date)); text(10, in.doc.substr(0, 18)); text(26, in.cur);
        text(34, FormatNumber(in.amount, 2)); text(46, in.cur == L"BYN" ? L"—" : FormatNumber(in.rate, 4) + L" / " + std::to_wstring(in.scale));
        text(58, FormatNumber(in.byn, 2));
        newLine();
    }
    newLine();
    text(0, L"Выручка в иностранной валюте (пересчитано), BYN: " + FormatNumber(s.foreign, 2)); newLine();
    text(0, L"Выручка в BYN: " + FormatNumber(s.byn, 2)); newLine();
    text(0, L"Налоговая база, BYN: " + FormatNumber(s.base, 2)); newLine();
    text(0, L"Ставка налога, %: " + FormatNumber(s.rate, 1)); newLine();
    SelectObject(dc, bold);
    text(0, L"Сумма налога к уплате, BYN: " + FormatNumber(s.tax, 2)); newLine();
    SelectObject(dc, font);
    newLine();
    text(0, L"Источник курсов: официальный API Национального банка Республики Беларусь (api.nbrb.by)");

    EndPage(dc);
    EndDoc(dc);
    DeleteObject(font);
    DeleteObject(bold);
    DeleteDC(dc);
    if (pd.hDevMode) GlobalFree(pd.hDevMode);
    if (pd.hDevNames) GlobalFree(pd.hDevNames);
    Status(L"Отчёт отправлен на печать");
}

// ---------- Настройки и резервные копии ----------
static void OnSaveSettings()
{
    double r;
    if (!ParseNumber(GetText(IDC_SET_RATE), r) || r < 0 || r > 100) {
        Error(L"Ставка налога должна быть числом от 0 до 100.");
        return;
    }
    g_store.taxRate = r;
    g_store.SaveSettings();
    SetText(IDC_TAX_RATE, FormatPlain(r, 1));
    Status(L"Настройки сохранены");
}

static void OnBackup()
{
    BROWSEINFOW bi = {};
    bi.hwndOwner = g_hMain;
    bi.lpszTitle = L"Папка для резервной копии";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t folder[MAX_PATH];
    BOOL ok = SHGetPathFromIDListW(pidl, folder);
    CoTaskMemFree(pidl);
    if (!ok) return;
    std::wstring file, err;
    if (g_store.Backup(folder, file, err)) MessageBoxW(g_hMain, (L"Резервная копия создана:\n" + file).c_str(), L"КурсНалог", MB_ICONINFORMATION);
    else Error(err);
}

static void OnRestore()
{
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMain;
    ofn.lpstrFilter = L"Резервная копия КурсНалог (*.csv)\0*.csv\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;
    if (MessageBoxW(g_hMain, L"Текущая книга доходов будет заменена данными из копии. Продолжить?", L"КурсНалог",
                    MB_ICONQUESTION | MB_YESNO) != IDYES) return;
    std::wstring err;
    if (!g_store.Restore(file, err)) { Error(err); return; }
    ResetIncomeForm();
    RefreshIncomes();
    MessageBoxW(g_hMain, L"Данные восстановлены.", L"КурсНалог", MB_ICONINFORMATION);
}

// ---------- Построение интерфейса ----------
static LRESULT CALLBACK PageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_COMMAND:
    case WM_NOTIFY:
        return SendMessageW(g_hMain, msg, wp, lp);   // команды страниц обрабатывает главное окно
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, GetSysColor(COLOR_WINDOW));
        if (GetDlgCtrlID((HWND)lp) == IDC_RATE_RESULT) SetTextColor(dc, RGB(30, 58, 138));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void FillCurrencyCombo(int id, int count)
{
    HWND h = Item(id);
    for (int i = 0; i < count; ++i) {
        std::wstring s = std::wstring(kCurrencies[i].abbr) + L" — " + kCurrencies[i].name;
        SendMessageW(h, CB_ADDSTRING, 0, (LPARAM)s.c_str());
    }
    SendMessageW(h, CB_SETCURSEL, 0, 0);
}

static void CreatePages()
{
    for (int i = 0; i < 4; ++i)
        g_pages[i] = CreateWindowExW(WS_EX_CONTROLPARENT, L"RateTaxPage", L"", WS_CHILD | WS_CLIPSIBLINGS,
                                     0, 0, 10, 10, g_hMain, nullptr, g_hInst, nullptr);

    // Вкладка 1 — Курс на дату (UC-01, UC-05)
    HWND p = g_pages[0];
    Label(p, L"Валюта:", 16, 20, 60);
    Combo(p, 80, 16, 250, IDC_RATE_CUR);
    Label(p, L"Дата:", 350, 20, 40);
    DatePicker(p, 395, 16, 130, IDC_RATE_DATE);
    Button(p, L"Получить курс", 540, 14, 150, IDC_RATE_GET);
    Ctl(p, L"STATIC", L"Выберите валюту и дату", 0, 16, 56, 700, 36, IDC_RATE_RESULT, g_fontBig);
    Label(p, L"Источник: официальный API Национального банка Республики Беларусь", 16, 94, 700, 20, IDC_RATE_INFO);
    Ctl(p, L"BUTTON", L"Динамика курса", BS_GROUPBOX, 8, 124, 726, 450, -1);
    Label(p, L"с", 24, 152, 16);
    DatePicker(p, 42, 148, 125, IDC_DYN_FROM);
    Label(p, L"по", 180, 152, 24);
    DatePicker(p, 206, 148, 125, IDC_DYN_TO);
    Button(p, L"Построить график", 344, 146, 170, IDC_DYN_BUILD);
    CreateWindowExW(0, L"RateTaxChart", L"", WS_CHILD | WS_VISIBLE, S(20), S(184), S(702), S(378),
                    p, (HMENU)(INT_PTR)IDC_CHART, g_hInst, nullptr);
    Ctl(p, L"BUTTON", L"Конвертер", BS_GROUPBOX, 746, 124, 300, 240, -1);
    Label(p, L"Сумма в выбранной валюте:", 762, 152, 270);
    Edit(p, 762, 174, 180, IDC_CONV_AMOUNT);
    Button(p, L"Пересчитать", 762, 206, 130, IDC_CONV_BTN);
    Ctl(p, L"STATIC", L"", 0, 762, 246, 276, 44, IDC_CONV_RESULT, g_fontBold);
    Label(p, L"Пересчёт по курсу, полученному кнопкой «Получить курс», с учётом масштаба.", 762, 296, 276, 54);
    FillCurrencyCombo(IDC_RATE_CUR, kForeignCount);
    SetPicker(IDC_DYN_FROM, AddDays(TodayIso(), -90));

    // Вкладка 2 — Книга доходов (UC-02)
    p = g_pages[1];
    Label(p, L"Дата поступления", 16, 12, 130);
    Label(p, L"Валюта", 152, 12, 120);
    Label(p, L"Сумма", 282, 12, 110);
    Label(p, L"Документ", 402, 12, 150);
    Label(p, L"Комментарий", 562, 12, 240);
    DatePicker(p, 16, 32, 128, IDC_INC_DATE);
    Combo(p, 152, 32, 122, IDC_INC_CUR);
    Edit(p, 282, 32, 110, IDC_INC_AMOUNT);
    Edit(p, 402, 32, 150, IDC_INC_DOC);
    Edit(p, 562, 32, 250, IDC_INC_COMMENT);
    Button(p, L"Добавить", 822, 30, 110, IDC_INC_ADD);
    Button(p, L"Отмена", 940, 30, 100, IDC_INC_CANCEL);
    ShowWindow(Item(IDC_INC_CANCEL), SW_HIDE);
    Label(p, L"Период:", 16, 78, 60);
    Combo(p, 80, 74, 200, IDC_INC_PERIOD);
    Button(p, L"Изменить", 300, 72, 110, IDC_INC_EDIT);
    Button(p, L"Удалить", 420, 72, 110, IDC_INC_DELETE);
    Button(p, L"Экспорт CSV", 540, 72, 130, IDC_INC_EXPORT);
    ListView(p, 16, 112, 1028, 420, IDC_INC_LIST,
             { { L"Дата", 90 }, { L"Документ", 130 }, { L"Валюта", 70 }, { L"Сумма", -120 },
               { L"Курс НБ РБ", -170 }, { L"Сумма, BYN", -120 }, { L"Комментарий", 300 } });
    Ctl(p, L"STATIC", L"", 0, 16, 542, 1028, 24, IDC_INC_TOTAL, g_fontBold);
    FillCurrencyCombo(IDC_INC_CUR, (int)(sizeof(kCurrencies) / sizeof(kCurrencies[0])));
    FillPeriodCombo(IDC_INC_PERIOD, 0);

    // Вкладка 3 — Расчёт налога (UC-03, UC-04)
    p = g_pages[2];
    Label(p, L"Период:", 16, 20, 60);
    Combo(p, 80, 16, 200, IDC_TAX_PERIOD);
    Label(p, L"Ставка, %:", 300, 20, 80);
    Edit(p, 385, 16, 70, IDC_TAX_RATE);
    Button(p, L"Рассчитать", 470, 14, 130, IDC_TAX_CALC);
    Button(p, L"Экспорт CSV", 760, 14, 130, IDC_TAX_EXPORT);
    Button(p, L"Печать / PDF", 900, 14, 144, IDC_TAX_PRINT);
    ListView(p, 16, 60, 640, 330, IDC_TAX_LIST, { { L"Показатель", 430 }, { L"Значение, BYN", -180 } });
    Label(p, L"Ставка задаётся пользователем. Приложение не выдаёт налоговых консультаций и не подаёт декларацию.\n"
             L"Для получения PDF выберите в окне печати принтер «Microsoft Print to PDF».", 16, 402, 1000, 44);
    FillPeriodCombo(IDC_TAX_PERIOD, CurrentQuarterIndex());
    SetText(IDC_TAX_RATE, FormatPlain(g_store.taxRate, 1));

    // Вкладка 4 — Настройки (REQ-FUNC-006, REQ-REL-002)
    p = g_pages[3];
    Label(p, L"Ставка налога по умолчанию, %:", 16, 20, 230);
    Edit(p, 250, 16, 70, IDC_SET_RATE);
    Button(p, L"Сохранить", 332, 14, 120, IDC_SET_SAVE);
    Label(p, (L"Папка данных: " + g_store.Dir()).c_str(), 16, 64, 1000);
    Button(p, L"Создать резервную копию…", 16, 100, 250, IDC_SET_BACKUP);
    Button(p, L"Восстановить из копии…", 280, 100, 250, IDC_SET_RESTORE);
    Label(p, L"Автоматическая резервная копия создаётся раз в 7 дней в папке backups (хранятся 5 последних).\n"
             L"Курсы, полученные из Интернета, сохраняются в локальный кэш и доступны без подключения к сети.\n"
             L"Источник курсов: официальный API Национального банка Республики Беларусь (api.nbrb.by).", 16, 150, 1000, 70);
    SetText(IDC_SET_RATE, FormatPlain(g_store.taxRate, 1));
}

static void ShowPage(int index)
{
    for (int i = 0; i < 4; ++i) ShowWindow(g_pages[i], i == index ? SW_SHOW : SW_HIDE);
    SetWindowPos(g_pages[index], HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

static void Layout()
{
    RECT rc;
    GetClientRect(g_hMain, &rc);
    SendMessageW(g_hStatus, WM_SIZE, 0, 0);
    RECT sb;
    GetWindowRect(g_hStatus, &sb);
    int statusH = sb.bottom - sb.top;
    int m = S(8);
    MoveWindow(g_hTab, m, m, rc.right - 2 * m, rc.bottom - statusH - 2 * m, TRUE);
    RECT page = { 0, 0, rc.right - 2 * m, rc.bottom - statusH - 2 * m };
    TabCtrl_AdjustRect(g_hTab, FALSE, &page);
    for (HWND p : g_pages)
        MoveWindow(p, page.left + m, page.top + m, page.right - page.left, page.bottom - page.top, TRUE);
}

static void OnCreate(HWND hwnd)
{
    g_hMain = hwnd;
    g_hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
                                hwnd, (HMENU)(INT_PTR)IDC_STATUS, g_hInst, nullptr);
    SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_font, TRUE);
    g_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP, 0, 0, 10, 10,
                             hwnd, (HMENU)(INT_PTR)IDC_TAB, g_hInst, nullptr);
    SendMessageW(g_hTab, WM_SETFONT, (WPARAM)g_font, TRUE);
    const wchar_t* tabs[] = { L"Курс на дату", L"Книга доходов", L"Расчёт налога", L"Настройки" };
    for (int i = 0; i < 4; ++i) {
        TCITEMW t = {};
        t.mask = TCIF_TEXT;
        t.pszText = (LPWSTR)tabs[i];
        TabCtrl_InsertItem(g_hTab, i, &t);
    }
    BuildPeriods();
    CreatePages();
    RefreshIncomes();
    Layout();
    ShowPage(0);
    g_store.AutoBackup();
    Status(L"Готово · данные: " + g_store.Dir());
}

static void OnCommand(int id, int code)
{
    if (code == CBN_SELCHANGE && id == IDC_INC_PERIOD) { RefreshIncomes(); return; }
    if (code != BN_CLICKED) return;
    switch (id) {
    case IDC_RATE_GET: OnGetRate(); break;
    case IDC_DYN_BUILD: OnBuildChart(); break;
    case IDC_CONV_BTN: OnConvert(); break;
    case IDC_INC_ADD: OnSaveIncome(); break;
    case IDC_INC_CANCEL: ResetIncomeForm(); break;
    case IDC_INC_EDIT: OnEditIncome(); break;
    case IDC_INC_DELETE: OnDeleteIncome(); break;
    case IDC_INC_EXPORT: OnExportIncomes(); break;
    case IDC_TAX_CALC: OnCalcTax(); break;
    case IDC_TAX_EXPORT: OnExportTax(); break;
    case IDC_TAX_PRINT: OnPrint(); break;
    case IDC_SET_SAVE: OnSaveSettings(); break;
    case IDC_SET_BACKUP: OnBackup(); break;
    case IDC_SET_RESTORE: OnRestore(); break;
    }
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        OnCreate(hwnd);
        return 0;
    case WM_SIZE:
        if (g_hTab) Layout();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize = { S(1120), S(720) };
        return 0;
    }
    case WM_COMMAND:
        OnCommand(LOWORD(wp), HIWORD(wp));
        return 0;
    case WM_NOTIFY: {
        auto* nm = (NMHDR*)lp;
        if (nm->hwndFrom == g_hTab && nm->code == TCN_SELCHANGE) ShowPage(TabCtrl_GetCurSel(g_hTab));
        else if (nm->idFrom == IDC_INC_LIST && nm->code == NM_DBLCLK) OnEditIncome();
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow)
{
    g_hInst = hInst;
    SetProcessDPIAware();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);   // для диалога выбора папки

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    HDC screen = GetDC(nullptr);
    g_dpi = GetDeviceCaps(screen, LOGPIXELSY);
    ReleaseDC(nullptr, screen);
    g_font = CreateFontW(-MulDiv(9, g_dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBold = CreateFontW(-MulDiv(10, g_dpi, 72), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBig = CreateFontW(-MulDiv(18, g_dpi, 72), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    std::wstring err;
    if (!g_store.Init(err)) {
        MessageBoxW(nullptr, err.c_str(), L"КурсНалог", MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    wc.lpfnWndProc = PageProc;
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = L"RateTaxPage";
    RegisterClassExW(&wc);

    wc.lpfnWndProc = ChartProc;
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"RateTaxChart";
    RegisterClassExW(&wc);

    wc.lpfnWndProc = MainProc;
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName = L"RateTaxMain";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, L"RateTaxMain", L"КурсНалог — архивные курсы НБ РБ для расчёта налога ИП",
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, S(1120), S(720),
                                nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(hwnd, &msg)) continue;   // переход между полями клавишей Tab
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_font);
    DeleteObject(g_fontBold);
    DeleteObject(g_fontBig);
    CoUninitialize();
    return (int)msg.wParam;
}
