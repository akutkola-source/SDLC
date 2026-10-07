// Прототип графического интерфейса системы «КурсНалог»
// Язык: SysML v2 (текстовая нотация). Каждый экран описан как part,
// элементы управления — как вложенные part с атрибутами расположения.
// Графическое представление: ../img/ui-main.png, ../img/ui-income.png, ../img/ui-report.png

package RateTaxUI {
    private import ScalarValues::*;

    // ---------- Библиотека элементов интерфейса ----------
    abstract part def Widget {
        attribute x : Integer;
        attribute y : Integer;
        attribute width : Integer;
        attribute height : Integer;
    }
    part def Window :> Widget { attribute title : String; }
    part def Label :> Widget { attribute text : String; }
    part def Button :> Widget { attribute caption : String; attribute isPrimary : Boolean = false; }
    part def ComboBox :> Widget { attribute items : String[*]; attribute selected : String; }
    part def DatePicker :> Widget { attribute value : String; }
    part def NumberBox :> Widget { attribute value : Real; }
    part def DataGrid :> Widget { attribute columns : String[*]; }
    part def Chart :> Widget { attribute seriesName : String; }
    part def TabBar :> Widget { attribute tabs : String[*]; attribute active : String; }
    part def StatusBar :> Widget { attribute text : String; }

    // ---------- Экран 1: Курс на дату (UC-01, UC-05) ----------
    part mainScreen : Window {
        attribute redefines title = "КурсНалог — Курс на дату";
        attribute redefines width = 1100;
        attribute redefines height = 680;

        part tabs : TabBar {
            attribute redefines tabs = ("Курс на дату", "Книга доходов", "Расчёт налога", "Настройки");
            attribute redefines active = "Курс на дату";
        }
        part currency : ComboBox {
            attribute redefines items = ("USD", "EUR", "RUB", "CNY", "PLN");
            attribute redefines selected = "USD";
        }
        part onDate : DatePicker;
        part btnGetRate : Button { attribute redefines caption = "Получить курс"; attribute redefines isPrimary = true; }
        part rateCard : Label { attribute redefines text = "1 USD = 2,9547 BYN"; }
        part periodFrom : DatePicker;
        part periodTo : DatePicker;
        part dynamics : Chart { attribute redefines seriesName = "Официальный курс НБ РБ"; }
        part converterAmount : NumberBox;
        part converterResult : Label;
        part status : StatusBar { attribute redefines text = "Источник: api.nbrb.by · кэш обновлён"; }
    }

    // ---------- Экран 2: Книга учёта доходов (UC-02) ----------
    part incomeScreen : Window {
        attribute redefines title = "КурсНалог — Книга доходов";
        part btnAdd : Button { attribute redefines caption = "+ Добавить поступление"; attribute redefines isPrimary = true; }
        part btnImport : Button { attribute redefines caption = "Импорт выписки (CSV)"; }
        part grid : DataGrid {
            attribute redefines columns = ("Дата", "Документ", "Валюта", "Сумма", "Курс НБ РБ", "Сумма, BYN", "Комментарий");
        }
        part totalLabel : Label { attribute redefines text = "Итого за период, BYN"; }

        part addDialog : Window {
            attribute redefines title = "Новое поступление";
            part date : DatePicker;
            part cur : ComboBox;
            part amount : NumberBox;
            part doc : Label { attribute redefines text = "Номер платёжного документа"; }
            part rateHint : Label { attribute redefines text = "Курс подставляется автоматически"; }
            part btnSave : Button { attribute redefines caption = "Сохранить"; attribute redefines isPrimary = true; }
            part btnCancel : Button { attribute redefines caption = "Отмена"; }
        }
    }

    // ---------- Экран 3: Расчёт налога и отчёт (UC-03, UC-04) ----------
    part reportScreen : Window {
        attribute redefines title = "КурсНалог — Расчёт налога";
        part period : ComboBox {
            attribute redefines items = ("I квартал", "II квартал", "III квартал", "IV квартал", "Год");
        }
        part taxRate : NumberBox;
        part summary : DataGrid {
            attribute redefines columns = ("Показатель", "Значение, BYN");
        }
        part btnExportCsv : Button { attribute redefines caption = "Экспорт CSV"; }
        part btnPrint : Button { attribute redefines caption = "Печать / PDF"; attribute redefines isPrimary = true; }
    }

    // ---------- Навигация между экранами ----------
    state def Navigation {
        entry; then RateView;
        state RateView;
        state IncomeView;
        state ReportView;
        transition RateView_to_Income first RateView then IncomeView;
        transition Income_to_Report first IncomeView then ReportView;
        transition Report_to_Rate first ReportView then RateView;
    }
}
