#pragma once

#include "common.h"
#include "formula.h"

#include <variant>
#include <string_view>
#include <cassert>
#include <iostream>
#include <string>
#include <optional>

class Sheet;

// Исключение, выбрасываемое при попытке некорректного прочтения строки
class CellException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Базовый интерфейс представления ячейки
class Impl {
public:

    virtual ~Impl() = default;
    virtual std::string GetString() const = 0;                            // возвращает строковое представление данных
    virtual CellInterface::Value GetValue() const = 0;                    // возвращает значение формулы или строки
};

// Представление пустой ячейки
class EmptyImpl : public Impl {
public:
    explicit EmptyImpl(std::string_view text)
        : _data(std::string(text)) {
    }

    std::string GetString() const override {
        return "";
    }

    CellInterface::Value GetValue() const override {
        return "";
    }
private:
    std::string _data;
};

// Представление текстовой ячейки
class TextImpl : public Impl {
public:
    explicit TextImpl(std::string_view text) 
        : _data(std::string(text)) {
    }

    CellInterface::Value GetValue() const override {
        // если в данных строки апостров
        if (_data[0] == ESCAPE_SIGN) {
            // возвращаем Value без него
            return _data.substr(1);
        }
        return _data;
    }

    std::string GetString() const override {
        return _data;
    }
private:
    std::string _data;
};

// Представление формульной ячейки
class FormulaImpl : public Impl {
public:
    FormulaImpl() = default;

    explicit FormulaImpl(const SheetInterface& sheet, std::string text)
        : _sheet(sheet), _data(ParseFormula(std::string(text))) {
    }

    // возвращает указатель на формулу
    FormulaInterface* GetFormula() {
        return _data.get();
    }

    // возвращает вектор зависимостей формулы
    bool HasDepends() const {
        return _data.get()->HasDepends();
    }

    // возвращает вектор зависимостей формулы
    std::vector<Position> GetReferencedCells() const {
        return _data.get()->GetReferencedCells();
    }

    std::string GetString() const override {
        // возвращаем текстовое представление со знаком равно
        return FORMULA_SIGN + _data->GetExpression();
    }

    CellInterface::Value GetValue() const override {
        // если данные еще не кешированны
        if (!IsCached()) {
            // сначала записываем в кеш
            _cache_result = _data->Evaluate(_sheet);
        }

        // возвращаем результат в зависимости от того, что хранится в кеше
        if (std::holds_alternative<double>(_cache_result.value())) {
            return std::get<double>(_cache_result.value());
        }
        else if (std::holds_alternative<FormulaError>(_cache_result.value())) {
            return std::get<FormulaError>(_cache_result.value());
        }
        else {
            throw CellException("ERROR::FormulaImpl::GetValue()::uncorrect value" + std::to_string(__LINE__));
        }
    }

    bool IsCached() const {
        return _cache_result.has_value();
    }

    void ClearCache() {
        if (IsCached()) _cache_result.reset();
    }

private:
    const SheetInterface& _sheet;                                                 // ссылка на таблицу
    std::unique_ptr<FormulaInterface> _data;                                      // формульные данные
    mutable std::optional<FormulaInterface::Value> _cache_result;                 // кешированный результат работы формулы
};


class Cell : public CellInterface {
private:
    Sheet& _sheet;
public:
    Cell() = default;
    ~Cell();

    Cell(Sheet& /*sheet*/, Position/*pos*/);                                      // конструктор от ссылки на таблицу

    Cell(const Cell&) = delete;                                                   // запрещаем конструктор копирования
    Cell& operator=(const Cell& /*other*/) = delete;                              // запрещаем оператор присваивания
    Cell(Cell&&) = delete;                                                        // запрещаем конструктор присваивания
    Cell& operator=(Cell&& /*other*/) = delete;                                   // запрещаем оператор перемещения
 
    // --------------------------------------- сеттеры класса ----------------------------------------------------------------------

    void SetData(std::string /*text*/);                                           // задать новое содержимое ячейки
    void SetPosition(Position /*pos*/);                                           // задать позицию ячейки

    // --------------------------------------- геттеры класса ----------------------------------------------------------------------

