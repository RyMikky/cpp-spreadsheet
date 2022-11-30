#pragma once

#include "FormulaLexer.h"
#include "common.h"

#include <forward_list>
#include <functional>
#include <stdexcept>

// лямбда-функция поиска ячейки по позиции в таблице
// возвращает результат в double если ячейка существует и нормально считается
using CellFinder = std::function<double(Position)>;

namespace ASTImpl {
class Expr;
}

class ParsingError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class FormulaAST {
public:
    explicit FormulaAST(std::unique_ptr<ASTImpl::Expr> root_expr,
                        std::forward_list<Position> cells);
    FormulaAST(FormulaAST&&) = default;
    FormulaAST& operator=(FormulaAST&&) = default;
    ~FormulaAST();

    // в экзекут передается лямбда поиска и применяется по необходимости 
    // если ячейка имеет в себе ссылки, для обычной строки надобности в поисковике нет
    double Execute(CellFinder [[maybe_unused]]finder) const;
    void PrintReferenceCells(std::ostream& out) const;                     // печать листа ячеек
    void Print(std::ostream& out) const;                                   // обычная печать
    void PrintFormula(std::ostream& out) const;                            // печать формулы
    bool HasDepends() const;                                               // возвращает флаг того, что есть вектор зависимостей
    std::forward_list<Position> GetReferenceList() ;                       // возвращает вектор позиций ссылок
    const std::forward_list<Position>& GetReferenceList() const;           // возвращает вектор позиций ссылок

private:
    std::unique_ptr<ASTImpl::Expr> root_expr_;
    std::forward_list<Position> cells_;
};

FormulaAST ParseFormulaAST(std::istream& in);
FormulaAST ParseFormulaAST(const std::string& in_str);