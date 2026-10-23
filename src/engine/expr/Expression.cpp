#include "engine/expr/Expression.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <regex>

#include "engine/StringHelper.h"

namespace eqapo {

// ----------------------------------------------------------------- Value

Value Value::fromNumber(double d, bool integer) {
    Value v;
    v.type = Number;
    v.number = d;
    v.isInteger = integer;
    return v;
}
Value Value::fromBool(bool b) {
    Value v;
    v.type = Bool;
    v.boolean = b;
    return v;
}
Value Value::fromString(std::string s) {
    Value v;
    v.type = String;
    v.string = std::move(s);
    return v;
}
Value Value::fromArray(std::vector<Value> a) {
    Value v;
    v.type = Array;
    v.array = std::move(a);
    return v;
}

std::string Value::toString() const {
    switch (type) {
        case Number: {
            if (isInteger && std::fabs(number) < 1e15 && number == std::floor(number)) return std::to_string(static_cast<long long>(number));
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.15g", number);
            return buf;
        }
        case Bool: return boolean ? "true" : "false";
        case String: return string;
        case Array: {
            std::string s = "{";
            for (size_t i = 0; i < array.size(); i++) {
                if (i) s += ", ";
                s += array[i].toString();
            }
            return s + "}";
        }
    }
    return "";
}

bool Value::toBool() const {
    switch (type) {
        case Number: return number != 0;
        case Bool: return boolean;
        case String: return !string.empty();
        case Array: return !array.empty();
    }
    return false;
}

double Value::toNumber() const {
    switch (type) {
        case Number: return number;
        case Bool: return boolean ? 1 : 0;
        default: throw ExpressionError("expected a numeric value but got type '" + std::string(1, typeChar()) + "'");
    }
}

char Value::typeChar() const {
    switch (type) {
        case Number: return isInteger ? 'i' : 'f';
        case Bool: return 'b';
        case String: return 's';
        case Array: return 'm';
    }
    return '?';
}

// ----------------------------------------------------------------- tokens

namespace {

enum class Tok { End, Number, String, Ident, Op };

struct Token {
    Tok type = Tok::End;
    std::string text;
    double number = 0;
    bool integer = false;
};

class Lexer {
public:
    explicit Lexer(const std::string& s) : s_(s) {}

    std::vector<Token> run() {
        std::vector<Token> tokens;
        while (true) {
            Token t = next();
            tokens.push_back(t);
            if (t.type == Tok::End) break;
        }
        return tokens;
    }

private:
    Token next() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) pos_++;
        Token t;
        if (pos_ >= s_.size()) return t;
        char c = s_[pos_];
        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && pos_ + 1 < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_ + 1])))) {
            size_t start = pos_;
            bool integer = true;
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) pos_++;
            if (pos_ < s_.size() && s_[pos_] == '.') {
                integer = false;
                pos_++;
                while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) pos_++;
            }
            if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
                size_t save = pos_;
                pos_++;
                if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) pos_++;
                if (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
                    integer = false;
                    while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) pos_++;
                } else {
                    pos_ = save;
                }
            }
            t.type = Tok::Number;
            t.text = s_.substr(start, pos_ - start);
            t.number = std::strtod(t.text.c_str(), nullptr);
            t.integer = integer;
            return t;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = pos_;
            while (pos_ < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '_')) pos_++;
            t.type = Tok::Ident;
            t.text = s_.substr(start, pos_ - start);
            return t;
        }
        if (c == '"') {
            pos_++;
            std::string out;
            while (pos_ < s_.size() && s_[pos_] != '"') {
                if (s_[pos_] == '\\' && pos_ + 1 < s_.size()) {
                    char n = s_[pos_ + 1];
                    out += n == 'n' ? '\n' : n == 't' ? '\t' : n;
                    pos_ += 2;
                } else {
                    out += s_[pos_++];
                }
            }
            if (pos_ >= s_.size()) throw ExpressionError("unterminated string literal");
            pos_++;
            t.type = Tok::String;
            t.text = out;
            return t;
        }
        static const char* ops[] = {"<<", ">>", "<=", ">=", "==", "!=", "+=", "-=", "*=", "/=", "&&", "||",
                                    "+", "-", "*", "/", "^", "<", ">", "=", "?", ":", "(", ")", "{", "}", "[", "]",
                                    ",", ";", "&", "|", "!"};
        for (const char* op : ops) {
            size_t n = std::strlen(op);
            if (s_.compare(pos_, n, op) == 0) {
                t.type = Tok::Op;
                t.text = op;
                pos_ += n;
                return t;
            }
        }
        throw ExpressionError(std::string("unexpected character '") + c + "'");
    }

    const std::string& s_;
    size_t pos_ = 0;
};

}  // namespace

