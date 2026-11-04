/*
    ------------------------------------------------------------------

    Formula waveforms. See Formula.h.

    ------------------------------------------------------------------
*/

#include "Formula.h"
#include "PatternGenerator.h"

#include <cctype>
#include <cmath>
#include <cstring>
#include <locale>
#include <sstream>

namespace PatternGen
{

namespace
{

enum class Op
{
    Number,
    VarT,
    VarLength,
    VarFs,
    Neg,
    Not,
    Add,
    Sub,
    Mul,
    Div,
    Pow,
    Lt,
    Le,
    Gt,
    Ge,
    Eq,
    Ne,
    And,
    Or,
    Call
};

enum class Fn
{
    Sin, Cos, Tan, Asin, Acos, Atan, Atan2, Sinh, Cosh, Tanh,
    Exp, Log, Log2, Log10, Sqrt, Abs, Sign, Floor, Ceil, Round, Fix,
    Min, Max, Mod, Rem, Hypot, Clamp, If, Step, Square, Sawtooth
};

struct FnInfo
{
    const char* name;
    Fn fn;
    int minArgs;
    int maxArgs;
};

constexpr FnInfo kFunctions[] = {
    { "sin", Fn::Sin, 1, 1 }, { "cos", Fn::Cos, 1, 1 }, { "tan", Fn::Tan, 1, 1 },
    { "asin", Fn::Asin, 1, 1 }, { "acos", Fn::Acos, 1, 1 }, { "atan", Fn::Atan, 1, 1 },
    { "atan2", Fn::Atan2, 2, 2 }, { "sinh", Fn::Sinh, 1, 1 }, { "cosh", Fn::Cosh, 1, 1 },
    { "tanh", Fn::Tanh, 1, 1 }, { "exp", Fn::Exp, 1, 1 }, { "log", Fn::Log, 1, 1 },
    { "log2", Fn::Log2, 1, 1 }, { "log10", Fn::Log10, 1, 1 }, { "sqrt", Fn::Sqrt, 1, 1 },
    { "abs", Fn::Abs, 1, 1 }, { "sign", Fn::Sign, 1, 1 }, { "floor", Fn::Floor, 1, 1 },
    { "ceil", Fn::Ceil, 1, 1 }, { "round", Fn::Round, 1, 1 }, { "fix", Fn::Fix, 1, 1 },
    { "min", Fn::Min, 2, 2 }, { "max", Fn::Max, 2, 2 }, { "mod", Fn::Mod, 2, 2 },
    { "rem", Fn::Rem, 2, 2 }, { "hypot", Fn::Hypot, 2, 2 }, { "clamp", Fn::Clamp, 3, 3 },
    { "if", Fn::If, 3, 3 }, { "step", Fn::Step, 1, 1 }, { "square", Fn::Square, 1, 2 },
    { "sawtooth", Fn::Sawtooth, 1, 1 },
};

} // namespace

struct Formula::Node
{
    Op op = Op::Number;
    double value = 0;
    Fn fn = Fn::Sin;
    int args[3] = { -1, -1, -1 };
    int numArgs = 0;
};

class FormulaParser
{
public:
    FormulaParser (const std::string& text_, Formula& f_) : text (text_), f (f_) {}

    bool parse (std::string& error)
    {
        skipSpace();
        if (pos >= text.size())
            return fail ("the formula is empty", error);

        f.root = parseOr();
        if (failed)
        {
            error = message;
            return false;
        }

        skipSpace();
        if (pos < text.size())
            return fail ("unexpected '" + std::string (1, text[pos]) + "'", error);

        return true;
    }

private:
    const std::string& text;
    Formula& f;
    std::size_t pos = 0;
    bool failed = false;
    std::string message;

    bool fail (const std::string& what, std::string& error)
    {
        error = what + " at position " + std::to_string (pos + 1);
        return false;
    }

