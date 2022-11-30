#include "common.h"

#include <cctype>
#include <sstream>
#include <cassert>
#include <iostream>
//#include <future>
#include <cmath>
#include <set>
#include <map>

const int LETTERS = 26;
const int MAX_POSITION_LENGTH = 17;
const int MAX_POS_LETTER_COUNT = 3;

const Position Position::NONE = {-1, -1};

using namespace std::literals;

bool Size::operator==(Size rhs) const {
	return this->cols == rhs.cols && this->rows == rhs.rows;
}

// ---------------------------------------- class FormulaError --------------------------------------------

// конструктор от категории ошибки
FormulaError::FormulaError(Category category) 
	: _category(category) {
}
// вернуть категорию ошибки
FormulaError::Category FormulaError::GetCategory() const {
	return _category;
}
// проверить тип категории
bool FormulaError::operator==(FormulaError rhs) const {
	return _category == rhs._category;
}
// вернуть строковое представление ошибки
std::string_view FormulaError::ToString() const {
	switch (_category)
	{
	case FormulaError::Category::Ref:
		return "#REF!";
	case FormulaError::Category::Value:
		return "#VALUE!";
	case FormulaError::Category::Div0:
		return "#DIV/0!";
	}
	return "";
}

// ---------------------------------------- class FormulaError END ----------------------------------------

namespace detail {

	// блок статик расчётов при компиляции
	namespace staticaly {

		// базовая таблица буквенных символов для рассчёта адреса ячейки
		const static std::map<char, int8_t> __BASIC_CHARACTER_SYMBOLS_SI__ =
		{ {'A', 1}, {'B', 2}, {'C', 3}, {'D', 4}, {'E', 5}, {'F', 6}, {'G', 7}, {'H', 8}, {'I', 9}
		, {'J', 10}, {'K', 11}, {'L', 12}, {'M', 13}, {'N', 14}, {'O', 15}, {'P', 16}, {'Q', 17}
		, {'R', 18}, {'S', 19}, {'T', 20}, {'U', 21}, {'V', 22}, {'W', 23}, {'X', 24}, {'Y', 25}, {'Z', 26} };

		// базовая таблица буквенных символов для рассчёта адреса ячейки
		const static std::map<int8_t, char> __BASIC_CHARACTER_SYMBOLS_IS__ =
		{ {1, 'A'}, {2, 'B'}, {3, 'C'}, {4, 'D'}, {5, 'E'}, {6, 'F'}, {7, 'G'}, {8, 'H'}, {9, 'I'}
		, {10, 'J'}, {11, 'K'}, {12, 'L'}, {13, 'M'}, {14, 'N'}, {15, 'O'}, {16, 'P'}, {17, 'Q'}
		, {18, 'R'}, {19, 'S'}, {20, 'T'}, {21, 'U'}, {22, 'V'}, {23, 'W'}, {24, 'X'}, {25, 'Y'}, {26, 'Z'} };

		// базовая таблица цифровых символов для рассчёта адреса ячейки
		const static std::set<char> __BASIC_NUMERIC_SYMBOLS__ =
			{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };

		// максимальная величина индекса по количеству печатных символов
		const static std::map<int8_t, int16_t> __BASIC_CHARACTER_COUNT_LIMMIT__ = 
			{ 
				  {0, 0}        // для нуля символов
				, {1, 26}       // для одного символа
				, {2, 702}      // для двух символов
				, {3, 17575}    // для трех символов
			};

	} // namespace staticaly

	namespace calculate {

		/*
		    Расчёт номера столбца определяется формулами приведения в 26 ую систему счисления
			Для односимвольного адреса используется просто порядковый номер буквы в алфавите
			A = 1                                                                        (0)
			Z = 26                                                                      (25)

			Для двухсимвольного адреса используется формула
			index = 26 * {номер первого символа} 
			           + {номер второго символа}
			AA = 26*1 + 1 = 27                                                          (26)
			AB = 26*1 + 2 = 28                                                          (27)
			AZ = 26*1 + 26 = 52                                                         (51)
			BA = 26*2 + 1 = 53                                                          (52)
			CA = 26*3 + 1 = 79                                                          (78)
			DF = 26*4 + 6 = 110                                                        (109)
			ZZ = 26*26 + 26 = 702                                                      (701)

			Для трехсимвольного адреса используется формула
			index = 26 * 26 * {номер первого символа} 
			           + 26 * {номер второго символа} 
			                + {номер третьего символа}

			AAA = 26*26*1 + 26*1 + 1 = 703                                             (702)
			ABA = 26*26*1 + 26*2 + 1 = 676 + 52 + 1 = 729                              (728)
			ACA = 26*26*1 + 26*3 + 1 = 676 + 78 + 1 = 755                              (754)

			АDE = 26*26*1 + 26*4 + 5 = 676 + 104 + 5 = 785                             (784)

			BAA = 26*26*2 + 26*1 + 1 = 1352 + 26 + 1 = 1379                           (1378)
			BCA = 26*26*2 + 26*3 + 1 = 1352 + 78 + 1 = 1431                           (1430)
			BCD = 26*26*2 + 26*3 + 4 = 1434                                           (1433)

			FAA = 26*26*6 + 26*1 + 1 = 4083                                           (4082)
			FCA = 26*26*6 + 26*3 + 1 = 4135                                           (4134)
			FCC = 26*26*6 + 26*3 + 3 = 4137                                           (4136)

			Также формулу можно записать в одно дейстиве, как

				(N^i(I)) * p(I)) + (N^(i - 1)(I)) * p(I)) + ... + (N^(i = 0) * p(I))

				где:
					N - основание системы счисления (в нашем случае 26)
					i - разряд символа (считается справа на лево от нуля)
					p - номер символа согласно таблице
					I - рассматриваемый символ

		*/

