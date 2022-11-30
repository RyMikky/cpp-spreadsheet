#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>
//#include <execution>

using namespace std::literals;

// ----------------------------------- class Sheet -------------------------------------------------------

Sheet::~Sheet() {
    for (auto& cell : _data) {
        if (cell.second) {
            cell.second.release()->~Cell();
        }
    }
}

// конструктор копирования
Sheet::Sheet(const Sheet& other)
    : _print(other._print), _ps_flag(other._ps_flag) {

    for (const auto& item : other._data) {
        SetCell(item.first, item.second->GetTextData());
    }
}
// оператор присваивания
Sheet& Sheet::operator=(const Sheet& other) {

    // исключаем самокопирование
    if (*this != other && !IsEqual(other)) {

        // для начала удаляем имеющиеся данные и освобождаем память
        EraseSheet();

        // перезабиваем таблицу по новой
        for (const auto& item : other._data) {
            SetCell(item.first, item.second->GetTextData());
        }

        // если у исходного с печатной областью всё ок, то берем её
        // иначе всё равно придётся пересчитывать
        if (other._ps_flag == PSizeFlag::actual) {
            // перезаписываем размер печатной области
            _print = other._print;
            _ps_flag = PSizeFlag::actual;
        }
    }
    return *this;
}

// конструктор перемещения
Sheet::Sheet(Sheet&& other) noexcept
    : _data(std::move(other._data))
    , _print(std::move(other._print))
    , _ps_flag(std::move(other._ps_flag)) {
}
// оператор перемещения
Sheet& Sheet::operator=(Sheet&& other) noexcept {

    // исключаем самокопирование
    if (*this != other && !IsEqual(other)) {
        _data = std::move(other._data);

        _print = std::move(other._print);
        _ps_flag = std::move(other._ps_flag);
    }
    return *this;
}

// конструктор из вектора строк
Sheet::Sheet(SheetData&& data)
    : _data(std::move(data)) {
}

void Sheet::SetCell(Position pos, std::string text) {

    // для начала проверяем может быть такая ячейка вообще есть
    // внутренний метод IsValid() возвращает true, если ячейка существует, false, если нет
    // и пробразывает исключение о выходе за пределы при out of limmit
    if (!IsValid(pos)) {
        // если же такой ячейки еще не было, то просто создаём новую
        _data[pos] = std::make_unique<Cell>(*this, pos);
    }

    // загружаем в неё данные, а там уже разберутся, что ресетить, что удалять и вообще как с этим быть
    _data.at(pos)->SetData(text);
    // также после имплементации, если всё окей, обновляем печатный размер
    _ps_flag = PrintSizeManager(pos, OpFlag::set);
    // также, после всех манипуляций, проверяем - а не была ли новая созданная ячейка в пуле на добавление зависимостей
    UpdateFutureReferences(pos);
}

// скопировать ячейку из одной позиции в другую
void Sheet::CopyCell(Position from, Position to) {
    
    // проверяем что исходная ячейка существует
    if (!IsValid(from)) {
        // если метод вернул false - то неоткуда копировать
        throw SheetError("ERROR::CopyCell()::POS from is not Valid::" + std::to_string(__LINE__));
    }

    // внутренний метод IsValid() возвращает true, если ячейка существует, false, если нет
    // и пробразывает исключение о выходе за пределы при out of limmit
    if (!IsValid(to)) {
        // если же такой ячейки еще не было, то просто создаём новую
        _data[to] = std::make_unique<Cell>(*this, to);
    }

    // копируем данные из одной в другую методом ячейки
    _data.at(to)->Copy(*GetDirectCell(from));
    // после имплементации, если всё окей, обновляем печатный размер
    _ps_flag = PrintSizeManager(to, OpFlag::set);
}
// переместить ячейку из одной позиции в другую
void Sheet::MoveCell(Position from, Position to) {

    // внутренний метод IsValid() возвращает true, если ячейка существует, false, если нет
    // и пробразывает исключение о выходе за пределы при out of limmit
    if (!IsValid(to)) {
        // если же такой ячейки еще не было, то просто создаём новую
        _data[to] = std::make_unique<Cell>(*this, to);
    }

    // копируем данные из одной в другую методом ячейки
    _data.at(to)->Move(*GetDirectCell(from));
    // после имплементации, если всё окей, обновляем печатный размер
    _ps_flag = PrintSizeManager(to, OpFlag::set);
}

