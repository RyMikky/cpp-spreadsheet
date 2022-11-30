#include "unit_test_system.h"
#include "test_runner_p.h"

#include <deque>
#include <vector>

namespace unit_tests {

	namespace detail {


	} // namespace detail

	namespace position_tests {

		// запуск всех тестов структуры позиции
		void PositionCompleteTests() {
			Position pos(0,0);
			pos.RunAllTest();
		}

	} // namespace position_tests 

	namespace final_tests {

		// корректность определения зоны печати
		void SheetPrintRangeTest() {
			{
				Sheet a;
				a.SetCell({ 0, 0 }, "text");
				assert(a.GetPrintableSize() == Size(1, 1));

				a.SetCell({ 2, 7 }, "text");
				assert(a.GetPrintableSize() == Size(3, 8));

				a.SetCell({ 1, 3 }, "text");
				assert(a.GetPrintableSize() == Size(3, 8));

				a.ClearCell({ 0, 0 });
				assert(a.GetPrintableSize() == Size(3, 8));

				a.ClearCell({ 2, 7 });
				assert(a.GetPrintableSize() == Size(2, 4));
			}

			{
				Sheet a;
				a.SetCell({ 2, 7 }, "text");
				assert(a.GetPrintableSize() == Size(3, 8));

				a.SetCell({ 1, 3 }, "text");
				assert(a.GetPrintableSize() == Size(3, 8));

				a.SetCell({ 2, 2 }, "text");
				assert(a.GetPrintableSize() == Size(3, 8));

				a.ClearCell({ 2, 7 });
				assert(a.GetPrintableSize() == Size(3, 4));
			}

			{
				Sheet a;
				a.SetCell({ 1, 3 }, "text");
				assert(a.GetPrintableSize() == Size(2, 4));

				a.SetCell({ 2, 2 }, "text");
				assert(a.GetPrintableSize() == Size(3, 4));

				a.ClearCell({ 2, 2 });
				assert(a.GetPrintableSize() == Size(2, 4));

				a.SetCell({ 8, 1 }, "text");
				assert(a.GetPrintableSize() == Size(9, 4));

				a.ClearCell({ 1, 3 });
				assert(a.GetPrintableSize() == Size(9, 2));
			}
		}
		// вывод печатной области по значениям
		void SheetPrintValuesTest() {

			{
				Sheet a;
				a.SetCell({ 0,1 }, "vasya");
				a.SetCell({ 0,3 }, "masha");
				a.SetCell({ 1,0 }, "dasha");
				a.SetCell({ 1,2 }, "petya");

				assert(a.GetPrintableSize() == Size(2, 4));

				std::stringstream out;

				a.PrintValues(out);
				ASSERT_EQUAL(out.str(), "\tvasya\t\tmasha\ndasha\t\tpetya\t\n")
			}

			{
				Sheet a;
				a.SetCell({ 0,0 }, "=(1+2)*3");
				a.SetCell({ 0,1 }, "=1+(2*3)");

				a.SetCell({ 1,0 }, "some");
				a.SetCell({ 1,1 }, "text");
				a.SetCell({ 1,2 }, "here");

				a.SetCell({ 2,2 }, "'and");
				a.SetCell({ 2,3 }, "'here");

				a.SetCell({ 4,1 }, "=1/0");

				assert(a.GetPrintableSize() == Size(5, 4));

				std::stringstream out;

				a.PrintValues(out);

				ASSERT_EQUAL(out.str(), "9\t7\t\t\nsome\ttext\there\t\n\t\tand\there\n\t\t\t\n\t#DIV/0!\t\t\n")
			}

			{
				Sheet a;
				a.SetCell({ 0,0 }, "=(1+2)*3");
				a.SetCell({ 0,1 }, "=1+(2*3)");

				a.SetCell({ 1,0 }, "some");
				a.SetCell({ 1,1 }, "text");
				a.SetCell({ 1,2 }, "here");

				a.SetCell({ 2,2 }, "'and");
				a.SetCell({ 2,3 }, "'here");

				a.SetCell({ 4,1 }, "=1/0");

				assert(a.GetPrintableSize() == Size(5, 4));

				a.ClearCell({ 2,2 }); // удаляем ячейку из середины

				assert(a.GetPrintableSize() == Size(5, 4));

				std::stringstream out;

				a.PrintValues(out);

				ASSERT_EQUAL(out.str(), "9\t7\t\t\nsome\ttext\there\t\n\t\t\there\n\t\t\t\n\t#DIV/0!\t\t\n")
			}

			{
				auto sheet = CreateSheet();
				sheet->SetCell({ 1, 0 }, "meow");
				sheet->SetCell({ 1, 1 }, "=1+2");
				sheet->SetCell({ 0, 0 }, "=1/0");

				assert(sheet->GetPrintableSize() == (Size{ 2, 2 }));

				std::ostringstream values;
				sheet->PrintValues(values);
				ASSERT_EQUAL(values.str(), "#DIV/0!\t\nmeow\t3\n");

				sheet->ClearCell({ 1, 1 });
				assert(sheet->GetPrintableSize() == (Size{ 2, 1 }));

				std::ostringstream new_values;
				sheet->PrintValues(new_values);
				ASSERT_EQUAL(new_values.str(), "#DIV/0!\nmeow\n");
			}
		}
		// вывод печатной области текстом
		void SheetPrintTextesTest() {

			{
				Sheet a;
				a.SetCell({ 0,0 }, "=(1+2)*3");
				a.SetCell({ 0,1 }, "=1+(2*3)");

				a.SetCell({ 1,0 }, "some");
				a.SetCell({ 1,1 }, "text");
				a.SetCell({ 1,2 }, "here");

				a.SetCell({ 2,2 }, "'and");
				a.SetCell({ 2,3 }, "'here");

				a.SetCell({ 4,1 }, "=1/0");

				assert(a.GetPrintableSize() == Size(5, 4));

				std::stringstream out;

				a.PrintTexts(out);

				ASSERT_EQUAL(out.str(), "=(1+2)*3\t=1+2*3\t\t\nsome\ttext\there\t\n\t\t'and\t'here\n\t\t\t\n\t=1/0\t\t\n")
			}

			{
				Sheet a;
				a.SetCell({ 0,0 }, "=(1+2)*3");
				a.SetCell({ 0,1 }, "=1+(2*3)");

				a.SetCell({ 1,0 }, "some");
				a.SetCell({ 1,1 }, "text");
				a.SetCell({ 1,2 }, "here");

				a.SetCell({ 2,2 }, "'and");
				a.SetCell({ 2,3 }, "'here");

				a.SetCell({ 4,1 }, "=1/0");

				assert(a.GetPrintableSize() == Size(5, 4));

				std::stringstream out;

				a.ClearCell({ 4,1 });
				assert(a.GetPrintableSize() == Size(3, 4));

				a.PrintTexts(out);

				ASSERT_EQUAL(out.str(), "=(1+2)*3\t=1+2*3\t\t\nsome\ttext\there\t\n\t\t'and\t'here\n")
			}

			{
				auto sheet = CreateSheet();
				sheet->SetCell({ 1, 0 }, "meow");
				sheet->SetCell({ 1, 1 }, "=1+2");
				sheet->SetCell({ 0, 0 }, "=1/0");

				assert(sheet->GetPrintableSize() == (Size{ 2, 2 }));

				std::ostringstream texts;
				sheet->PrintTexts(texts);
				ASSERT_EQUAL(texts.str(), "=1/0\t\nmeow\t=1+2\n");

				sheet->ClearCell({ 1, 1 });
				assert(sheet->GetPrintableSize() == (Size{ 2, 1 }));


				std::ostringstream new_textes;
				sheet->PrintTexts(new_textes);
				ASSERT_EQUAL(new_textes.str(), "=1/0\nmeow\n");
			}
		}
		// тестирование отложенных ссылок
		void FutureReferencesTest() {

			auto sheet = CreateSheet();

			sheet->SetCell({1,1}, "=B1");
			assert(sheet->GetCell({ 1,1 })->GetValue() == CellInterface::Value(0.0));
			sheet->SetCell({ 0,1 }, "5");
			assert(sheet->GetCell({ 1,1 })->GetValue() == CellInterface::Value(5.0));

			// все ссылаются на C1
			sheet->SetCell({ 0,1 }, "=C1");       // B1
			sheet->SetCell({ 1,1 }, "=C1+C2");    // B2
			sheet->SetCell({ 2,1 }, "=C1-C2");    // B3

			// кеши должны инвалидироваться
			assert(sheet->GetCell({ 0,1 })->GetValue() == CellInterface::Value(0.0));
			assert(sheet->GetCell({ 1,1 })->GetValue() == CellInterface::Value(0.0));
			assert(sheet->GetCell({ 2,1 })->GetValue() == CellInterface::Value(0.0));

			// создаём С1
			sheet->SetCell({ 0,2 }, "8");         // C1
			// так как В1 зависит только от С1, то она должна посчитаться полностью
			// остальные будут возвращать лишь часть посчитанной формулы
			assert(sheet->GetCell({ 0,1 })->GetValue() == CellInterface::Value(8.0));
			assert(sheet->GetCell({ 1,1 })->GetValue() == CellInterface::Value(8.0));  // 8.0 + 0.0
			assert(sheet->GetCell({ 2,1 })->GetValue() == CellInterface::Value(8.0));  // 8.0 - 0.0

			// создаём С2
			sheet->SetCell({ 1,2 }, "4");         // C2
			assert(sheet->GetCell({ 0,1 })->GetValue() == CellInterface::Value(8.0));
			assert(sheet->GetCell({ 1,1 })->GetValue() == CellInterface::Value(12.0)); // 8.0 + 4.0
			assert(sheet->GetCell({ 2,1 })->GetValue() == CellInterface::Value(4.0));  // 8.0 - 4.0
		}
		// определение циклической заивисимости
		void CyclicDependenceTest() {

			{
				auto sheet = CreateSheet();

				sheet->SetCell({ 1,1 }, "=B1");  // B2
				sheet->SetCell({ 0,1 }, "5");    // B1
				assert(sheet->GetCell({ 1,1 })->GetValue() == CellInterface::Value(5.0));

				try
				{
					sheet->SetCell({ 0,1 }, "=B2");  // B1
					assert(false);
				} catch (const std::exception&) {
					// должны попасть сюда
				}
			}

			{
				auto sheet = CreateSheet();

				sheet->SetCell({ 1,1 }, "=B1");       // B2 -> B1
				sheet->SetCell({ 0,1 }, "=C3+D5");    // B1 -> C3, C5
				sheet->SetCell({ 2,2 }, "=D2");       // C3 -> D2

				try
				{
					sheet->SetCell({ 1,3 }, "=B2");   // D2 -> B2
					assert(false);
				}
				catch (const std::exception&) {
					// должны попасть сюда
				}
			}

			{
				auto sheet = CreateSheet();

				sheet->SetCell({ 1,1 }, "=B1");       // B2 -> B1
				sheet->SetCell({ 0,1 }, "=C3+D5");    // B1 -> C3, C5
				sheet->SetCell({ 2,2 }, "=D2");       // C3 -> D2
				sheet->SetCell({ 4,3 }, "=D2");       // C5 -> D2

				try
				{
					sheet->SetCell({ 1,3 }, "=B1");   // D2 -> B1
					assert(false);
				}
				catch (const std::exception&) {
					// должны попасть сюда
				}
			}

			{
				auto sheet = CreateSheet();

				sheet->SetCell({ 1,1 }, "=B1");       // B2 -> B1
				sheet->SetCell({ 0,1 }, "=C3+D5");    // B1 -> C3, C5
				sheet->SetCell({ 2,2 }, "=D2");       // C3 -> D2
				sheet->SetCell({ 4,3 }, "=D2");       // C5 -> D2
				sheet->SetCell({ 0,0 }, "=D2+C5+C3+B1");       // A1 -> D2, C5, C3, B1

				try
				{
					sheet->SetCell({ 0,1 }, "=A1");   // B1 -> A1
					assert(false);
				}
				catch (const std::exception&) {
					// должны попасть сюда
				}
			}
		}

	} //namespace final_tests


	void RunAllTests() {

		TestRunner tr;

		// блок тестов работоспособности позиции
		tr.RunTest(position_tests::PositionCompleteTests, "PositionCompleteTests");
		tr.RunTest(final_tests::SheetPrintRangeTest, "SheetPrintRangeTest");
		tr.RunTest(final_tests::SheetPrintValuesTest, "SheetPrintValuesTest");
		tr.RunTest(final_tests::SheetPrintTextesTest, "SheetPrintTextesTest");
		tr.RunTest(final_tests::FutureReferencesTest, "FutureReferencesTest");
		tr.RunTest(final_tests::CyclicDependenceTest, "CyclicDependenceTest");
	}

} // namespace unit_tests