		// Проверяет, что номер строки не превышает максимум и не меньше нуля
		bool IsRowInRange(int row) {
			return row >= 0 && row < Position::MAX_ROWS;
		}
		// Проверяет, что номер столбца не превышает максимум и не меньше нуля
		bool IsColInRange(int col) {
			return col >= 0 && col < Position::MAX_COLS;
		}

		// Индекс односимвольного адреса. Наивная реализация.
		int FromStringSingleSymbolCalculate(std::string_view col) {

			// берем значение напрямую из мапы
			int _result = staticaly::__BASIC_CHARACTER_SYMBOLS_SI__.at(*col.data()) - 1;

			// отправляем данные после проверки условия попадания в диапазон
			return IsColInRange(_result) ? _result
				: throw std::logic_error("COL number is out of range::LINE::" + std::to_string(__LINE__));
		}
		// Индекс двухсимвольного адреса. Наивная реализация.
		int FromStringDoubleSymbolCalculate(std::string_view col) {

			// берем из мапы номера символов
			int _first_char = staticaly::__BASIC_CHARACTER_SYMBOLS_SI__.at(*col.data());
			int _second_char = staticaly::__BASIC_CHARACTER_SYMBOLS_SI__.at(*(col.data() + 1));

			// считаем результат по описанной выше формуле
			int _result = (26 * _first_char) + _second_char - 1;

			// отправляем данные после проверки условия попадания в диапазон
			return IsColInRange(_result) ? _result 
				: throw std::logic_error("COL number is out of range::LINE::" + std::to_string(__LINE__));
		}
		// Индекс трехсимвольного адреса. Наивная реализация.
		int FromStringTripleSymbolCalculate(std::string_view col) {

			// берем из мапы номера символов
			int _first_char = staticaly::__BASIC_CHARACTER_SYMBOLS_SI__.at(*col.data());
			int _second_char = staticaly::__BASIC_CHARACTER_SYMBOLS_SI__.at(*(col.data() + 1));
			int _triple_char = staticaly::__BASIC_CHARACTER_SYMBOLS_SI__.at(*(col.data() + 2));

			// считаем результат по описанной выше формуле
			int _result = (26 * 26 * _first_char) + (26 * _second_char) + _triple_char - 1;

			// отправляем данные после проверки условия попадания в диапазон
			return IsColInRange(_result) ? _result
				: throw std::logic_error("COL number is out of range::LINE::" + std::to_string(__LINE__));
		}

		// Рекурсивный расчёт индекса. Из результата работы функции ОБЯЗАТЕЛЬНО вычитание единицы!
		// reg - количество разрядов полученного буквенного числа. Равно col.size() - 1
		int FromStringRecursiveSymbolCalculate(std::string_view col, size_t reg /*количество разрядов - 1. равно col.size() - 1*/) {

			/*
				Рекурсия выдает результат на единицу больше! Так как оперирует числами из константной мапы!
				С нуля стартовать нелья, так как тогда символ 'А' будет обрабатываться некорректно!
				После работы рекурсии требуется вычитание единицы из полученного результата!
			*/

			// выходим их рекурсии когда разрядов не осталось вообще
			if (!reg) {
				// возвращаем номер символа из базы
				return staticaly::__BASIC_CHARACTER_SYMBOLS_SI__.at(*col.data());
			} 

			else {
				// рекурентно прибавляем число с сокращением строки и понижением разряда
				int result = (static_cast<int>(std::pow(26, reg))
					* staticaly::__BASIC_CHARACTER_SYMBOLS_SI__.at(*col.data()))
					+ FromStringRecursiveSymbolCalculate(col.substr(1, col.size()), reg - 1);

				// перед возвратом проверям попадание в границы. Важно помнить, что рекурсия считает на единицу больше
				return detail::calculate::IsColInRange(result - 1) ? result :
					throw std::logic_error("COL number is out of range::LINE::" + std::to_string(__LINE__));
			}
		}

