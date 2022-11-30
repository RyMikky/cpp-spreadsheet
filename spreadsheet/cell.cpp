#include "cell.h"
#include "sheet.h"

Cell::~Cell() {
	// сначала запускаем очистку данных
	// чтобы обновились хеши если надо
	Clear();
}

// конструктор от ссылки на таблицу
Cell::Cell(Sheet& sheet, Position pos) 
	: _sheet(sheet), _pos(pos) {
}

// задать новое содержимое ячейки
void Cell::SetData(std::string text) {

	// перезапись осуществляется если только не передана точно такая же строка
	if (_string_data != text)
	{
		if (text.empty()) {
			// для пустой строки создаём пустую имплементацию
			_impl = std::make_unique<EmptyImpl>(text);
		}

		else {
			// если строка не начинается с формульного знака
			if (text[0] != FORMULA_SIGN) {
				// создаём тесктовую имплементацию
				_impl = std::make_unique<TextImpl>(text);
			}
			else
			{
				// создаём новую формульную имплементацию
				std::unique_ptr<FormulaImpl> new_implementation =
					std::make_unique<FormulaImpl>(_sheet, text.substr(1, text.size()));

				// если формула имеет зависимости
				if (new_implementation->HasDepends()) {
					std::vector<Position> depends_on = new_implementation->GetReferencedCells();

					// запускаем проверку на образование циклической зависимости
					// проверка выкинет исключение если будет найдена такая зависимость
					// таким образом данные в ячейке не постарадают, так как метод прекратит выполнение
					ReferenceManager(RManagerFlag::cyclic_check, depends_on);
					// после имплементации и проверки запускаем менеджер зависимостей на обновление пула ссылок
					ReferenceManager(RManagerFlag::update_roots, depends_on);
					// в случае успешных проверок, чтобы не терять массив кидаем его в список зависимости ячейки
					AddDependsOn(depends_on);
				}

				// просле проверок смотрим была ли ячейка уже активированна чем-то
				// если ячейка создаётся впервые, то скорее всего на неё еще нет ссылок
				if (!IsRaw() && !IsEmpty()) {
					Clear();     // операция очистки ячейки позаботится о инвалидации кеша
				}

				// после всех проверок и обновлений загружаем новую имплементацию
				_impl = std::move(new_implementation);
				// записываем полученную строку
				_string_data = text;
			}
		}
	}
}

// задать позицию ячейки
void Cell::SetPosition(Position pos) {
	_pos.col = pos.col;
	_pos.row = pos.row;
}

// скопировать содержимое другой ячейки
void Cell::Copy(const Cell& other) {
	// првоеряем на самоприсваивание по адресам в памяти и по значениям
	if (*this != other && !IsEqual(other)) {

		if (static_cast<bool>(_impl)) {
			// очищаем старые данные если они есть
			Clear();  // очистка автоматически инвалидирует кеш если от данной ячейки зависят
		}

		_string_data = other._string_data;
		// делаем перерасчёт данных при копировании содержимого ячейки
		SetData(_string_data);
	}
}
// переместить содержимое из другой ячейки
void Cell::Move(Cell& other) {
	// првоеряем на самоприсваивание по адресам в памяти и по значениям
	if (*this != other && !IsEqual(other)) {

		// для начала инвалидируем кеши обоих ячеек
		ClearCache(); other.ClearCache();

		// переносим базовую строку
		_string_data = std::move(other._string_data);
		// указатель на данные обрабатываем через release/reset
		_impl.reset(other._impl.release());
	}
}
// обменять содержимое ячеек
void Cell::Swap(Cell& other) {
	// првоеряем на самоприсваивание по адресам в памяти и по значениям
	if (*this != other && !IsEqual(other)) {
		try
		{
			Cell temp(_sheet, Position::NONE); // делаем временную копию ячейки
			this->Copy(other);                 // записываем данные из другой ячейки
			other.Move(temp);           // перезаписываем данные другой ячейки
		}
		catch (const std::exception&)
		{
			throw CellException("ERROR::Swap()::" + std::to_string(__LINE__));
		}
	}
}

// очистить ранее посчитаный кеш формулы
void Cell::ClearCache() {
	if (IsFormula()) {
		// удаляем кеши через менеджер со спец-флагом
		ReferenceManager(RManagerFlag::clear_cache, _dependent);
	}
}

// удалить содержимое ячейки
void Cell::Clear() {
	// если ячейка формульная и от неё зависят другие ячейки
	if (IsFormula() && IsRoot()) {
		// необходимо инвалидировать кеши зависимых
		ReferenceManager(RManagerFlag::clear_cache, _dependent);
	}

	if (_impl) _impl.release()->~Impl();       // полностью удаляет содержимое
	_string_data = "";                         // удаляем входящую строку
}

