#include "expression_visitor.h"

namespace planmt {

void accept_visitor(const Expression& expr, ExpressionVisitor& visitor) {
    if (expr.is_atom()) {
        const Atom& atom = expr.value();
        
        if (atom.is_symbol()) {
            visitor.visit_symbol(atom.symbol(), expr.kind(), expr.type_enum());
        } else if (atom.is_integer()) {
            visitor.visit_integer(atom.integer(), expr.kind());
        } else if (atom.is_real()) {
            visitor.visit_real(atom.real(), expr.kind());
        } else if (atom.is_boolean()) {
            visitor.visit_boolean(atom.boolean(), expr.kind());
        }
    } 
    else if (expr.is_list()) {
        const auto& list = expr.list();
        
        // Determine what kind of list this is based on the kind
        switch (expr.kind()) {
            case Expression::Kind::FUNCTION_APPLICATION:
                if (!list.empty() && list[0].is_atom() && list[0].value().is_symbol()) {
                    std::string function_name = list[0].value().symbol();
                    std::vector<Expression> args(list.begin() + 1, list.end());
                    visitor.visit_function_application(function_name, args, expr.kind());
                } else {
                    visitor.visit_list(list, expr.kind());
                }
                break;
                
            case Expression::Kind::STATE_VARIABLE:
                if (!list.empty() && list[0].is_atom() && list[0].value().is_symbol()) {
                    std::string fluent_name = list[0].value().symbol();
                    std::vector<Expression> args(list.begin() + 1, list.end());
                    visitor.visit_fluent_application(fluent_name, args, expr.kind());
                } else {
                    visitor.visit_list(list, expr.kind());
                }
                break;
                
            default:
                visitor.visit_list(list, expr.kind());
                break;
        }
        
        // For recursive visiting, we need to visit all sub-expressions
        // This is done automatically by the visitor implementations if they want recursion
    }
}

} // namespace planmt