		// Преобразование номера в буквенный код. Наивная реализация.
		// Не проходит все личные ассерты, но проходит по условию задачи и тест-системы
		std::string ToStringNumericCast(int col) {

			if (!IsColInRange(col)) return "";

			std::string result = "";


			if ((col + 1) / LETTERS == 0) {
				return result += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at(col + 1);
			}
			else if (((col + 1) / LETTERS == 1) && ((col + 1) % LETTERS == 0)) {
				return result += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at(col + 1);
			}
			else if ((col + 1) / LETTERS <= LETTERS) {

				int a = 0;
				int b = 0;

				if (((col + 1) % LETTERS) == 0) {
					a = ((col + 1) / LETTERS) - 1;
					b = 26;
				}
				else {
					a = ((col + 1) / LETTERS);
					b = ((col + 1) % LETTERS);
				}

				result += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at(a);
				result += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at(b);
				return result;
			}
			else {
				if ((((double)col + static_cast<double>(1)) / (double)LETTERS) == 27.0) {
					return result = "ZZ"; /*701*/
				}
				else {

					int a = 0;
					int b = 0;
					int c = 0;

					if (((col + 1) % LETTERS) == 0) {
						a = ((col + 1) / LETTERS) / LETTERS;
						b = (((col + 1) / LETTERS) % LETTERS) - 1;
						c = 26;

					} else{
						a = ((col + 1) / LETTERS) / LETTERS;
						b = ((col + 1) / LETTERS) % LETTERS;
						c = ((col + 1) % LETTERS);
					}
					
					result += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at(a);
					result += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at(b);
					result += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at(c);

					return result;
				}
			}
		}