// получить расчётное значение ячейки
Cell::Value Cell::GetValue() const {
	if (!IsRaw()) {
		// только не сырая ячейка может дать какое-то значение
		return _impl->GetValue();
	}
	else {
		return 0.0;
	}
}
// получить текстовое представление ячейки
std::string Cell::GetText() const {
	if (!IsRaw()) {
		// только не сырая ячейка может вернуть текст
		return _impl->GetString();
	}
	else {
		return "";
	}
}

// получить содержимое пула зависимостей формулы
std::vector<Position> Cell::GetReferencedCells() const {
	if (IsFormula()) {
		// только формульная ячейка может содержать ссылки
		return AsFormula()->GetReferencedCells();
	}
	else {
		return {};
	}
}
// получить базовую строку ячйеки
const std::string& Cell::GetTextData() const {
	return _string_data;
}

// добавить зависимую ячейку
void Cell::AddDependentCell(Position pos) {
	// проверяем есть ли такая ячейка в векторе
	if (!IsDependentCell(pos)) {
		_dependent.push_back(pos);
	}
}
// добавить вектор зависимых ячейк
void Cell::AddDependentCell(const std::vector<Position>& dependent) {
	// копируем новый лист зависимых
	_dependent = dependent;
}
// добавить ячейку от которой зависит текущая
void Cell::AddDependsOn(Position pos) {
	// проверяем есть ли такая ячейка в векторе
	if (!IsDependsFromCell(pos)) {
		_depends_on.push_back(pos);
		// не вызываем менеджер для единичного случая
		// "сообщаем" ячейке, что у неё появилась зависимая подруга
		_sheet.GetDirectCell(pos)->AddDependentCell(_pos);
	}
}
// добавить вектор ячеек от которой зависит текущая
void Cell::AddDependsOn(const std::vector<Position>& dependent) {
	// копируем новый лист зависимостей от
	_depends_on = dependent;
	// запускаем менеджер на обновление ссылок
	ReferenceManager(RManagerFlag::update_roots, _depends_on);
}

// подтверждает что позиция является зависимой от текущей
bool Cell::IsDependentCell(Position pos) const {
	return std::find(/*std::execution::par,*/_dependent.begin(), _dependent.end(), pos) != _dependent.end() ? true : false;
}
// подтверждает что данная ячейка зависит от позиции
bool Cell::IsDependsFromCell(Position pos) const {
	return std::find(/*std::execution::par,*/_depends_on.begin(), _depends_on.end(), pos) != _depends_on.end() ? true : false;
}
// проверка на циклическую зависимость
bool Cell::CyclicRecurceCheck(Position pos) const {
	
	if (_depends_on.empty()) {
		// если список зависимостей пуст, то дальше и искать не надо
		return false;
	}

	if (IsDependsFromCell(pos)) {
		// если в списке зависимостей есть переданная позиция, то сразу говорим - да, мы уже от неё зависим!
		return true;
	}
	else {
		for (const auto& cell : _depends_on) {
			// запускаем дальнейшую проверку
			const Cell* test = _sheet.GetDirectCell(cell);
			if (test && test->CyclicRecurceCheck(pos)) {
				// если нашли таки попадание, то выходим
				return true;
			}
			else {
				// иначе продолжаем до талого, когда наконец не появится фолс
				continue;
			}
		}

		return false;
	}
}
// возвращает вектор ячеек зависимых от текущей 
std::vector<Position> Cell::GetDependent() const{
	return _dependent;
}
// возвращает вектор ячеек, от которых зависит текущая
std::vector<Position> Cell::GetDependsOn() const{
	return _depends_on;
}

// печать GetValue в поток
void Cell::PrintValue(std::ostream& out) {
	// если строка текстовая, то выведется текстовое предаставление 
	if (IsText()) {
		out << std::get<std::string>(GetValue());
	}
	else {
		// если формула то будем смотреть что там выводится
		if (IsFormula() && std::holds_alternative<double>(GetValue())) {
			out << std::get<double>(GetValue());
		}
		else {
			out << std::get<FormulaError>(GetValue());
		}
	}
}
// печать GetText в поток
void Cell::PrintText(std::ostream& out) {
	if (!IsEmpty() && !IsRaw()) {
		out << GetText();
	}
}