// выдаёт ячейку по позиции
const CellInterface* Sheet::GetCell(Position pos) const {
    /* 
        По условию присутствует теста 
        void TestCellReferences() - main.cpp::305

        auto sheet = CreateSheet();
        sheet->SetCell("A1"_pos, "1");
        sheet->SetCell("A2"_pos, "=A1");
        sheet->SetCell("B2"_pos, "=A1");

        ...

        sheet->SetCell("B2"_pos, "=B1");
        ASSERT(sheet->GetCell("B1"_pos)->GetReferencedCells().empty());
        ASSERT_EQUAL(sheet->GetCell("B2"_pos)->GetReferencedCells(), std::vector{ "B1"_pos });

        Мы создаём ячейку B2, которая ссылается на ещё не существующую ячейку B1, после чего у неё вызывается метод GetReferencedCells().
        Данная реализация не позволяет это сделать, так как ячейки физически еще нет НО, упоминание о неё есть в FutureReferences _future_refs.
        Таким образом поступим "хитро". В случае подобной ситуации можно будет вернуть "виртуальную" еще не существующую пустую ячейку _DUMMY
    */

    if (IsValid(pos)) {
        // если позиция валидна - просто возвращаем ее
        return _data.at(pos).get();
    }

    else if (IsFutureDependendCell(pos)) {
        // если позиция не валидна, но ожидаема - возвращаем загрушку
        return const_cast<Sheet*>(this)->GetDummy(pos);
    }

    else {
        // иначе возвращаем nullptr
        return nullptr;
    }
}

// выдаёт ячейку по позиции
CellInterface* Sheet::GetCell(Position pos) {

    if (IsValid(pos)) {
        // если позиция валидна - просто возвращаем ее
        return _data.at(pos).get();
    }
    else if (IsFutureDependendCell(pos)) {
        // если позиция не валидна, но ожидаема - возвращаем загрушку
        return const_cast<CellInterface*>(const_cast<Sheet*>(this)->GetDummy(pos));
    }
    else {
        // иначе возвращаем nullptr
        return nullptr;
    }
}

// выдаёт ячейку по позиции
const Cell* Sheet::GetDirectCell(Position pos) const {
    return IsValid(pos) ? _data.at(pos).get() : nullptr;
}
// выдаёт ячейку по позиции
Cell* Sheet::GetDirectCell(Position pos) {
    return IsValid(pos) ? _data.at(pos).get() : nullptr;
}

// удаляет ячейку по позиции
void Sheet::ClearCell(Position pos) {
    if (IsValid(pos)) {
        // если ячейка существует очищаем ее данные
        _data.at(pos).release()->~Cell();
        // удаляем позицию из массива данных, в целях экономии памяти
        _data.erase(pos);
        // обновляем флаг печатной области
        PrintSizeManager(pos, OpFlag::clear);
    }
}

// удаляет данные таблицы
Sheet& Sheet::EraseSheet() {
    _data.erase(_data.begin(), _data.end());
    return *this;
}

// выдает размер печатной области
Size Sheet::GetPrintableSize() const {
    // если флаг указывает на неактуальность данных 
    if (_ps_flag == PSizeFlag::not_actual) {
        // снимаем константность и делаем пересчёт
        const_cast<Sheet*>(this)->PrintSizeCalculate();
    }

    // возвращаем значение только при актуальном значении размера зоны печати
    return _ps_flag == PSizeFlag::actual ? _print :
        // иначе бросаем исключение, что что-то пошло не так
        throw SheetError("ERROR::GetPrintableSize()::Print size calculation is incorrect::" + std::to_string(__LINE__));
}
// вывод печатной области по значениям
void Sheet::PrintValues(std::ostream& output) const {

    // берем величину зоны печати, метод сам определит актуальна она или нет
    Size print = GetPrintableSize();

    // бежим по массиву передавая в печать строки
    for (int i = 0; i != print.rows; ++i) 
    {
        bool IsFirst = true; // флаг первой ячейки в строке
        for (int j = 0; j != print.cols; ++j)
        {
            Position pos(i, j);
            if (IsFirst) 
            {
                if (IsValid(pos)) {
                    // если позиция валидна, то печатаем
                    _data.at(pos)->PrintValue(output);
                }
                IsFirst = false;
            }
            else 
            {
                output << '\t';
                if (IsValid(pos)) {
                    // если позиция валидна, то печатаем
                    _data.at(pos)->PrintValue(output);
                }
            }
        }
        output << '\n'; // на конец каждой строки добавляем перенос
    }
}
// вывод печатной области по текстовому представлению
void Sheet::PrintTexts(std::ostream& output) const {

    // берем величину зоны печати, метод сам определит актуальна она или нет
    Size print = GetPrintableSize();

    // бежим по массиву передавая в печать строки
    for (int i = 0; i != print.rows; ++i)
    {
        bool IsFirst = true; // флаг первой ячейки в строке
        for (int j = 0; j != print.cols; ++j)
        {
            Position pos(i, j);
            if (IsFirst)
            {
                if (IsValid(pos)) {
                    // если позиция валидна, то печатаем
                    _data.at(pos)->PrintText(output);
                }
                IsFirst = false;
            }
            else
            {
                output << '\t';
                if (IsValid(pos)) {
                    // если позиция валидна, то печатаем
                    _data.at(pos)->PrintText(output);
                }
            }
        }
        output << '\n'; // на конец каждой строки добавляем перенос
    }
}