		// Рекусривное преобразование номера в буквенный код.
		// Принимает в параметре число увеличенное на единицу!
		// Работает с одно, двух и трех буквенными числами.
		// Логика описана в комментариях по ходу выполнения функции.
		std::string ToStringRecursiveNumericCast(int col) {

			/*
				Все вычисления в рекурсии кастуем к инту! Это важно!
			*/

			// если переданный номер выходит за границы диапазона, то кидаем исключение
			// так как в рекурсию передается индекс + 1, то проверяем с - 1
			if (!IsColInRange(col - 1)) 
				throw std::logic_error("COL number is out of range::LINE::" + std::to_string(__LINE__));

			// выход из рекурсии когда переданный адрес не более 26
			// обрабатывается или меньше или 26 (символ 'Z')
			if (!(static_cast<int>(col / LETTERS)) || ((col - LETTERS) == 0))
				// возвращаем результат из таблицы символов заданой системы счисления
				return std::string() += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at(col);

			// базовый остаток от деления на основание системы счисления
			int dev_rem_ = static_cast<int>(col % LETTERS);
			// флага кратности поцизии системе счисления, если false - значит на конце стоит 'Z'
			bool IsZetEnd = !(dev_rem_);

			int dev_ = 0;                // счётчик величины множителя, определяет количество разрядов буквенного кода
			int col_ = col;              // разделяемое значение используемое в вычислениях

			// делим значение до того, пока оно не станет меньше основания системы счисления
			while (col_ > LETTERS)
			{
				static_cast<int>(col_ /= LETTERS);     // делим на основание системы счисления
				++dev_;                                // плюсуем счётчик
			}

			// делитель равен единице значит символ двухбуквенный, например "ZA" или "DF"
			if (dev_ == 1) {

				// если у нас позиция кратна основанию системы счисления значит присутствует 'Z'
				if (IsZetEnd) {
					//возвращаем промежуточный результат деления минус единица
					return std::string() += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at(col_ - 1)
						// прибавляем рекурсию от позиции минус основание умноженное на промежуточное минус единица
						+ ToStringRecursiveNumericCast(col - (LETTERS * (col_ - 1)));
				}
				else {
					// возвращаем промежуточный результат деления
					return std::string() += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at(col_)
						// прибавляем рекурсию от позиции минус основание умноженное на промежуточный результат
						+ ToStringRecursiveNumericCast(col - (LETTERS * col_));
				}
			} // делитель равен единице значит символ двухбуквенный, например "ZA" или "DF"

			// делитель равен двум значит символ трехбуквенный, например "TAF" или "ADC"
			else if (dev_ == 2) {

				// вводим новую переменную - обратное возведение основания в степень делителя
				int inverse_ = static_cast<int>(std::pow(LETTERS, dev_));

				// воодим новую переменную - показатель разряда, который поможет в расчётах
				// определения значения вычитания и индекса в таблице символов системы счисления
				// базово вычисляется как произведение полученного конечного деления на
				// обратное возведение основания системы счисления в степень делителя
				int discharge_ = inverse_ * col_;

				// если у нас позиция кратна основанию системы счисления значит присутствует 'Z'
				if (IsZetEnd) {

					// особый случай - "ZZ", делитель два, но и разрядов тоже два
					// проверяется легко - разница индекса и основания в степени делителя
					// должна быть равна основанию системы счисления
					if (col - inverse_ == LETTERS) {
						// возвращаем разницу позиции и основания в степени делителя
						return std::string() += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at
							(col - inverse_)
							// прибавляем рекурсию от той же разницы
							+ ToStringRecursiveNumericCast(col - inverse_);
					}
					else {

						// показатель разряда иначе обратное возведение в степень делителя основания системы 
						// счисления и умножение на остаток разделяемого больше или равно изначальному числу
						if (discharge_ >= col) {

							// пересчитываем показатель разряда таким образом, что из базового числа вычитаем
							// уже вычисленное значение показателя плюч значение деления минус единица
							// таким образом получаем индекс символа текущей итерации рекурсии
							discharge_ = (col - discharge_) + (col_ - 1);

							// возвращаемое значение на итерации равно пересчитанному показателю разряда
							return std::string() += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at(discharge_)
								// прибавляем рекурсию от позиции минус обратное возведеие умноженное на разделенное минус единица
								+ ToStringRecursiveNumericCast(col - (inverse_ * (col_ - 1)));
						}
						else {

							// пересчитываем показатель разряда таким образом, что из базового числа вычитаем
							// уже вычисленное значение показателя и используем для определения индекса
							discharge_ = col - discharge_;                    // col - (inverse_ * col_)
							// равенство показателя разряда системе счисления, такое возможно когда у нас имеются две "ZZ" на конце
							bool equality_ = discharge_ == LETTERS;           // например "AZZ", "DZZ", "FZZ", etc.

							// возвращаемое значение на итерации зависит от равенства показателя разряда основанию системы счисления
							return std::string() += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at
							// в случае равенства индексом является результат деления минус единица, иначе просто результат деления
							(equality_ ? (col_ - 1) : col_)
								// прибавляем рекурсию от позиции минус обратное возведеие в степень 
								// умноженное на результат деления определяемое от равенства разряда основанию
								+ ToStringRecursiveNumericCast(col - (inverse_ * (equality_ ? (col_ - 1) : col_)));
						}
					}
				} // IsZetEnd

				else {

					// TODO!!!!
					// ПРОВЕРИТЬ ЭТО УСЛОВИЕ (ветка if) - ОНО ВООБЩЕ НАДО?!?!?!?!

					// показатель разряда иначе обратное возведение в степень делителя основания системы 
					// счисления и умножение на остаток разделяемого больше или равно изначальному числу
					if (discharge_ >= col) {

						// в данном варианте пересчитывать и использовать показатель разряда нет необходимости

						// возвращаемое значение на итерации равно разнице исходного числа и показателя разряда
						return std::string() += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at(col - discharge_)
							// прибавляем рекурсию от позиции минус обратное возведение в степень
							+ ToStringRecursiveNumericCast(col - inverse_);
					}
					else {

						// пересчитываем показатель разряда таким образом, что из базового числа вычитаем
						// уже вычисленное значение показателя и используем для определения индекса
						discharge_ = col - discharge_;                    // col - (inverse_ * col_)

						// превыщение показателя разряда системы счисления, такое возможно когда у нас имеются "Z" в середине
						bool greater_ = discharge_ > LETTERS;             // например "AZA", "DZC", "FZY", etc.

						// возвращаемое значение на итерации зависит от превышения показателя разряда основания системы счисления
						return std::string() += staticaly::__BASIC_CHARACTER_SYMBOLS_IS__.at
						// в случае равенства индексом является результат деления, иначе результат деления минус единица
						(greater_ ? col_ : col_ - 1)
							// прибавляем рекурсию от позиции минус обратное возведеие в степень 
							// умноженное на результат деления определяемое от первышения разрядом основания
							+ ToStringRecursiveNumericCast(col - (inverse_ * (greater_ ? col_ : col_ - 1)));
					}
				}
			} // делитель равен двум значит символ трехбуквенный, например "TAF" или "ADC"

			// делитель больше трех - символ четырехбуквенный - вышли за пределы
			else {
				throw std::logic_error("COL number is out of range::LINE::" + std::to_string(__LINE__));
			}
		}

	} // namespace calculate
	
	// Парсер номера строки, выдает число или исключение при некорректном номере
	int PositionRowParse(std::string_view row) {

		// предварительно парсим строку, чтобы убедиться, что все символы - числа
		// так как std::stoi легко обработает например "134DV-=-/AD" - будет число 134.
		for (char c : row) {
			if (staticaly::__BASIC_NUMERIC_SYMBOLS__.count(c)) {
				continue;
			}
			else {
				// бросаем исключение при некорректности символов
				throw std::logic_error("Uncorrect ROW number::LINE::" + std::to_string(__LINE__));
			}
		}

		// пробуем преобразовать, если будет исключение поймаем его в анализаторе
		int r = std::stoi(std::string(row)) - 1;
		// возвращаем число если входит в диапазон и бросаем исключение в противном случае
		return calculate::IsRowInRange(r) ? r 
			: throw std::logic_error("ROW number is out of range::LINE::" + std::to_string(__LINE__));
	}

