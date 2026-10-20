
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

enum class TokenKind {
    Number, Plus, Minus, Star, LeftParen, RightParen, End
};

struct Token {
    TokenKind kind;
    std::string text;
};

class Lexer {
    std::string source;
    size_t pos = 0;

public:
    explicit Lexer(const std::string& input) : source(input) {}

    std::vector<Token> scan() {
        std::vector<Token> tokens;

        while (pos < source.size()) {
            char c = source[pos];

            if (std::isspace(static_cast<unsigned char>(c))) {
                ++pos;
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(c))) {
                size_t start = pos;
                while (pos < source.size() &&
                       std::isdigit(static_cast<unsigned char>(source[pos]))) {
                    ++pos;
                }
                tokens.push_back({
                    TokenKind::Number,
                    source.substr(start, pos - start)
                });
                continue;
            }

            TokenKind kind;
            switch (c) {
                case '+': kind = TokenKind::Plus; break;
                case '-': kind = TokenKind::Minus; break;
                case '*': kind = TokenKind::Star; break;
                case '(': kind = TokenKind::LeftParen; break;
                case ')': kind = TokenKind::RightParen; break;
                default:
                    throw std::runtime_error(
                        std::string("Unexpected character: ") + c);
            }

            tokens.push_back({kind, std::string(1, c)});
            ++pos;
        }

        tokens.push_back({TokenKind::End, ""});
        return tokens;
    }
};

struct Expr {
    enum class Kind { Number, Binary, Unary };

    Kind kind;
    int value = 0;
    char op = 0;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;

    explicit Expr(int number) : kind(Kind::Number), value(number) {}

    Expr(char operation, std::unique_ptr<Expr> operand)
        : kind(Kind::Unary), op(operation), left(std::move(operand)) {}

    Expr(char operation, std::unique_ptr<Expr> lhs,
         std::unique_ptr<Expr> rhs)
        : kind(Kind::Binary), op(operation),
          left(std::move(lhs)), right(std::move(rhs)) {}
};

class Parser {
    std::vector<Token> tokens;
    size_t pos = 0;

    const Token& current() const {
        return tokens[pos];
    }

    bool match(TokenKind kind) {
        if (current().kind != kind) return false;
        ++pos;
        return true;
    }

    std::unique_ptr<Expr> primary() {
        if (current().kind == TokenKind::Number) {
            std::string text = current().text;
            ++pos;

            long long value = std::stoll(text);
            if (value > 2147483647LL) {
                throw std::runtime_error(
                    "Integer literal exceeds supported i32 range");
            }
            return std::make_unique<Expr>(static_cast<int>(value));
        }

        if (match(TokenKind::LeftParen)) {
            auto expr = expression();
            if (!match(TokenKind::RightParen)) {
                throw std::runtime_error("Expected ')'");
            }
            return expr;
        }

        throw std::runtime_error("Expected an integer or '('");
    }

    std::unique_ptr<Expr> unary() {
        if (match(TokenKind::Minus)) {
            return std::make_unique<Expr>('-', unary());
        }
        if (match(TokenKind::Plus)) {
            return unary();
        }
        return primary();
    }

    std::unique_ptr<Expr> term() {
        auto lhs = unary();

        while (current().kind == TokenKind::Star) {
            ++pos;
            auto rhs = unary();
            lhs = std::make_unique<Expr>(
                '*', std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

    std::unique_ptr<Expr> expression() {
        auto lhs = term();

        while (current().kind == TokenKind::Plus ||
               current().kind == TokenKind::Minus) {
            char op = current().kind == TokenKind::Plus ? '+' : '-';
            ++pos;
            auto rhs = term();
            lhs = std::make_unique<Expr>(
                op, std::move(lhs), std::move(rhs));
        }
        return lhs;
    }

public:
    explicit Parser(std::vector<Token> input)
        : tokens(std::move(input)) {}

    std::unique_ptr<Expr> parse() {
        auto root = expression();
        if (current().kind != TokenKind::End) {
            throw std::runtime_error("Unexpected token: " +
                                     current().text);
        }
        return root;
    }
};

class PtxEmitter {
    int nextReg = 1;
    std::vector<std::string> instructions;

    std::string reg() {
        return "%r" + std::to_string(nextReg++);
    }

    std::string emitExpr(const Expr& expr) {
        if (expr.kind == Expr::Kind::Number) {
            std::string result = reg();
            instructions.push_back(
                "    mov.s32 " + result + ", " +
                std::to_string(expr.value) + ";");
            return result;
        }

        if (expr.kind == Expr::Kind::Unary) {
            std::string operand = emitExpr(*expr.left);
            std::string result = reg();
            instructions.push_back(
                "    neg.s32 " + result + ", " + operand + ";");
            return result;
        }

        std::string lhs = emitExpr(*expr.left);
        std::string rhs = emitExpr(*expr.right);
        std::string result = reg();

        std::string instruction;
        if (expr.op == '+') instruction = "add.s32";
        else if (expr.op == '-') instruction = "sub.s32";
        else instruction = "mul.lo.s32";

        instructions.push_back(
            "    " + instruction + " " + result + ", " +
            lhs + ", " + rhs + ";");

        return result;
    }

public:
    std::string generate(const Expr& root) {
        instructions.clear();
        nextReg = 1;

        std::string result = emitExpr(root);

        std::ostringstream out;
        out << ".version 7.0\n"
            << ".target sm_52\n"
            << ".address_size 64\n\n"
            << ".visible .entry miniptx_kernel(\n"
            << "    .param .u64 out_ptr\n"
            << ") {\n"
            << "    .reg .b64 %rd<2>;\n"
            << "    .reg .s32 %r<" << nextReg << ">;\n\n";

        for (const auto& instruction : instructions) {
            out << instruction << '\n';
        }

        out << "\n"
            << "    ld.param.u64 %rd1, [out_ptr];\n"
            << "    st.global.s32 [%rd1], " << result << ";\n"
            << "    ret;\n"
            << "}\n";

        return out.str();
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: miniptx \"expression\"\n";
        return 1;
    }

    std::string source;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) source += ' ';
        source += argv[i];
    }

    try {
        Lexer lexer(source);
        Parser parser(lexer.scan());
        auto ast = parser.parse();

        PtxEmitter emitter;
        std::cout << emitter.generate(*ast);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "MiniPTX error: " << error.what() << '\n';
        return 1;
    }
}