    int error (const std::string& what)
    {
        if (! failed)
        {
            failed = true;
            message = what + " at position " + std::to_string (pos + 1);
        }
        return -1;
    }

    int add (Formula::Node n)
    {
        f.nodes.push_back (n);
        return (int) f.nodes.size() - 1;
    }

    int binary (Op op, int a, int b)
    {
        Formula::Node n;
        n.op = op;
        n.args[0] = a;
        n.args[1] = b;
        n.numArgs = 2;
        return add (n);
    }

    int unary (Op op, int a)
    {
        Formula::Node n;
        n.op = op;
        n.args[0] = a;
        n.numArgs = 1;
        return add (n);
    }

    void skipSpace()
    {
        while (pos < text.size() && std::isspace ((unsigned char) text[pos]))
            ++pos;
    }

    // Matches an operator, also accepting MATLAB's element-wise spellings (.* ./ .^)
    bool accept (const char* token)
    {
        skipSpace();
        const std::size_t len = std::strlen (token);

        if (text.compare (pos, len, token) == 0)
        {
            pos += len;
            return true;
        }

        if (len == 1 && (token[0] == '*' || token[0] == '/' || token[0] == '^')
            && pos + 1 < text.size() && text[pos] == '.' && text[pos + 1] == token[0])
        {
            pos += 2;
            return true;
        }

        return false;
    }

    bool peekIs (char c)
    {
        skipSpace();
        return pos < text.size() && text[pos] == c;
    }

    int parseOr()
    {
        int a = parseAnd();
        while (! failed && accept ("||"))
            a = binary (Op::Or, a, parseAnd());
        return a;
    }

    int parseAnd()
    {
        int a = parseCompare();
        while (! failed && accept ("&&"))
            a = binary (Op::And, a, parseCompare());
        return a;
    }

    int parseCompare()
    {
        int a = parseAdd();

        while (! failed)
        {
            if (accept ("<="))
                a = binary (Op::Le, a, parseAdd());
            else if (accept (">="))
                a = binary (Op::Ge, a, parseAdd());
            else if (accept ("=="))
                a = binary (Op::Eq, a, parseAdd());
            else if (accept ("~=") || accept ("!="))
                a = binary (Op::Ne, a, parseAdd());
            else if (accept ("<"))
                a = binary (Op::Lt, a, parseAdd());
            else if (accept (">"))
                a = binary (Op::Gt, a, parseAdd());
            else
                break;
        }

        return a;
    }

    int parseAdd()
    {
        int a = parseMul();

        while (! failed)
        {
            if (accept ("+"))
                a = binary (Op::Add, a, parseMul());
            else if (accept ("-"))
                a = binary (Op::Sub, a, parseMul());
            else
                break;
        }

        return a;
    }

    int parseMul()
    {
        int a = parseUnary();

        while (! failed)
        {
            if (accept ("*"))
                a = binary (Op::Mul, a, parseUnary());
            else if (accept ("/"))
                a = binary (Op::Div, a, parseUnary());
            else
                break;
        }

        return a;
    }

    // MATLAB: unary minus binds looser than ^, so -2^2 = -4
    int parseUnary()
    {
        if (accept ("-"))
            return unary (Op::Neg, parseUnary());
        if (accept ("+"))
            return parseUnary();
        if (! peekNotEqual() && (accept ("~") || accept ("!")))
            return unary (Op::Not, parseUnary());
        return parsePower();
    }

    bool peekNotEqual()
    {
        skipSpace();
        return pos + 1 < text.size() && (text[pos] == '~' || text[pos] == '!') && text[pos + 1] == '=';
    }

    // MATLAB: ^ is left associative (2^3^2 = 64) and its operand may carry a sign (2^-1)
    int parsePower()
    {
        int a = parsePrimary();

        while (! failed && accept ("^"))
            a = binary (Op::Pow, a, parsePowerOperand());

        return a;
    }