	// Парсер номера столбца, выдает число или исключение при некорректном номере
	int PositionColParse(std::string_view col) {
		// получаем цифровой диапазон в котором будет находиться индекс

		if (0 < col.size() && col.size() <= 3) {
			return calculate::FromStringRecursiveSymbolCalculate(col, col.size() - 1) - 1;
		}
		else {
			throw std::logic_error("COL symbols count is out of range::LINE::" + std::to_string(__LINE__));
		}

		// наивная реализация
		/*switch (col.size())
		{
		case 0:
			throw std::logic_error("COL symbols count is out of range::LINE::" + std::to_string(__LINE__));
		case 1:
			return calculate::FromStringSingleSymbolCalculate(col);
		case 2:
			return calculate::FromStringDoubleSymbolCalculate(col);
		case 3:
			return calculate::FromStringTripleSymbolCalculate(col);
		default:
			throw std::logic_error("COL symbols count is out of range::LINE::" + std::to_string(__LINE__));
		}*/
	}

	// Получение позиции по переданной строке
	Position PositionAnalizer(std::string_view col, std::string_view row) {

		try
		{
			/*
				НОРМАЛЬНАЯ ЧЕЛОВЕЧЕСКАЯ ВЕРСИЯ С АСИНХОМ
				ОТКЛЮЧЕНО ДЛЯ ПРОВЕРКИ ТРЕНАЖЕРА

				// так как расчёт позиции в 26-ой системе счисления будет явно дольше
				// чем приведение не очень длинного номера через std::stoi, то
				// производим расчёт номера столбца параллельно
				std::future<int> _col_ret = std::async(&PositionColParse, col);

				// позицию строки считаем в базовом потоке
				// если будет брошено исключение, то его обработает catch-блок
				int _row = PositionRowParse(row);

				// получаем данные по номеру столбца
				int _col = _col_ret.get();
			*/

			// так как расчёт позиции в 26-ой системе счисления будет явно дольше
			// чем приведение не очень длинного номера через std::stoi, то
			// производим расчёт номера столбца параллельно
			//std::future<int> _col_ret = std::async(&PositionColParse, col);

			// позицию строки считаем в базовом потоке
			// если будет брошено исключение, то его обработает catch-блок
			int _row = PositionRowParse(row);
			
			// получаем данные по номеру столбца
			int _col = PositionColParse(col);
			
			// возвращаем результат
			return { _row , _col };
		}
		catch (const std::exception& /*e*/)
		{
			// в случае любого пойманного исключение возвращаем пустую строку
			//std::cerr << "FILE::structures.cpp::LINE::"sv << __LINE__ << std::endl;
			//std::cerr << "FUNC::detail::PositionAnalizer()"sv << std::endl;
			//std::cerr << "EXCP::"sv << e.what() << std::endl;
			return Position::NONE;
		}

		return Position::NONE;
	}

	// Базовый парсер строки
	Position FromStringParse(std::string_view str) {

		// Функция разделяет на буквенный и цифровой адресс.
		// Тут же проверяется условие длины символьного адреса.
		
		int8_t _pos_counter = 0;          // позиция в строке, с которой начинаются цифры
		std::string_view _symbolic;       // символьный адресс колонки
		std::string_view _numeric;        // цифровой адресс строки

		for (char c : str)                // перебираем строку
		{
			// если символ является буквенным и позиция меньше максимума
			if (staticaly::__BASIC_CHARACTER_SYMBOLS_SI__.count(c) 
				&& _pos_counter < MAX_POS_LETTER_COUNT) {
				_pos_counter++;
			}

			// если символ является буквенным и позиция вышла за пределы максимума
			else if (staticaly::__BASIC_CHARACTER_SYMBOLS_SI__.count(c)
				&& _pos_counter >= MAX_POS_LETTER_COUNT) {
				return Position::NONE;    // сразу говорим о ошибке
			}

			// если символ является цифровым и позиция НЕ нулевая
			else if (staticaly::__BASIC_NUMERIC_SYMBOLS__.count(c) && _pos_counter) {
				// разбираем строку на подстроки
				_symbolic = str.substr(0, _pos_counter);
				_numeric = str.substr(_pos_counter, str.size());
				// передаем в анализатор
				return PositionAnalizer(_symbolic, _numeric);
			}

			// в противном случае уходим с ошибкой
			else {
				return Position::NONE;
			}
		}

		// если строка вообще пуста, то сразу уходим
		return Position::NONE;
	}