// ------------------------------------------------------------------ AST

struct ExpressionEvaluator::Node {
    enum Kind { Literal, Variable, Unary, Binary, Ternary, Assign, Call, ArrayLit, Index, Cast };
    Kind kind = Literal;
    std::string op;  // operator, function name, variable name or cast type
    Value literal;
    std::vector<std::unique_ptr<Node>> children;
};

namespace {

using Node = ExpressionEvaluator::Node;
using NodePtr = std::unique_ptr<Node>;

NodePtr makeNode(Node::Kind kind, std::string op = "") {
    auto n = std::make_unique<Node>();
    n->kind = kind;
    n->op = std::move(op);
    return n;
}

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    // expression-list := expr (';' expr)*
    std::vector<NodePtr> parseAll() {
        std::vector<NodePtr> result;
        while (true) {
            if (peek().type == Tok::End) break;
            result.push_back(parseAssignment());
            if (peek().type == Tok::Op && peek().text == ";") {
                advance();
                continue;
            }
            if (peek().type != Tok::End) throw ExpressionError("unexpected token '" + peek().text + "'");
        }
        return result;
    }

private:
    const Token& peek(size_t ahead = 0) const { return tokens_[std::min(pos_ + ahead, tokens_.size() - 1)]; }
    Token advance() { return tokens_[pos_ < tokens_.size() - 1 ? pos_++ : pos_]; }
    bool isOp(const char* op, size_t ahead = 0) const { return peek(ahead).type == Tok::Op && peek(ahead).text == op; }
    bool isIdent(const char* name) const { return peek().type == Tok::Ident && peek().text == name; }
    void expect(const char* op) {
        if (!isOp(op)) throw ExpressionError(std::string("expected '") + op + "' but found '" + peek().text + "'");
        advance();
    }

