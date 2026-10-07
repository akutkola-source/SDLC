// Контекстная модель и варианты использования системы «КурсНалог»
// Язык: SysML v2 (текстовая нотация)

package RateTaxContext {
    private import ScalarValues::*;

    // ---------- Акторы ----------
    part def IndividualEntrepreneur {
        doc /* Индивидуальный предприниматель, получающий выручку в иностранной валюте */
    }
    part def Accountant {
        doc /* Бухгалтер на аутсорсе, ведущий учёт нескольких ИП */
    }
    part def NbrbApi {
        doc /* Внешний сервис: REST API Национального банка Республики Беларусь (api.nbrb.by) */
        attribute baseUrl : String = "https://api.nbrb.by/exrates";
    }

    // ---------- Система ----------
    part def RateTaxApp {
        doc /* Desktop-приложение «КурсНалог» */
        port nbrbPort : RatesPort;
        port fileExport : ExportPort;
    }

    port def RatesPort {
        out item rateRequest : RateRequest;
        in item rateResponse : RateResponse;
    }
    port def ExportPort {
        out item report : TaxReport;
    }

    item def RateRequest {
        attribute curAbbreviation : String;
        attribute onDate : String;
    }
    item def RateResponse {
        attribute curScale : Integer;
        attribute curOfficialRate : Real;
    }
    item def TaxReport {
        attribute periodFrom : String;
        attribute periodTo : String;
        attribute totalIncomeByn : Real;
        attribute taxAmountByn : Real;
    }

    // ---------- Контекст ----------
    part context {
        part user : IndividualEntrepreneur;
        part accountant : Accountant;
        part app : RateTaxApp;
        part nbrb : NbrbApi;

        connect app.nbrbPort to nbrb;
    }

    // ---------- Варианты использования ----------
    use case def ViewRateOnDate {
        doc /* UC-01: получить официальный курс выбранной валюты на дату */
        subject app : RateTaxApp;
        actor user : IndividualEntrepreneur;
    }
    use case def RegisterIncome {
        doc /* UC-02: зарегистрировать поступление выручки в валюте и пересчитать в BYN */
        subject app : RateTaxApp;
        actor user : IndividualEntrepreneur;
        include use case getRate : ViewRateOnDate;
    }
    use case def CalculateTax {
        doc /* UC-03: рассчитать налоговую базу и сумму налога за период */
        subject app : RateTaxApp;
        actor user : IndividualEntrepreneur;
    }
    use case def ExportReport {
        doc /* UC-04: выгрузить книгу учёта доходов в CSV и распечатать расчёт (в т.ч. в PDF) */
        subject app : RateTaxApp;
        actor accountant : Accountant;
    }
    use case def AnalyseDynamics {
        doc /* UC-05: просмотреть график динамики курса за период */
        subject app : RateTaxApp;
        actor user : IndividualEntrepreneur;
    }
}