// свапает таблицы местами по ссылке
Sheet& Sheet::SwapSheet(Sheet& other) {

    // Copy and Swap
    Sheet tmp(*this);                    // делаем копию текущей таблицы
    *this = other;                       // присваиваем данные из второй таблицы
    other = std::move(tmp);       // перемещаем данные из временной таблицы

    return *this;
}
// свапает таблицы местами по указателю
Sheet& Sheet::SwapSheet(Sheet* other) {
    SwapSheet(*other);
    return *this;
}

// возвращает флаг того, что от данной ячейки завият
bool Sheet::IsFutureDependendCell(Position pos) const {
    return _future_refs.count(pos);
}

// возвращает флаг пустоты пула отложенных ссылок
bool Sheet::IsFutureRefsActual() const {
    return !static_cast<bool>(_future_refs.size());
}

// добавить направление ссылки в пул
void Sheet::AddFutureRefLine(Position from, Position to) {
    _future_refs[from].insert(to);
}

// провести обновление ссылок
void Sheet::UpdateFutureReferences() {
    // чтобы потом удалить обновленные
    FutureReferences new_pole;
    // начинаем перебирать пул
    for (const auto& line : _future_refs) {
        // если появилась ячейка от которой зависят другие
        if (IsValid(line.first)) {
            // начинаем перебирать пул её зависимых
            // так как сюда попадают только ячейки которые уже существуют, то пересохранять ничего не надо
            for (const auto& pos : line.second) {
                // говорим ячейке, что от неё зависит ячейка базового цикла
                GetDirectCell(line.first)->AddDependentCell(pos);
            }
        }
        // если условие не проходит, то откладываем на будущее
        else {
            new_pole[line.first] = line.second;
        }
    }
    // в конце просто свапаем местами старый и новый пулы
    std::swap(_future_refs, new_pole);
}
// провести обновление отложенных ссылок по позиции
void Sheet::UpdateFutureReferences(Position pos) {
    // работаем только в том случае, если ячейка есть в листе на будушее обновление
    if (IsFutureDependendCell(pos)) {
        for (const auto& cell : _future_refs.at(pos)) {
            // говорим ячейке, что от неё зависит другая ячейка
            GetDirectCell(pos)->AddDependentCell(cell);
            // очищаем кеш ячейки после того, как у неё появился наконец сюзерен
            GetDirectCell(cell)->ClearCache();
        }
        // удаляем запись о отложенном обновлении
        _future_refs.erase(pos);
    }
}

// возвращает флаг того, что таблица пуста
bool Sheet::IsEmpty() const {
    return _data.empty();
}

// флаг существующей доступной ячейки
bool Sheet::IsValid(Position pos) const {
    if (!pos.IsValid()) {
        // если позиция в принципе не валидна то кидаем исключение
        throw InvalidPositionException("incoming POS is not Valid::" + std::to_string(__LINE__));
    }
    return _data.count(pos);
}