// возвращает флаг незаполненной ячейки
bool Cell::IsEmpty() const {
	// если лежит именно EmptyImpl
	return !IsRaw() && dynamic_cast<EmptyImpl*>(_impl.get()) ? true : false;
}
// возвращает флаг текстовой ячейки
bool Cell::IsText() const {
	return !IsRaw() && dynamic_cast<TextImpl*>(_impl.get()) ? true : false;
}
// возвращает флаг ячейки с формульным выражением
bool Cell::IsFormula() const {
	return !IsRaw() && dynamic_cast<FormulaImpl*>(_impl.get()) ? true : false;
}
// возвращает флаг того, что ячейка является ссылкой
bool Cell::IsReference() const {
	return !_depends_on.empty();
}
// возвращает флаг того, что на данную ячейку ссылаются
bool Cell::IsRoot() const {
	return !_dependent.empty();
}
// возвращает флаг пустой ячейки
bool Cell::IsRaw() const {
	return !(_impl);
}
// флаг равенство ячеек по значениям по ссылке
bool Cell::IsEqual(const Cell& other) const {

	// обе ячейки "сырые" - равны по значению
	if (IsRaw() && other.IsRaw()) {
		return true;
	}
	// обе ячейки НЕ "сырые" - сравниваем по значению и текстовому представлению
	else if (!IsRaw() && !other.IsRaw()) {
		return _impl->GetString() == other._impl->GetString();
	}
	else {
		return false;
	}
}
// флаг равенство ячеек по значениям по указателю
bool Cell::IsEqual(const Cell* other) const {
	return other == nullptr ? false : IsEqual(*other);
}
// флаг равенство ячеек по значениям по указателю на родителя
bool Cell::IsEqual(const CellInterface* other) const {
	// пользуемся "грязными методами" намеренно
	// необходимо проверить - на сравнение передана ячейка или что?
	// в случае успеха нарушения данных не будет так как сравниваться будет копия
	if (dynamic_cast<Cell*>(const_cast<CellInterface*>(other))) {
		return IsEqual(dynamic_cast<Cell*>(const_cast<CellInterface*>(other)));
	}
	return false;
}

// читает ячейку как текст
TextImpl* Cell::AsText() const {
	if (this->IsText()) {
		return dynamic_cast<TextImpl*>(_impl.get());
	}
	else {
		throw CellException("ERROR::AsText()::" + std::to_string(__LINE__));
	}
}

// читает ячейку как формулу
FormulaImpl* Cell::AsFormula() const {
	if (this->IsFormula()) {
		return dynamic_cast<FormulaImpl*>(_impl.get());
	}
	else {
		throw CellException("ERROR::AsFormula()::" + std::to_string(__LINE__));
	}
}

// получение результата работы формулы
Cell::Value Cell::GetFormulaEvaluate() const {
	return AsFormula()->GetValue();
}

// менеджер обработки ссылок
void Cell::ReferenceManager(RManagerFlag flag, const std::vector<Position>&refs) {

	switch (flag)
	{
		// работа во время имплементации или просто обновления
	case Cell::update_roots:
		// идём по списку и добавляем джанные в каждую ячейку
		std::for_each(/*std::execution::par,*/refs.begin(), refs.end(), [this](const Position& pos) {
			// если искомая ячейка, от которой зависит текущая существует
			if (_sheet.IsValid(pos)) {
				// добавляем зависимость напрямую по полученному указателю ячейки, через соответствующий метод класса
				_sheet.GetDirectCell(pos)->AddDependentCell(_pos);
			}
		// но может случиться так, что еще нет той ячейки от которой зависит текущая
			else {
				// необходимо сказать таблице - "я тут это, завишу вон от той, ты потом ей скажи, когда она появится, позязя"
				_sheet.AddFutureRefLine(pos, _pos);
			}
			});
		break;

		// очистка кеша по всей линии зависимых ссылок
	case Cell::clear_cache:
		// удаление кеша приведет к инвалидации кеша, зависимых ячеек
		// таким образом необходимо сначала очистить кеши зависимых и всем цепочкам их зависимостей
		std::for_each(/*std::execution::par,*/_dependent.begin(), _dependent.end(), [this](const Position& pos) {
			// сначала точно также запускаем рекурсивное удаление
			Cell* cell = _sheet.GetDirectCell(pos);
			if (cell) cell->ClearCache();

			});
		// удаляем текущий кеш
		AsFormula()->ClearCache();
		break;

		// проверка на образование циклической звисимости сразу выкинет исключение если что-то найдёт
	case Cell::cyclic_check:
		// сейчас будет масло масленное - необходимо проверить, не входит ли
		// данная ячейка в списки зависимостей ячеек, от которых зависит она сама

		// начинаем искать совпадение по зависимостям
		for (const auto& item : refs) {
			if (item == _pos) {
				// если ссылаемся на себя же то выдаем ошибку
				throw CircularDependencyException("IsCyclicDependency");
			}

			// получаем ячейку на проверку
			const Cell* cell = _sheet.GetDirectCell(item);
			// проверяем совпадение по зависимостям полученной ячейки
			if (cell && cell->CyclicRecurceCheck(_pos)) {
				throw CircularDependencyException("IsCyclicDependency");
			}
		}
		break;
	default:
		break;
	}
}

bool operator==(const Cell& lhs, const Cell& rhs) {
	// ячейки сравниваются по адресу нахождения в памяти!
	return &lhs == &rhs;
}
bool operator!=(const Cell& lhs, const Cell& rhs) {
	return !(lhs == rhs);
}