    NodePtr parseAssignment() {
        if (peek().type == Tok::Ident && peek(1).type == Tok::Op) {
            const std::string& op = peek(1).text;
            if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=") {
                std::string name = advance().text;
                advance();
                auto n = makeNode(Node::Assign, op);
                n->children.push_back(makeNode(Node::Variable, name));
                n->children.push_back(parseAssignment());
                return n;
            }
        }
        return parseTernary();
    }

    NodePtr parseTernary() {
        NodePtr cond = parseOr();
        if (isOp("?")) {
            advance();
            auto n = makeNode(Node::Ternary);
            n->children.push_back(std::move(cond));
            n->children.push_back(parseAssignment());
            expect(":");
            n->children.push_back(parseAssignment());
            return n;
        }
        return cond;
    }

    NodePtr binaryChain(NodePtr (Parser::*next)(), std::initializer_list<const char*> ops, bool identOps = false) {
        NodePtr left = (this->*next)();
        while (true) {
            std::string matched;
            for (const char* op : ops) {
                if ((peek().type == Tok::Op && peek().text == op) || (identOps && peek().type == Tok::Ident && peek().text == op)) {
                    matched = op;
                    break;
                }
            }
            if (matched.empty()) return left;
            advance();
            auto n = makeNode(Node::Binary, matched);
            n->children.push_back(std::move(left));
            n->children.push_back((this->*next)());
            left = std::move(n);
        }
    }

    NodePtr parseOr() { return binaryChain(&Parser::parseXor, {"or", "||"}, true); }
    NodePtr parseXor() { return binaryChain(&Parser::parseAnd, {"xor"}, true); }
    NodePtr parseAnd() { return binaryChain(&Parser::parseBitOr, {"and", "&&"}, true); }
    NodePtr parseBitOr() { return binaryChain(&Parser::parseBitAnd, {"|"}); }
    NodePtr parseBitAnd() { return binaryChain(&Parser::parseEquality, {"&"}); }
    NodePtr parseEquality() { return binaryChain(&Parser::parseComparison, {"==", "!="}); }
    NodePtr parseComparison() { return binaryChain(&Parser::parseShift, {"<", ">", "<=", ">="}); }
    NodePtr parseShift() { return binaryChain(&Parser::parseAdditive, {"<<", ">>"}); }
    NodePtr parseAdditive() { return binaryChain(&Parser::parseMultiplicative, {"+", "-"}); }
    NodePtr parseMultiplicative() { return binaryChain(&Parser::parseUnary, {"*", "/"}); }

    NodePtr parseUnary() {
        if (isOp("-") || isOp("+") || isOp("!") || isIdent("not")) {
            std::string op = advance().text;
            auto n = makeNode(Node::Unary, op == "not" ? "!" : op);
            n->children.push_back(parseUnary());
            return n;
        }
        // casts: (int) / (float)
        if (isOp("(") && peek(1).type == Tok::Ident && (peek(1).text == "int" || peek(1).text == "float") && peek(2).type == Tok::Op && peek(2).text == ")") {
            advance();
            std::string type = advance().text;
            advance();
            auto n = makeNode(Node::Cast, type);
            n->children.push_back(parseUnary());
            return n;
        }
        return parsePower();
    }

    NodePtr parsePower() {
        NodePtr base = parsePostfix();
        if (isOp("^")) {
            advance();
            auto n = makeNode(Node::Binary, "^");
            n->children.push_back(std::move(base));
            n->children.push_back(parseUnary());  // right associative, binds tighter than unary minus on the left
            return n;
        }
        return base;
    }

    NodePtr parsePostfix() {
        NodePtr node = parsePrimary();
        while (isOp("[")) {
            advance();
            auto n = makeNode(Node::Index);
            n->children.push_back(std::move(node));
            n->children.push_back(parseAssignment());
            expect("]");
            node = std::move(n);
        }
        return node;
    }

    NodePtr parsePrimary() {
        Token t = peek();
        if (t.type == Tok::Number) {
            advance();
            auto n = makeNode(Node::Literal);
            n->literal = Value::fromNumber(t.number, t.integer);
            return n;
        }
        if (t.type == Tok::String) {
            advance();
            auto n = makeNode(Node::Literal);
            n->literal = Value::fromString(t.text);
            return n;
        }
        if (t.type == Tok::Ident) {
            advance();
            if (t.text == "true" || t.text == "false") {
                auto n = makeNode(Node::Literal);
                n->literal = Value::fromBool(t.text == "true");
                return n;
            }
            if (isOp("(")) {
                advance();
                auto n = makeNode(Node::Call, t.text);
                if (!isOp(")")) {
                    while (true) {
                        n->children.push_back(parseAssignment());
                        if (isOp(",")) {
                            advance();
                            continue;
                        }
                        break;
                    }
                }
                expect(")");
                return n;
            }
            return makeNode(Node::Variable, t.text);
        }
        if (t.type == Tok::Op && t.text == "(") {
            advance();
            NodePtr inner = parseAssignment();
            expect(")");
            return inner;
        }
        if (t.type == Tok::Op && t.text == "{") {
            advance();
            auto n = makeNode(Node::ArrayLit);
            if (!isOp("}")) {
                while (true) {
                    n->children.push_back(parseAssignment());
                    if (isOp(",")) {
                        advance();
                        continue;
                    }
                    break;
                }
            }
            expect("}");
            return n;
        }
        if (t.type == Tok::End) throw ExpressionError("unexpected end of expression");
        throw ExpressionError("unexpected token '" + t.text + "'");
    }

    std::vector<Token> tokens_;
    size_t pos_ = 0;
};

bool bothIntegers(const Value& a, const Value& b) {
    return a.type == Value::Number && b.type == Value::Number && a.isInteger && b.isInteger;
}

}  // namespace

// ------------------------------------------------------------ evaluator

ExpressionEvaluator::ExpressionEvaluator() {
    defineConstant("e", Value::fromNumber(M_E));
    defineConstant("pi", Value::fromNumber(M_PI));
}

void ExpressionEvaluator::clearVariables() { variables_.clear(); }

void ExpressionEvaluator::clearConstants() {
    constants_.clear();
    defineConstant("e", Value::fromNumber(M_E));
    defineConstant("pi", Value::fromNumber(M_PI));
}

void ExpressionEvaluator::defineConstant(const std::string& name, Value value) { constants_[name] = std::move(value); }

Value ExpressionEvaluator::evaluate(const std::string& expression) {
    Lexer lexer(expression);
    Parser parser(lexer.run());
    std::vector<NodePtr> nodes = parser.parseAll();
    if (nodes.empty()) throw ExpressionError("empty expression");
    Value result;
    for (const NodePtr& n : nodes) result = eval(*n);
    return result;
}