// флаг равенство таблиц по значениям
bool Sheet::IsEqual(const Sheet& other) const {
    // если обе таблицы пустые - они равны по значениям
    if (IsEmpty() && other.IsEmpty()) {
        return true;
    }
    // если обе таблицы НЕ пустые
    else if (!IsEmpty() && !other.IsEmpty()) {

        // в случае равенства областей - отправляем в цикл сравнения
        if (GetPrintableSize() == other.GetPrintableSize()) {
            return SheetСomparison(other);
        }
        else {
            return false;
        }
    }
    // если одна строка сырая, а другая нет - не равны
    else {
        return false;
    }
}
// флаг равенство таблиц по значениям
bool Sheet::IsEqual(const Sheet* other) const {
    return other == nullptr ? false : IsEqual(*other);
}
// флаг равенство таблиц по значениям
bool Sheet::IsEqual(const SheetInterface* other) const {
    // вынужденно снимаем константность, данные не пострадают
    if (dynamic_cast<Sheet*>(const_cast<SheetInterface*>(other))) {
        return IsEqual(dynamic_cast<Sheet*>(const_cast<SheetInterface*>(other)));
    }

    return false;
}

// базовый итератор доступа begin()
auto Sheet::Begin() {
    return _data.begin();
}
// базовый итератор доступа end()
auto Sheet::End() {
    return _data.end();
}
// константный итератор доступа begin()
auto Sheet::Begin() const {
    return _data.begin();
}
// константный итератор доступа begin()
auto Sheet::End() const {
    return _data.end();
}
// константный итератор доступа cbegin()
auto Sheet::cBegin() const {
    return _data.cbegin();
}
// константный итератор доступа cend()
auto Sheet::cEnd() const {
    return _data.cend();
}

// возвращает виртуальную загрушку
const CellInterface* Sheet::GetDummy(Position pos) {
    if (!_DUMMY.get()) {
        _DUMMY = std::make_unique<Cell>(*this, pos);
        _DUMMY->SetData("");
    }
    return _DUMMY.get();
}

// калькулятор области печати
Sheet::PSizeFlag Sheet::PrintSizeCalculate() {
    if (IsEmpty() && _print.cols == -1 && _print.rows == -1) {
        // если таблица пуста 
        return _ps_flag = PSizeFlag::actual;
    }

    for (const auto& cell : _data) {

        if (cell.second) {
            if (_print.cols <= cell.first.col) {
                _print.cols = cell.first.col + 1;
            }

            if (_print.rows <= cell.first.row) {
                _print.rows = cell.first.row + 1;
            }
        }
    }
    return _ps_flag = PSizeFlag::actual;
}

// управление печатной областью
Sheet::PSizeFlag Sheet::PrintSizeManager(Position pos, OpFlag flag) {
    switch (flag)
    {
    case Sheet::set:
        
        // если координаты позиции превышают уже записанные
        if (_print.rows <= pos.row || _print.cols <= pos.col) {

            // проверяем строку
            if (_print.rows <= pos.row) {
                _print.rows = pos.row + 1;
            }
            // проверяем столбец
            if (_print.cols <= pos.col) {
                _print.cols = pos.col + 1;
            }
        }

        return _ps_flag = PSizeFlag::actual;

    case Sheet::clear:
        // если удаляемая ячейка находится внутри зоны
        if (_print.rows > pos.row + 1 && _print.cols > pos.col + 1) {
            return _ps_flag = PSizeFlag::actual;
        }
        else {
            // сбрасываем в нули
            _print.cols = 0; _print.rows = 0;
            return _ps_flag = PSizeFlag::not_actual;
        }

    default:
        return _ps_flag;
    }
}

// прямое сравнение таблиц по ячейкам
bool Sheet::SheetСomparison(const Sheet& other) const {

    // перебираем все значения в базе 
    for (const auto& cell : _data) {

        if (other.GetDirectCell(cell.first) == nullptr) {
            // если такая позиция не встречается, то выходим с false
            return false;
        }

        if (cell.second->IsEqual(other.GetDirectCell(cell.first))) {
            // если позиции не равны по значениям, то выходим с false
            return false;
        }
    }
    return true;
}

// ----------------------------------- class Sheet END ---------------------------------------------------

bool operator==(const Sheet& lhs, const Sheet& rhs) {
    // проверяем равенство также по памяти и занимаемому размеру
    return &lhs == &rhs && sizeof(lhs) == sizeof(rhs);
}
bool operator!=(const Sheet& lhs, const Sheet& rhs) {
    return !(lhs == rhs);
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::move(std::make_unique<Sheet>());
}