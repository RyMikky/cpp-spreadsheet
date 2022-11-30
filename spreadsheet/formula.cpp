#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << fe.ToString();
}

namespace {
    double GetDoubleFrom(const std::string& str) {
        double value = 0;
        if (!str.empty()) {
            std::istringstream in(str);
            if (!(in >> value) || !in.eof()) {
                throw FormulaError(FormulaError::Category::Value);
            }
        }
        return value;
    }

    double GetDoubleFrom(double value) {
        return value;
    }

    double GetDoubleFrom(FormulaError error) {
        throw FormulaError(error);
    }

    double GetCellValue(const CellInterface* cell) {
        if (!cell) return 0;
        return std::visit([](const auto& value) { return GetDoubleFrom(value); },
            cell->GetValue());
    }
}

namespace {
    class Formula : public FormulaInterface {
    public:
        // Реализуйте следующие методы:
        explicit Formula(std::string expression) try
            : ast_(ParseFormulaAST(expression)) {
        }
        catch (const std::exception& exc){
            std::throw_with_nested(FormulaException(exc.what()));
        }

        Value Evaluate(const SheetInterface& sheet) const override {
            try
            {
                // лямбда поиска ячейки CellFinder
                auto cell_finder = [&sheet](Position position) -> double {
                    if (!position.IsValid()) {
                        throw FormulaError(FormulaError::Category::Ref);
                    }
                    const auto* cell = sheet.GetCell(position);
                    return GetCellValue(cell);
                };
                return ast_.Execute(cell_finder);
            }
            catch (FormulaError exception)
            {
                return exception;
            }
        }

        std::string GetExpression() const override {
            std::stringstream out;
            ast_.PrintFormula(out);
            return out.str();
        }

        std::vector<Position> GetReferencedCells() const override {
            std::vector<Position> result;

            for (Position item : ast_.GetReferenceList()) {
                if (item.IsValid()) {
                    result.push_back(item);
                }
                
            }
            // убираем дубликаты
            result.resize(std::unique(result.begin(), result.end()) - result.begin());
            return result;
        }

        bool HasDepends() const override {
            return ast_.HasDepends();
        }

    private:
        FormulaAST ast_;
    };
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(std::move(expression));
}