Value ExpressionEvaluator::eval(const Node& node) {
    switch (node.kind) {
        case Node::Literal: return node.literal;
        case Node::Variable: {
            auto v = variables_.find(node.op);
            if (v != variables_.end()) return v->second;
            auto c = constants_.find(node.op);
            if (c != constants_.end()) return c->second;
            throw ExpressionError("unknown variable or constant \"" + node.op + "\"");
        }
        case Node::Assign: {
            const std::string& name = node.children[0]->op;
            if (constants_.count(name)) throw ExpressionError("cannot assign to constant \"" + name + "\"");
            Value rhs = eval(*node.children[1]);
            if (node.op != "=") {
                auto it = variables_.find(name);
                if (it == variables_.end()) throw ExpressionError("unknown variable \"" + name + "\"");
                Value lhs = it->second;
                bool integer = bothIntegers(lhs, rhs);
                double a = lhs.toNumber(), b = rhs.toNumber();
                if (node.op == "+=") {
                    if (lhs.type == Value::String || rhs.type == Value::String) rhs = Value::fromString(lhs.toString() + rhs.toString());
                    else rhs = Value::fromNumber(a + b, integer);
                } else if (node.op == "-=") rhs = Value::fromNumber(a - b, integer);
                else if (node.op == "*=") rhs = Value::fromNumber(a * b, integer);
                else if (node.op == "/=") rhs = Value::fromNumber(a / b, false);
            }
            variables_[name] = rhs;
            return rhs;
        }
        case Node::Ternary: return eval(*node.children[0]).toBool() ? eval(*node.children[1]) : eval(*node.children[2]);
        case Node::Unary: {
            Value v = eval(*node.children[0]);
            if (node.op == "-") return Value::fromNumber(-v.toNumber(), v.type == Value::Number && v.isInteger);
            if (node.op == "+") return v;
            return Value::fromBool(!v.toBool());
        }
        case Node::Cast: {
            Value v = eval(*node.children[0]);
            if (node.op == "int") return Value::fromNumber(std::trunc(v.toNumber()), true);
            return Value::fromNumber(v.toNumber(), false);
        }
        case Node::ArrayLit: {
            std::vector<Value> items;
            for (const NodePtr& c : node.children) items.push_back(eval(*c));
            return Value::fromArray(std::move(items));
        }
        case Node::Index: {
            Value arr = eval(*node.children[0]);
            Value idx = eval(*node.children[1]);
            long i = static_cast<long>(idx.toNumber());
            if (arr.type == Value::String) {
                if (i < 0 || i >= static_cast<long>(arr.string.size())) throw ExpressionError("string index out of range");
                return Value::fromString(std::string(1, arr.string[static_cast<size_t>(i)]));
            }
            if (arr.type != Value::Array) throw ExpressionError("index applied to a non-array value");
            if (i < 0 || i >= static_cast<long>(arr.array.size())) throw ExpressionError("array index out of range");
            return arr.array[static_cast<size_t>(i)];
        }
        case Node::Call: {
            std::vector<Value> args;
            for (const NodePtr& c : node.children) args.push_back(eval(*c));
            return callFunction(node.op, args);
        }
        case Node::Binary: {
            const std::string& op = node.op;
            if (op == "and" || op == "&&") return Value::fromBool(eval(*node.children[0]).toBool() && eval(*node.children[1]).toBool());
            if (op == "or" || op == "||") return Value::fromBool(eval(*node.children[0]).toBool() || eval(*node.children[1]).toBool());
            Value a = eval(*node.children[0]);
            Value b = eval(*node.children[1]);
            if (op == "xor") return Value::fromBool(a.toBool() != b.toBool());
            if (op == "+") {
                if (a.type == Value::String || b.type == Value::String) return Value::fromString(a.toString() + b.toString());
                return Value::fromNumber(a.toNumber() + b.toNumber(), bothIntegers(a, b));
            }
            if (op == "==" || op == "!=") {
                bool equal;
                if (a.type == Value::String || b.type == Value::String) equal = a.toString() == b.toString();
                else if (a.type == Value::Array || b.type == Value::Array) equal = a.toString() == b.toString();
                else equal = a.toNumber() == b.toNumber();
                return Value::fromBool(op == "==" ? equal : !equal);
            }
            if (a.type == Value::String && b.type == Value::String) {
                int cmp = a.string.compare(b.string);
                if (op == "<") return Value::fromBool(cmp < 0);
                if (op == ">") return Value::fromBool(cmp > 0);
                if (op == "<=") return Value::fromBool(cmp <= 0);
                if (op == ">=") return Value::fromBool(cmp >= 0);
            }
            double x = a.toNumber(), y = b.toNumber();
            bool integer = bothIntegers(a, b);
            if (op == "-") return Value::fromNumber(x - y, integer);
            if (op == "*") return Value::fromNumber(x * y, integer);
            if (op == "/") {
                if (y == 0 && integer) throw ExpressionError("division by zero");
                return Value::fromNumber(x / y, false);
            }
            if (op == "^") return Value::fromNumber(std::pow(x, y), integer && y >= 0);
            if (op == "<") return Value::fromBool(x < y);
            if (op == ">") return Value::fromBool(x > y);
            if (op == "<=") return Value::fromBool(x <= y);
            if (op == ">=") return Value::fromBool(x >= y);
            long long xi = static_cast<long long>(x), yi = static_cast<long long>(y);
            if (op == "&") return Value::fromNumber(static_cast<double>(xi & yi), true);
            if (op == "|") return Value::fromNumber(static_cast<double>(xi | yi), true);
            if (op == "<<") return Value::fromNumber(static_cast<double>(xi << yi), true);
            if (op == ">>") return Value::fromNumber(static_cast<double>(xi >> yi), true);
            throw ExpressionError("unknown operator " + op);
        }
    }
    throw ExpressionError("invalid expression node");
}

