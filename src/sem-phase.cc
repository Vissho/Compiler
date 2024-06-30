#include "cool-parse.h"
#include "cool-tree.h"
#include "utilities.h"
#include <cstdio>
#include <functional>
#include <iostream>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

std::FILE* token_file = stdin;
extern Classes parse_results;
extern Program ast_root;
extern int curr_lineno;
const char* curr_filename = "<stdin>";
extern int parse_errors;

extern int yy_flex_debug;
extern int cool_yydebug;
int lex_verbose = 0;
extern int cool_yyparse();

int err_count = 0;

void sequence_out(std::string title, std::unordered_set<std::string> set)
{
    std::cerr << title << ": ";
    for (auto s : set) {
        std::cerr << s << ' ';
    }
    std::cerr << '\n';
}

bool DFS(
        const std::string& node,
        std::unordered_map<std::string, std::string>& inheritance,
        std::unordered_set<std::string>& visited,
        std::unordered_set<std::string>& recStack)
{
    visited.insert(node);
    recStack.insert(node);
    if (inheritance.find(node) != inheritance.end()) {
        std::string parent = inheritance[node];
        if (visited.find(parent) == visited.end()
            && DFS(parent, inheritance, visited, recStack)) {
            return true;
        } else if (recStack.find(parent) != recStack.end()) {
            return true;
        }
    }
    recStack.erase(node);
    return false;
}

bool cycle_inheritance(
        std::unordered_map<std::string, std::string>& inheritance)
{
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recStack;

    for (auto& pair : inheritance) {
        if (visited.find(pair.first) == visited.end()) {
            if (DFS(pair.first, inheritance, visited, recStack)) {
                return true;
            }
        }
    }
    return false;
}

bool check_signatures(method_class* m1, method_class* m2)
{
    // Get return types
    GetType m1_type_visitor;
    GetType m2_type_visitor;
    m1->accept(m1_type_visitor);
    m2->accept(m2_type_visitor);
    // Check methods return types
    if (m1_type_visitor.type != m2_type_visitor.type) {
        return false;
    }

    // Get formals
    GetFormals m1_formals_visitor;
    GetFormals m2_formals_visitor;
    m1->accept(m1_formals_visitor);
    m2->accept(m2_formals_visitor);
    Formals m1_formals = m1_formals_visitor.formals;
    Formals m2_formals = m2_formals_visitor.formals;

    // Check formals count
    if (m1_formals->len() != m2_formals->len()) {
        return false;
    }

    // Loop through formals
    for (int i = m1_formals->first(); m1_formals->more(i);
         i = m1_formals->next(i)) {
        formal_class* m1_formal
                = dynamic_cast<formal_class*>(m1_formals->nth(i));
        formal_class* m2_formal
                = dynamic_cast<formal_class*>(m2_formals->nth(i));

        // Get formal names
        GetName m1_formal_generalVisitor;
        GetName m2_formal_generalVisitor;
        m1_formal->accept(m1_formal_generalVisitor);
        m2_formal->accept(m2_formal_generalVisitor);
        std::string name1 = m1_formal_generalVisitor.name;
        std::string name2 = m2_formal_generalVisitor.name;
        // Check formal names
        if (name1 != name2) {
            return false;
        }

        // Get formal types
        GetType m1_formal_type_visitor;
        GetType m2_formal_type_visitor;
        m1_formal->accept(m1_formal_type_visitor);
        m2_formal->accept(m2_formal_type_visitor);
        std::string type1 = m1_formal_type_visitor.type;
        std::string type2 = m2_formal_type_visitor.type;
        // Check formal types
        if (type1 != type2) {
            return false;
        }
    }
    return true;
}

class__class* find_class(std::string name, Classes classes)
{
    for (int i = classes->first(); classes->more(i); i = classes->next(i)) {
        GetName generalVisitor;
        class__class* cur_class = dynamic_cast<class__class*>(classes->nth(i));
        cur_class->accept(generalVisitor);
        if (name == generalVisitor.name) {
            return cur_class;
        }
    }
    return nullptr;
}

void dump_symtables(IdTable idtable, StrTable strtable, IntTable inttable)
{
    ast_root->dump_with_types(std::cerr, 0);
    std::cerr << "\nID table:";
    idtable.print();

    std::cerr << "\nSTRING table:";
    stringtable.print();

    std::cerr << "\nINT table:";
    inttable.print();
}

void semantics_error(std::string error_msg)
{
    std::cerr << "\033[1;31mError:\033[0m " << error_msg << '\n';
    err_count++;
}