    Value GetValue() const override;                                              // получить расчётное значение ячейки
    std::string GetText() const override;                                         // получить текстовое представление ячейки
    std::vector<Position> GetReferencedCells() const override;                    // получить содержимое пула зависимостей формулы
    const std::string& GetTextData() const;                                       // получить базовую строку ячйеки

    // --------------------------------------- блок работы с зависимостями класса --------------------------------------------------

    void AddDependentCell(Position /*pos*/);                                      // добавить зависимую ячейку
    void AddDependentCell(const std::vector<Position>& /*dependent*/);            // добавить вектор зависимых ячейк

    void AddDependsOn(Position /*pos*/);                                          // добавить ячейку от которой зависит текущая
    void AddDependsOn(const std::vector<Position>& /*depends*/);                  // добавить вектор ячеек от которой зависит текущая

    bool IsDependentCell(Position /*pos*/) const;                                 // подтверждает что позиция является зависимой от текущей
    bool IsDependsFromCell(Position /*pos*/) const;                               // подтверждает что данная ячейка зависит от позиции
    bool CyclicRecurceCheck(Position /*pos*/) const;                              // проверка на циклическую зависимость

    std::vector<Position> GetDependent() const;                                   // возвращает вектор ячеек зависимых от текущей
    std::vector<Position> GetDependsOn() const;                                   // возвращает вектор ячеек, от которых зависит текущая

    // --------------------------------------- блок печати класса ------------------------------------------------------------------

    void PrintValue(std::ostream& /*out*/);                                       // печать GetValue в поток
    void PrintText(std::ostream& /*out*/);                                        // печать GetText в поток

    // --------------------------------------- операции переноса и удаления --------------------------------------------------------

    void Copy(const Cell& /*other*/);                                             // скопировать содержимое другой ячейки
    void Move(Cell& /*other*/);                                                   // переместить содержимое из другой ячейки
    void Swap(Cell& /*other*/);                                                   // обменять содержимое ячеек
    void ClearCache();                                                            // очистить ранее посчитаный кеш формулы
    void Clear();                                                                 // удалить содержимое ячейки

    // --------------------------------------- булевые флаги класса ----------------------------------------------------------------

    bool IsEmpty() const;                                                         // возвращает флаг незаполненной ячейки
    bool IsText() const;                                                          // возвращает флаг текстовой ячейки
    bool IsFormula() const;                                                       // возвращает флаг ячейки с формульным выражением

    bool IsReference() const;                                                     // возвращает флаг того, что ячейка является ссылкой
    bool IsRoot() const;                                                          // возвращает флаг того, что на данную ячейку ссылаются

    bool IsRaw() const;                                                           // возвращает флаг сырой ячейки без данных
    bool IsEqual(const Cell& /*other*/) const;                                    // флаг равенство ячеек по значениям
    bool IsEqual(const Cell* /*other*/) const;                                    // флаг равенство ячеек по значениям
    bool IsEqual(const CellInterface* /*other*/) const;                           // флаг равенство ячеек по значениям

private:
    std::string _string_data = "";                                                // базовая строка
    std::unique_ptr<Impl> _impl;                                                  // содержимое ячейки
    Position _pos = Position::NONE;                                               // позиция ячейки при создании

    std::vector<Position> _dependent;                                             // зависимые ячейки, которые ссылаются на эту
    std::vector<Position> _depends_on;                                            // ячейки, от которых зависит данная
    
    TextImpl* AsText() const;                                                     // кастует данные ячейки как текст
    FormulaImpl* AsFormula() const;                                               // кастует данные ячейки  как формулу

    Value GetFormulaEvaluate() const;                                             // получение результата работы формулы

    // флаг работы менеджера ссылок
    enum RManagerFlag
    {
        update_roots,        // обычное обновление вектора ссылок ячеек, от которых зависит текущая
        clear_cache,         // рекурсивная очистка кеша у всего пула зависимых ячеек
        cyclic_check         // проверка на образование циклической зависимости
    };

    void ReferenceManager(RManagerFlag /*flag*/, const std::vector<Position>& /*refs*/); // менеджер обработки ссылок
};

bool operator==(const Cell& /*lhs*/, const Cell& /*rhs*/);
bool operator!=(const Cell& /*lhs*/, const Cell& /*rhs*/);