	namespace tests {

		// Проверка корректности вычисления индекса столбца из строки
		void FromStringColAddressCalculate() {

			try {
				// односимвольные
				assert(PositionColParse("A") == 0);
				assert(PositionColParse("Z") == 25);
				// двухсимвольные
				assert(PositionColParse("AA") == 26);
				assert(PositionColParse("AB") == 27);
				assert(PositionColParse("AZ") == 51);
				assert(PositionColParse("BA") == 52);
				assert(PositionColParse("CA") == 78);
				assert(PositionColParse("DF") == 109);
				assert(PositionColParse("ZZ") == 701);
				// трехсимвольные
				assert(PositionColParse("AAA") == 702);
				assert(PositionColParse("ABA") == 728);
				assert(PositionColParse("ACA") == 754);
				assert(PositionColParse("ADE") == 784);
				assert(PositionColParse("BAA") == 1378);
				assert(PositionColParse("BCA") == 1430);
				assert(PositionColParse("BCD") == 1433);
				assert(PositionColParse("FAA") == 4082);
				assert(PositionColParse("FCA") == 4134);
				assert(PositionColParse("FCC") == 4136);
				assert(PositionColParse("XFD") == 16383);
			}
			catch (const std::exception&){
				assert(false);
			}
			
			try {
				// пустая строка
				PositionColParse("");
				assert(false);
			}
			catch (const std::exception&){
				// мы должны оказаться тут
				assert(true);
			}

			try {
				// индекс явно выходит за пределы лимита
				PositionColParse("ZZZ");
				assert(false);
			}
			catch (const std::exception&){
				// мы должны оказаться тут
				assert(true);
			}

			try {
				// строка состоит больше чем из трех символов
				PositionColParse("ZZZA");
				assert(false);
			}
			catch (const std::exception&){
				// мы должны оказаться тут
				assert(true);
				std::cerr << "   detail::tests::FromStringColAddressCalculate OK" << std::endl;
				
			}

		}

		// Проверка корректности рекурентного вычисления индекса столбца из строки
		void FromStringRecursiveColAddressCalculate() {

			try {
				// односимвольные
				assert(calculate::FromStringRecursiveSymbolCalculate("A", 0) - 1 == 0);
				assert(calculate::FromStringRecursiveSymbolCalculate("Z", 0) - 1 == 25);
				// двухсимвольные
				assert(calculate::FromStringRecursiveSymbolCalculate("AA", 1) - 1 == 26);
				assert(calculate::FromStringRecursiveSymbolCalculate("AB", 1) - 1 == 27);
				assert(calculate::FromStringRecursiveSymbolCalculate("AZ", 1) - 1 == 51);
				assert(calculate::FromStringRecursiveSymbolCalculate("BA", 1) - 1 == 52);
				assert(calculate::FromStringRecursiveSymbolCalculate("CA", 1) - 1 == 78);
				assert(calculate::FromStringRecursiveSymbolCalculate("DF", 1) - 1 == 109);
				assert(calculate::FromStringRecursiveSymbolCalculate("ZZ", 1) - 1 == 701);
				// трехсимвольные
				assert(calculate::FromStringRecursiveSymbolCalculate("AAA", 2) - 1 == 702);
				assert(calculate::FromStringRecursiveSymbolCalculate("ABA", 2) - 1 == 728);
				assert(calculate::FromStringRecursiveSymbolCalculate("ACA", 2) - 1 == 754);
				assert(calculate::FromStringRecursiveSymbolCalculate("ADE", 2) - 1 == 784);
				assert(calculate::FromStringRecursiveSymbolCalculate("BAA", 2) - 1 == 1378);
				assert(calculate::FromStringRecursiveSymbolCalculate("BCA", 2) - 1 == 1430);
				assert(calculate::FromStringRecursiveSymbolCalculate("BCD", 2) - 1 == 1433);
				assert(calculate::FromStringRecursiveSymbolCalculate("FAA", 2) - 1 == 4082);
				assert(calculate::FromStringRecursiveSymbolCalculate("FCA", 2) - 1 == 4134);
				assert(calculate::FromStringRecursiveSymbolCalculate("FCC", 2) - 1 == 4136);
				assert(calculate::FromStringRecursiveSymbolCalculate("XFD", 2) - 1 == 16383);
			}
			catch (const std::exception&) {
				assert(false);
			}

			std::cerr << "   detail::tests::FromStringRecursiveColAddressCalculate OK" << std::endl;
		}