    int parsePowerOperand()
    {
        if (accept ("-"))
            return unary (Op::Neg, parsePowerOperand());
        if (accept ("+"))
            return parsePowerOperand();
        return parsePrimary();
    }

    int parsePrimary()
    {
        skipSpace();

        if (pos >= text.size())
            return error ("the formula ends too early");

        const char c = text[pos];

        if (c == '(')
        {
            ++pos;
            const int inner = parseOr();
            if (! failed && ! accept (")"))
                return error ("expected ')'");
            return inner;
        }

        if (std::isdigit ((unsigned char) c) || (c == '.' && pos + 1 < text.size() && std::isdigit ((unsigned char) text[pos + 1])))
            return parseNumber();

        if (std::isalpha ((unsigned char) c) || c == '_')
            return parseName();

        return error ("unexpected '" + std::string (1, c) + "'");
    }

    int parseNumber()
    {
        const std::size_t start = pos;

        while (pos < text.size() && (std::isdigit ((unsigned char) text[pos]) || text[pos] == '.'))
            ++pos;

        if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E'))
        {
            std::size_t p = pos + 1;
            if (p < text.size() && (text[p] == '+' || text[p] == '-'))
                ++p;
            if (p < text.size() && std::isdigit ((unsigned char) text[p]))
            {
                pos = p;
                while (pos < text.size() && std::isdigit ((unsigned char) text[pos]))
                    ++pos;
            }
        }

        // locale-independent, correctly rounded
        std::istringstream in (text.substr (start, pos - start));
        in.imbue (std::locale::classic());
        double v = 0;
        in >> v;

        if (in.fail() || ! in.eof())
        {
            pos = start;
            return error ("invalid number");
        }

        Formula::Node n;
        n.op = Op::Number;
        n.value = v;
        return add (n);
    }