Value ExpressionEvaluator::callFunction(const std::string& name, std::vector<Value>& args) {
    auto need = [&](size_t n) {
        if (args.size() != n) throw ExpressionError(name + " expects " + std::to_string(n) + " argument(s)");
    };
    auto unary = [&](double (*fn)(double)) {
        need(1);
        return Value::fromNumber(fn(args[0].toNumber()), false);
    };
    if (name == "abs") {
        need(1);
        return Value::fromNumber(std::fabs(args[0].toNumber()), args[0].type == Value::Number && args[0].isInteger);
    }
    if (name == "sin") return unary(std::sin);
    if (name == "cos") return unary(std::cos);
    if (name == "tan") return unary(std::tan);
    if (name == "sinh") return unary(std::sinh);
    if (name == "cosh") return unary(std::cosh);
    if (name == "tanh") return unary(std::tanh);
    if (name == "ln" || name == "log") return unary(std::log);
    if (name == "log10") return unary(std::log10);
    if (name == "exp") return unary(std::exp);
    if (name == "sqrt") return unary(std::sqrt);
    if (name == "min" || name == "max" || name == "sum") {
        if (args.empty()) throw ExpressionError(name + " needs at least one argument");
        double acc = args[0].toNumber();
        bool integer = args[0].type == Value::Number && args[0].isInteger;
        for (size_t i = 1; i < args.size(); i++) {
            double v = args[i].toNumber();
            integer = integer && args[i].type == Value::Number && args[i].isInteger;
            if (name == "min") acc = std::min(acc, v);
            else if (name == "max") acc = std::max(acc, v);
            else acc += v;
        }
        return Value::fromNumber(acc, integer);
    }
    if (name == "str2dbl") {
        need(1);
        return Value::fromNumber(std::strtod(args[0].toString().c_str(), nullptr), false);
    }
    if (name == "strlen") {
        need(1);
        return Value::fromNumber(static_cast<double>(args[0].toString().size()), true);
    }
    if (name == "tolower") {
        need(1);
        return Value::fromString(StringHelper::toLowerCase(args[0].toString()));
    }
    if (name == "toupper") {
        need(1);
        return Value::fromString(StringHelper::toUpperCase(args[0].toString()));
    }
    if (name == "sizeof") {
        need(1);
        if (args[0].type == Value::Array) return Value::fromNumber(static_cast<double>(args[0].array.size()), true);
        if (args[0].type == Value::String) return Value::fromNumber(static_cast<double>(args[0].string.size()), true);
        return Value::fromNumber(1, true);
    }
    if (name == "regexSearch") {
        need(2);
        std::regex re(args[0].toString());
        std::smatch m;
        std::string text = args[1].toString();
        std::vector<Value> result;
        if (std::regex_search(text, m, re))
            for (size_t i = 0; i < m.size(); i++) result.push_back(Value::fromString(m.str(i)));
        return Value::fromArray(std::move(result));
    }
    if (name == "regexReplace") {
        need(3);
        std::regex re(args[0].toString());
        return Value::fromString(std::regex_replace(args[1].toString(), re, args[2].toString()));
    }
    if (name == "readRegString" || name == "readRegDWORD")
        throw ExpressionError(name + " is not available on macOS (no Windows registry)");
    throw ExpressionError("unknown function \"" + name + "\"");
}

}  // namespace eqapo