		// Проверка парсинга строки
		void FromStringBaseParsing() {

			assert(detail::FromStringParse("AAAZ12") == Position::NONE);
			assert(detail::FromStringParse("AZ12A-*45a") == Position::NONE);
			assert(detail::FromStringParse("AA12") != Position::NONE);

			assert(detail::FromStringParse("") == Position::NONE);
			assert(detail::FromStringParse("A") == Position::NONE);
			assert(detail::FromStringParse("1") == Position::NONE);
			assert(detail::FromStringParse("e2") == Position::NONE);
			assert(detail::FromStringParse("A0") == Position::NONE);
			assert(detail::FromStringParse("A-1") == Position::NONE);
			assert(detail::FromStringParse("A+1") == Position::NONE);
			assert(detail::FromStringParse("R2D2") == Position::NONE);
			assert(detail::FromStringParse("C3PO") == Position::NONE);
			assert(detail::FromStringParse("A16385") == Position::NONE);
			assert(detail::FromStringParse("XFD16385") == Position::NONE);
			assert(detail::FromStringParse("XFE16384") == Position::NONE);
			assert(detail::FromStringParse("A1234567890123456789") == Position::NONE);
			assert(detail::FromStringParse("ABCDEFGHIJKLMNOPQRS8") == Position::NONE);

			std::cerr << "   detail::tests::FromStringBaseParsing OK" << std::endl;
		}

		// Проверка корректности преобразования номера в буквенный код
		void ToStringColAddressCalculate() {
			assert(calculate::ToStringNumericCast(0) == "A");
			assert(calculate::ToStringNumericCast(1) == "B");
			assert(calculate::ToStringNumericCast(10) == "K");
			assert(calculate::ToStringNumericCast(24) == "Y");
			assert(calculate::ToStringNumericCast(25) == "Z");

			assert(calculate::ToStringNumericCast(26) == "AA");
			assert(calculate::ToStringNumericCast(27) == "AB");
			assert(calculate::ToStringNumericCast(51) == "AZ");
			assert(calculate::ToStringNumericCast(179) == "FX");
			assert(calculate::ToStringNumericCast(337) == "LZ");
			assert(calculate::ToStringNumericCast(657) == "YH");
			assert(calculate::ToStringNumericCast(700) == "ZY");
			assert(calculate::ToStringNumericCast(701) == "ZZ");

			assert(calculate::ToStringNumericCast(702) == "AAA");
			assert(calculate::ToStringNumericCast(703) == "AAB");
			assert(calculate::ToStringNumericCast(805) == "ADZ");
			// этот ассерт не проходит в наивной реализации
			//assert(calculate::ToStringColReverst(1351) == "AYZ");
			assert(calculate::ToStringNumericCast(2244) == "CHI");
			assert(calculate::ToStringNumericCast(4392) == "FLY");
			assert(calculate::ToStringNumericCast(4393) == "FLZ");
			assert(calculate::ToStringNumericCast(4394) == "FMA");
			assert(calculate::ToStringNumericCast(16383) == "XFD");
			assert(calculate::ToStringNumericCast(16385) == "");

			std::cerr << "   detail::tests::ToStringColAddressCalculate OK" << std::endl;
		}

