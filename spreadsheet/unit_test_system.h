#pragma once

#include "cell.h"
#include "sheet.h"
#include "common.h"
#include "formula.h"

#include <vector>

namespace unit_tests {

	namespace detail {

		const std::vector<std::string> __FORMULAS__ = { "=1/2", "=4/2", "=4*2", "=4+2*8", "=(5/2)", "=1/8", "=3+2", "=5-2" };
		const std::vector<std::string> __TEXTS__ = { "text_1", "text_2", "text_3", "text_4", "text_5", "text_6", "text_7" };

		enum CellType
		{
			raw = 0, clear, empty, text, formula
		};

	} // namespace detail

	namespace final_tests {

		void SheetPrintRangeTest();                                     // корректность определения зоны печати
		void SheetPrintValuesTest();                                    // вывод печатной области по значениям
		void SheetPrintTextesTest();                                    // вывод печатной области текстом
		void FutureReferencesTest();                                    // тестирование отложенных ссылок
		void CyclicDependenceTest();                                    // определение циклической заивисимости

	} //namespace final_tests


	namespace position_tests {

		void PositionCompleteTests();                                   // запуск всех тестов структуры позиции

	} // namespace position_tests 

	void RunAllTests();

} // namespace unit_tests