void check_builtin_types_init(std::string type, Expression expr)
{
    std::string expr_type = expr->get_expr_type();
    if (expr_type == "no_expr_class") {
        return;
    }
    if (type == "Int" && expr_type != "int_const_class") {
        semantics_error("Initialization of Int with non-integer value");
    } else if (type == "Bool" && expr_type != "bool_const_class") {
        semantics_error("Initialization of Bool with non-boolean value");
    } else if (type == "String" && expr_type != "string_const_class") {
        semantics_error("Initialization of String with non-string value");
    }
}

int main(int argc, char** argv)
{
    yy_flex_debug = 0;
    cool_yydebug = 0;
    lex_verbose = 0;
    for (int i = 1; i < argc; i++) {
        token_file = std::fopen(argv[i], "r");
        if (token_file == NULL) {
            std::cerr << "Error: can not open file " << argv[i] << std::endl;
            std::exit(1);
        }
        curr_lineno = 1;
        cool_yyparse();
        if (parse_errors != 0) {
            std::cerr << "Error: parse errors\n";
            std::exit(1);
        }

        // Adding built-in types to AST
        Symbol Object = idtable.add_string("Object");
        Symbol Bool = idtable.add_string("Bool");
        Symbol Int = idtable.add_string("Int");
        Symbol String = idtable.add_string("String");
        Symbol SELF_TYPE = idtable.add_string("SELF_TYPE");

        // dump_symtables(idtable, stringtable, inttable);

        std::unordered_set<std::string> non_inherited_types{
                "Bool", "Int", "String", "SELF_TYPE"};
        std::unordered_set<std::string> classes_names{
                "Object", "Bool", "Int", "String", "SELF_TYPE"};
        std::unordered_map<std::string, std::string> classes_hierarchy;
        GetName generalVisitor;

        // Loop through classes
        for (int i = parse_results->first(); parse_results->more(i);
             i = parse_results->next(i)) {
            // Get current class name
            parse_results->nth(i)->accept(generalVisitor);
            std::string class_name = generalVisitor.name;

            // Check unique class name
            auto result = classes_names.insert(class_name);
            if (!result.second)
                semantics_error("Class '" + class_name + "' already defined");

            // Add class to inheritance hierarchy
            GetParent parentVisitor;
            parse_results->nth(i)->accept(parentVisitor);
            std::string parent_name = parentVisitor.name;
            classes_hierarchy[class_name] = parent_name;

            // Check that parent class isn't builtin (except 'Object')
            if (non_inherited_types.find(parent_name)
                != non_inherited_types.end())
                semantics_error(
                        "Class '" + class_name + "': shouldn't use '"
                        + parent_name + "' as parent");

            // Get class features
            GetFeatures featuresVisitor;
            parse_results->nth(i)->accept(featuresVisitor);
            Features features = featuresVisitor.features;
            std::unordered_set<std::string> features_names;

            // Loop through features
            for (int j = features->first(); features->more(j);
                 j = features->next(j)) {
                // Current feature
                Feature feature = features->nth(j);

                // Get feature name
                feature->accept(generalVisitor);
                std::string feature_name = generalVisitor.name;

                // 'self' name check
                if (feature_name == "self")
                    semantics_error(
                            "Name 'self' can't be used as feature name");

                // Check unique feature name
                result = features_names.insert(feature_name);
                if (!result.second)
                    semantics_error(
                            "Feature '" + feature_name + "' in '" + class_name
                            + "' already defined");

                // Get feature type: methods - return_type, attrs - type_decl
                GetType type_visitor;
                feature->accept(type_visitor);

                // Type existence check
                std::string type = type_visitor.type;
                if (classes_names.find(type) == classes_names.end())
                    semantics_error(
                            "Unknown type '" + type + "' in " + feature_name);

                // SELF_TYPE check
                if (type == "SELF_TYPE")
                    semantics_error(
                            "SELF_TYPE can't be used as a type inside class");

                if (feature->get_feature_type() == "method_class") {
                    // Methods formals
                    GetFormals formals_visitor;
                    feature->accept(formals_visitor);
                    Formals formals = formals_visitor.formals;

                    // Check method overrides - must have same signature
                    if (std::string(parent_name) != "Object") {
                        // Get parent class features
                        GetFeatures parent_featuresVisitor;
                        class__class* parent
                                = find_class(parent_name, parse_results);
                        if (parent) {
                            parent->accept(parent_featuresVisitor);
                            Features parent_features
                                    = parent_featuresVisitor.features;

                            // Loop through parent features
                            for (int a = parent_features->first();
                                 parent_features->more(a);
                                 a = parent_features->next(a)) {
                                Feature parent_feature
                                        = parent_features->nth(a);

                                // Get feature name
                                parent_feature->accept(generalVisitor);
                                std::string parent_feature_name
                                        = generalVisitor.name;

                                // If there is parent feature with same name
                                if (parent_feature_name == feature_name) {
                                    // Check if feature is same type
                                    if (parent_feature->get_feature_type()
                                        != feature->get_feature_type()) {
                                        semantics_error(
                                                "Incorrect override of feature "
                                                "'"
                                                + feature_name
                                                + "' from class '" + parent_name
                                                + "' in class '" + class_name
                                                + "'");
                                    }

                                    // Check method signatures
                                    method_class* cur_method
                                            = dynamic_cast<method_class*>(
                                                    feature);
                                    method_class* parent_method
                                            = dynamic_cast<method_class*>(
                                                    parent_feature);
                                    if (!check_signatures(
                                                cur_method, parent_method)) {
                                        semantics_error("'" + feature_name + "' method from class '" +
                                                                  parent_name +
                                                                  "' doesn't match override version "
                                                                  "of it in class '" +
                                                                  class_name + "'");
                                    }
                                }
                            }
                        } else {
                            semantics_error(
                                    "Parent class '" + std::string(parent_name)
                                    + "' of class '" + class_name
                                    + "' doesn't exist");
                        }
                    }

                    // Local formals names
                    std::unordered_set<std::string> formals_names;

                    // Loop through formals
                    for (int k = formals->first(); formals->more(k);
                         k = formals->next(k)) {
                        // Get formal name
                        formals->nth(k)->accept(generalVisitor);
                        std::string formal_name = generalVisitor.name;

                        // 'self' name check
                        if (formal_name == "self") {
                            semantics_error(
                                    "Name 'self' can't be used as formal name");
                        }

                        // Unique name check
                        result = formals_names.insert(formal_name);
                        if (!result.second) {
                            semantics_error(
                                    "Formal '" + std::string(formal_name)
                                    + "' in '" + feature_name
                                    + "' already defined");
                        }

                        // Get formal type
                        formals->nth(k)->accept(type_visitor);
                        type = type_visitor.type;

                        // Check formal type
                        if (classes_names.find(type) == classes_names.end()) {
                            semantics_error(
                                    "Unknown type '" + type + "' in "
                                    + formal_name);
                        }

                        // Get method expression
                        GetExpression expr_visitor;
                        features->nth(j)->accept(expr_visitor);
                        Expression expr = expr_visitor.expr;

                        // block_class check
                        if (expr->get_expr_type() == "block_class") {
                            // Get expressions from block
                            GetExpressions exprs_visitor;
                            expr->accept(exprs_visitor);
                            Expressions exprs = exprs_visitor.exprs;

                            // Block expressions check
                            for (int l = exprs->first(); exprs->more(l);
                                 l = exprs->next(l)) {
                                Expression current = exprs->nth(l);

                                // let
                                if (current->get_expr_type() == "let_class") {
                                    // Get let-expr variable name
                                    current->accept(generalVisitor);
                                    formal_name = generalVisitor.name;

                                    // 'self' name check
                                    if (formal_name == "self") {
                                        semantics_error(
                                                "Name 'self' can't be used as "
                                                "new local "
                                                "variable name");
                                    }

                                    // Check unique of nested formal
                                    result = formals_names.insert(formal_name);
                                    if (!result.second) {
                                        semantics_error(
                                                "Formal '"
                                                + std::string(formal_name)
                                                + "' in '" + feature_name
                                                + "' from '" + class_name
                                                + "' already defined");
                                    }

                                    // Let-expr formal type check
                                    current->accept(type_visitor);
                                    type = type_visitor.type;
                                    if (classes_names.find(type)
                                        == classes_names.end()) {
                                        semantics_error(
                                                "Unknown type '" + type
                                                + "' in " + formal_name);
                                    }
                                }
                            }
                        }
                    }
                } else { // attr_class
                    // Check init expression
                    attr_class* attr = dynamic_cast<attr_class*>(feature);
                    attr->accept(type_visitor);
                    GetExpression getExpr;
                    attr->accept(getExpr);
                    check_builtin_types_init(type_visitor.type, getExpr.expr);
                }
            }

            // Check existence of method main in class Main
            if (class_name == "Main"
                && features_names.find("main") == features_names.end())
                semantics_error("Method 'main' in 'Main' class doesn't exist");
        }

        // Check existence of class Main
        if (classes_names.find("Main") == classes_names.end())
            semantics_error("Start point - class 'Main' doesn't exist");

        // Inheritance hierarchy loop check
        if (cycle_inheritance(classes_hierarchy)) {
            semantics_error("Cycles are found in the inheritance graph");
        }
        std::fclose(token_file);
    }

    if (err_count > 0) {
        std::cerr << "\033[1;31mSemantic errors:\033[0m " << ::err_count
                  << " errors\n";
    }

    return err_count;
}