		void ToStringRecurseColAddressCalculate() {

			// односимвольные
			assert(calculate::ToStringRecursiveNumericCast(0 + 1) == "A");
			assert(calculate::ToStringRecursiveNumericCast(1 + 1) == "B");
			assert(calculate::ToStringRecursiveNumericCast(10 + 1) == "K");
			assert(calculate::ToStringRecursiveNumericCast(24 + 1) == "Y");
			assert(calculate::ToStringRecursiveNumericCast(25 + 1) == "Z");

			// двухсимвольные
			assert(calculate::ToStringRecursiveNumericCast(26 + 1) == "AA");
			assert(calculate::ToStringRecursiveNumericCast(27 + 1) == "AB");
			assert(calculate::ToStringRecursiveNumericCast(51 + 1) == "AZ");
			assert(calculate::ToStringRecursiveNumericCast(52 + 1) == "BA");
			assert(calculate::ToStringRecursiveNumericCast(53 + 1) == "BB");
			assert(calculate::ToStringRecursiveNumericCast(76 + 1) == "BY");
			assert(calculate::ToStringRecursiveNumericCast(77 + 1) == "BZ");
			assert(calculate::ToStringRecursiveNumericCast(78 + 1) == "CA");
			assert(calculate::ToStringRecursiveNumericCast(79 + 1) == "CB");
			assert(calculate::ToStringRecursiveNumericCast(675 + 1) == "YZ");
			assert(calculate::ToStringRecursiveNumericCast(676 + 1) == "ZA");
			assert(calculate::ToStringRecursiveNumericCast(700 + 1) == "ZY");
			assert(calculate::ToStringRecursiveNumericCast(701 + 1) == "ZZ");

			// трехсимвольные
			assert(calculate::ToStringRecursiveNumericCast(702 + 1) == "AAA");
			assert(calculate::ToStringRecursiveNumericCast(703 + 1) == "AAB");
			assert(calculate::ToStringRecursiveNumericCast(704 + 1) == "AAC");
			assert(calculate::ToStringRecursiveNumericCast(727 + 1) == "AAZ");
			assert(calculate::ToStringRecursiveNumericCast(728 + 1) == "ABA");
			assert(calculate::ToStringRecursiveNumericCast(730 + 1) == "ABC");

			assert(calculate::ToStringRecursiveNumericCast(1350 + 1) == "AYY");
			assert(calculate::ToStringRecursiveNumericCast(1351 + 1) == "AYZ");
			assert(calculate::ToStringRecursiveNumericCast(1352 + 1) == "AZA");
			assert(calculate::ToStringRecursiveNumericCast(1377 + 1) == "AZZ");

			assert(calculate::ToStringRecursiveNumericCast(1378 + 1) == "BAA");
			assert(calculate::ToStringRecursiveNumericCast(1402 + 1) == "BAY");
			assert(calculate::ToStringRecursiveNumericCast(1403 + 1) == "BAZ");
			assert(calculate::ToStringRecursiveNumericCast(1404 + 1) == "BBA");
			assert(calculate::ToStringRecursiveNumericCast(1405 + 1) == "BBB");
			assert(calculate::ToStringRecursiveNumericCast(1429 + 1) == "BBZ");
			assert(calculate::ToStringRecursiveNumericCast(1430 + 1) == "BCA");

			assert(calculate::ToStringRecursiveNumericCast(2027 + 1) == "BYZ");
			assert(calculate::ToStringRecursiveNumericCast(2028 + 1) == "BZA");
			assert(calculate::ToStringRecursiveNumericCast(2053 + 1) == "BZZ");
			assert(calculate::ToStringRecursiveNumericCast(2054 + 1) == "CAA");

			assert(calculate::ToStringRecursiveNumericCast(5491 + 1) == "HCF");
			assert(calculate::ToStringRecursiveNumericCast(5510 + 1) == "HCY");
			assert(calculate::ToStringRecursiveNumericCast(5511 + 1) == "HCZ");
			assert(calculate::ToStringRecursiveNumericCast(5512 + 1) == "HDA");
			assert(calculate::ToStringRecursiveNumericCast(6083 + 1) == "HYZ");
			assert(calculate::ToStringRecursiveNumericCast(6084 + 1) == "HZA");
			assert(calculate::ToStringRecursiveNumericCast(6109 + 1) == "HZZ");

			assert(calculate::ToStringRecursiveNumericCast(16383 + 1) == "XFD");

			try
			{
				// функция должна бросить исключение, так как цисло за пределами диапазона
				assert(calculate::ToStringRecursiveNumericCast(16384 + 1) == "");
			}
			catch (const std::exception&)
			{
				// мы должны оказаться тут!
				assert(true);
			}
			

			std::cerr << "   detail::tests::ToStringRecurseColAddressCalculate OK" << std::endl;
		}

		// Проверка корректности преобразования номера в буквенный код
		void PositionToStringResultTest() {
			assert((Position{ 0, 16385 }).ToString() == "");
			assert((Position{ 16385, 0 }).ToString() == "");
			

			std::cerr << "   detail::tests::PositionToStringResultTest OK" << std::endl;
		}

	} // namespace tests 

} // namespace detail 


// ---------------------------------------- class Position ------------------------------------------------

// Реализуйте методы:
bool Position::operator==(const Position rhs) const {
	return this->col == rhs.col && this->row == rhs.row;
}

bool Position::operator!=(const Position rhs) const {
	return this->col != rhs.col && this->row != rhs.row;
}

bool Position::operator<(const Position rhs) const {
	if (this->col < rhs.col) {
		return true;
	}
	else if (this->col == rhs.col) {
		if (this->row < rhs.row) {
			return true;
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}
}

Position::Position(int r, int c) 
	: row(r), col(c) {
}

Position::Position(const Position& other) 
	: row(other.row), col(other.col) {
}

bool Position::IsValid() const {
	return (this->col >= 0 && this->col < MAX_COLS) && (this->row >= 0 && this->row < MAX_ROWS);
}

std::string Position::ToString() const {
	if (IsValid()){
		std::string result = detail::calculate::ToStringRecursiveNumericCast(col + 1);
		return result += std::to_string(row + 1);
	}
	return {};
}

Position Position::FromString(std::string_view str) {
	return detail::FromStringParse(str);
}

void Position::RunAllTest() {

	std::cerr << "PositionFullSelfTestBegin" << std::endl;

	detail::tests::FromStringColAddressCalculate();
	detail::tests::FromStringRecursiveColAddressCalculate();
	detail::tests::FromStringBaseParsing();
	detail::tests::ToStringColAddressCalculate();
	detail::tests::ToStringRecurseColAddressCalculate();
	detail::tests::PositionToStringResultTest();
}

// ---------------------------------------- class Position END --------------------------------------------