    int parseName()
    {
        const std::size_t start = pos;
        while (pos < text.size() && (std::isalnum ((unsigned char) text[pos]) || text[pos] == '_'))
            ++pos;
        const std::string name = text.substr (start, pos - start);

        if (peekIs ('('))
        {
            const FnInfo* info = nullptr;
            for (const auto& fi : kFunctions)
                if (name == fi.name)
                    info = &fi;

            if (info == nullptr)
            {
                pos = start;
                return error ("unknown function '" + name + "'");
            }

            accept ("(");
            Formula::Node n;
            n.op = Op::Call;
            n.fn = info->fn;

            if (! peekIs (')'))
            {
                do
                {
                    if (n.numArgs == 3)
                        return error ("too many arguments to " + name);
                    n.args[n.numArgs++] = parseOr();
                } while (! failed && accept (","));
            }

            if (failed)
                return -1;
            if (! accept (")"))
                return error ("expected ')' after the arguments to " + name);
            if (n.numArgs < info->minArgs || n.numArgs > info->maxArgs)
            {
                const std::string expected = info->minArgs == info->maxArgs
                                                 ? std::to_string (info->minArgs)
                                                 : std::to_string (info->minArgs) + " or " + std::to_string (info->maxArgs);
                return error (name + " takes " + expected + " argument" + (info->maxArgs == 1 ? "" : "s"));
            }

            return add (n);
        }

        Formula::Node n;
        if (name == "t")
            n.op = Op::VarT;
        else if (name == "T")
            n.op = Op::VarLength;
        else if (name == "fs")
            n.op = Op::VarFs;
        else if (name == "pi")
            n.value = kPi;
        else if (name == "e")
            n.value = 2.718281828459045;
        else
        {
            pos = start;
            return error ("unknown name '" + name + "' (use t, T, fs, pi or e)");
        }

        return add (n);
    }
};

Formula::~Formula() = default;

std::unique_ptr<Formula> Formula::compile (const std::string& expression, std::string& error)
{
    std::unique_ptr<Formula> f (new Formula());
    FormulaParser parser (expression, *f);

    if (! parser.parse (error))
        return nullptr;

    return f;
}

double Formula::evaluate (double t, double T) const
{
    return eval (root, t, T);
}

double Formula::eval (int index, double t, double T) const
{
    const Node& n = nodes[(std::size_t) index];

    auto arg = [&] (int i) { return eval (n.args[i], t, T); };

    switch (n.op)
    {
        case Op::Number: return n.value;
        case Op::VarT: return t;
        case Op::VarLength: return T;
        case Op::VarFs: return kSampleRateHz;
        case Op::Neg: return -arg (0);
        case Op::Not: return arg (0) == 0 ? 1.0 : 0.0;
        case Op::Add: return arg (0) + arg (1);
        case Op::Sub: return arg (0) - arg (1);
        case Op::Mul: return arg (0) * arg (1);
        case Op::Div: return arg (0) / arg (1);
        case Op::Pow:
        {
            const double a = arg (0);
            const double b = arg (1);
            return b == 2.0 ? a * a : std::pow (a, b);
        }
        case Op::Lt: return arg (0) < arg (1) ? 1.0 : 0.0;
        case Op::Le: return arg (0) <= arg (1) ? 1.0 : 0.0;
        case Op::Gt: return arg (0) > arg (1) ? 1.0 : 0.0;
        case Op::Ge: return arg (0) >= arg (1) ? 1.0 : 0.0;
        case Op::Eq: return arg (0) == arg (1) ? 1.0 : 0.0;
        case Op::Ne: return arg (0) != arg (1) ? 1.0 : 0.0;
        case Op::And: return (arg (0) != 0 && arg (1) != 0) ? 1.0 : 0.0;
        case Op::Or: return (arg (0) != 0 || arg (1) != 0) ? 1.0 : 0.0;
        case Op::Call: break;
    }

    switch (n.fn)
    {
        case Fn::Sin: return std::sin (arg (0));
        case Fn::Cos: return std::cos (arg (0));
        case Fn::Tan: return std::tan (arg (0));
        case Fn::Asin: return std::asin (arg (0));
        case Fn::Acos: return std::acos (arg (0));
        case Fn::Atan: return std::atan (arg (0));
        case Fn::Atan2: return std::atan2 (arg (0), arg (1));
        case Fn::Sinh: return std::sinh (arg (0));
        case Fn::Cosh: return std::cosh (arg (0));
        case Fn::Tanh: return std::tanh (arg (0));
        case Fn::Exp: return std::exp (arg (0));
        case Fn::Log: return std::log (arg (0));
        case Fn::Log2: return std::log2 (arg (0));
        case Fn::Log10: return std::log10 (arg (0));
        case Fn::Sqrt: return std::sqrt (arg (0));
        case Fn::Abs: return std::fabs (arg (0));
        case Fn::Sign:
        {
            const double a = arg (0);
            return a > 0 ? 1.0 : (a < 0 ? -1.0 : a); // sign(NaN) = NaN, sign(0) = 0
        }
        case Fn::Floor: return std::floor (arg (0));
        case Fn::Ceil: return std::ceil (arg (0));
        case Fn::Round: return std::round (arg (0));
        case Fn::Fix: return std::trunc (arg (0));
        case Fn::Min: return std::fmin (arg (0), arg (1));
        case Fn::Max: return std::fmax (arg (0), arg (1));
        case Fn::Mod: return mod (arg (0), arg (1));
        case Fn::Rem: return rem (arg (0), arg (1));
        case Fn::Hypot: return std::hypot (arg (0), arg (1));
        case Fn::Clamp: return std::fmin (std::fmax (arg (0), arg (1)), arg (2));
        case Fn::If: return arg (0) != 0 ? arg (1) : arg (2);
        case Fn::Step: return arg (0) >= 0 ? 1.0 : 0.0;
        case Fn::Square: return n.numArgs == 2 ? square (arg (0), arg (1)) : square (arg (0));
        case Fn::Sawtooth: return sawtooth (arg (0));
    }

    return NAN;
}

} // namespace PatternGen
