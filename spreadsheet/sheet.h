#pragma once

#include "cell.h"
#include "common.h"

#include <functional>
#include <vector>
#include <unordered_set>
#include <unordered_map>

// Описывает ошибки, которые могут возникнуть при работе с таблицей.
class SheetError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Sheet : public SheetInterface {
public:
    using SheetData = std::unordered_map<Position, std::unique_ptr<Cell>, PositionHasher>;
    using FutureReferences = std::unordered_map<Position, std::unordered_set<Position, PositionHasher>, PositionHasher>;

    // флаг выполняемой операции применяется при работе со вставкой и изменениями размера строки и таблицы
    enum OpFlag
    {
        set, get, copy, move, swap, clear
    };

    // флаг завершения работы метода PrintSizeManager()
    enum PSizeFlag
    {
        actual,                    // область печати в актуальном состоянии
        not_actual                 // необходимо пересчитать область печати
    };

    Sheet() = default;                                                                // базовый конструктор пустой таблицы
    ~Sheet();

    Sheet(const Sheet& /*other*/);                                                    // конструктор копирования
    Sheet& operator=(const Sheet& /*other*/);                                         // оператор присваивания

    Sheet(Sheet&& /*other*/) noexcept;                                                // конструктор перемещения
    Sheet& operator=(Sheet&& /*other*/) noexcept;                                     // оператор перемещения

    explicit Sheet(SheetData&& /*data*/);                                             // конструктор двухмерной базы

    // --------------------------------------- блок вставки и удаления элементов ------------------------------------------------------

    void SetCell(Position pos, std::string text) override;                            // назначает ячейку из строки по позиции
    void CopyCell(Position /*from*/, Position /*to*/);                                // скопировать ячейку из одной позиции в другую
    void MoveCell(Position /*from*/, Position /*to*/);                                // переместить ячейку из одной позиции в другую

    // можно добавить методы в зависимости от задания и необходимости

    const CellInterface* GetCell(Position pos) const override;                        // выдаёт ячейку по позиции
    CellInterface* GetCell(Position pos) override;                                    // выдаёт ячейку по позиции

    const Cell* GetDirectCell(Position pos) const;                                    // выдаёт ячейку по позиции
    Cell* GetDirectCell(Position pos);                                                // выдаёт ячейку по позиции

    void ClearCell(Position pos) override;                                            // удаляет ячейку по позиции
    Sheet& EraseSheet();                                                              // удаляет данные таблицы

    // --------------------------------------- блок вспомогательных методов класса ----------------------------------------------------

    Size GetPrintableSize() const override;                                           // выдает размер печатной области

    void PrintValues(std::ostream& output) const override;                            // вывод печатной области по значениям
    void PrintTexts(std::ostream& output) const override;                             // вывод печатной области по текстовому представлению

    Sheet& SwapSheet(Sheet& /*other*/);                                               // свапает таблицы местами по ссылке
    Sheet& SwapSheet(Sheet* /*other*/);                                               // свапает таблицы местами по указателю

    // --------------------------------------- блок работы с отложенными ссылками -----------------------------------------------------

    bool IsFutureDependendCell(Position /*pos*/) const;                               // возвращает флаг того, что от данной ячейки зависят
    bool IsFutureRefsActual() const;                                                  // возвращает флаг пустоты пула отложенных ссылок
    void AddFutureRefLine(Position /*from*/, Position /*to*/);                        // добавить направление ссылки на будущее обновление
    void UpdateFutureReferences();                                                    // провести обновление всех возможных отложенных ссылок
    void UpdateFutureReferences(Position /*pos*/);                                    // провести обновление отложенных ссылок по позиции

    // --------------------------------------- булевые флаги состояния класса ---------------------------------------------------------

    bool IsEmpty() const;                                                             // возвращает флаг того, что таблица пуста
    bool IsValid(Position pos) const;                                                 // флаг существующей доступной ячейки

    bool IsEqual(const Sheet& /*other*/) const;                                       // флаг равенство таблиц по значениям
    bool IsEqual(const Sheet* /*other*/) const;                                       // флаг равенство таблиц по значениям
    bool IsEqual(const SheetInterface* /*other*/) const;                              // флаг равенство таблиц по значениям

    // --------------------------------------- итераторы доступа класса ---------------------------------------------------------------

    auto Begin();                                                                     // базовый итератор доступа begin()
    auto End();                                                                       // базовый итератор доступа end()
    auto Begin() const;                                                               // константный итератор доступа begin()
    auto End() const;                                                                 // константный итератор доступа begin()
    auto cBegin() const;                                                              // константный итератор доступа cbegin()
    auto cEnd() const;                                                                // константный итератор доступа cend()

private:

    SheetData _data;                                                                  // базовый двухмерный массив таблицы
    Size _print = { 0, 0 };                                                           // величина печатной области
    PSizeFlag _ps_flag = not_actual;                                                  // флаг состояния печатной области
    FutureReferences _future_refs;                                                    // пул ссылок на отложенное обновление

    std::unique_ptr<Cell> _DUMMY;                                                     // виртуальная заглушка. Смотри метод GetCell(Position pos)
    const CellInterface* GetDummy(Position /*pos*/);                                  // возвращает виртуальную загрушку

    PSizeFlag PrintSizeCalculate();                                                   // калькулятор области печати
    PSizeFlag PrintSizeManager(Position /*pos*/, OpFlag /*flag*/);                    // управление печатной областью

    bool SheetСomparison(const Sheet& /*other*/) const;                               // прямое сравнение таблиц по ячейкам
};

// булевые флаги показывают только равенство/неравенство по расположению в памяти и размеру занимаемой области памяти!
bool operator==(const Sheet& /*lhs*/, const Sheet& /*rhs*/);       // для построчного сравнения используется метод IsEqual()
bool operator!=(const Sheet& /*lhs*/, const Sheet& /*rhs*/);