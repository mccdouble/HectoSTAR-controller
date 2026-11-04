/*
    ------------------------------------------------------------------

    Minimal JSON reader/writer. See Json.h.

    ------------------------------------------------------------------
*/

#include "Json.h"

#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace PatternGen
{
namespace json
{

Value Value::makeNumber (double v)
{
    Value r;
    r.type = Type::Number;
    r.number = v;
    return r;
}

Value Value::makeBool (bool v)
{
    Value r;
    r.type = Type::Bool;
    r.boolean = v;
    return r;
}

Value Value::makeString (std::string v)
{
    Value r;
    r.type = Type::String;
    r.string = std::move (v);
    return r;
}

Value Value::makeArray()
{
    Value r;
    r.type = Type::Array;
    return r;
}

Value Value::makeObject()
{
    Value r;
    r.type = Type::Object;
    return r;
}

const Value* Value::get (const std::string& key) const
{
    if (type != Type::Object)
        return nullptr;
    for (const auto& kv : object)
        if (kv.first == key)
            return &kv.second;
    return nullptr;
}

Value& Value::set (const std::string& key, Value v)
{
    type = Type::Object;
    for (auto& kv : object)
    {
        if (kv.first == key)
        {
            kv.second = std::move (v);
            return kv.second;
        }
    }
    object.emplace_back (key, std::move (v));
    return object.back().second;
}

double Value::getNumber (const std::string& key, double fallback) const
{
    const Value* v = get (key);
    if (v == nullptr)
        return fallback;
    if (v->type == Type::Number)
        return v->number;
    if (v->type == Type::Null)
        return NAN; // NaN/Inf are written as null
    return fallback;
}

std::string Value::getString (const std::string& key, const std::string& fallback) const
{
    const Value* v = get (key);
    return (v != nullptr && v->type == Type::String) ? v->string : fallback;
}

bool Value::getBool (const std::string& key, bool fallback) const
{
    const Value* v = get (key);
    return (v != nullptr && v->type == Type::Bool) ? v->boolean : fallback;
}

namespace
{

class Parser
{
public:
    explicit Parser (const std::string& t) : text (t) {}

    bool run (Value& out, std::string& error)
    {
        skip();
        if (! parseValue (out, 0))
        {
            error = message;
            return false;
        }
        skip();
        if (pos < text.size())
        {
            fail ("unexpected text after the end of the document");
            error = message;
            return false;
        }
        return true;
    }

private:
    const std::string& text;
    std::size_t pos = 0;
    std::string message;

    bool fail (const std::string& what)
    {
        int line = 1, col = 1;
        for (std::size_t i = 0; i < pos && i < text.size(); ++i)
        {
            if (text[i] == '\n')
            {
                ++line;
                col = 1;
            }
            else
            {
                ++col;
            }
        }
        message = what + " (line " + std::to_string (line) + ", column " + std::to_string (col) + ")";
        return false;
    }

    void skip()
    {
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' || text[pos] == '\r'))
            ++pos;
    }

    bool literal (const char* word)
    {
        const std::string w (word);
        if (text.compare (pos, w.size(), w) == 0)
        {
            pos += w.size();
            return true;
        }
        return false;
    }

    bool parseValue (Value& out, int depth)
    {
        if (depth > 64)
            return fail ("nested too deeply");

        skip();
        if (pos >= text.size())
            return fail ("unexpected end of file");

        const char c = text[pos];

        if (c == '{')
            return parseObject (out, depth);
        if (c == '[')
            return parseArray (out, depth);
        if (c == '"')
        {
            out.type = Value::Type::String;
            return parseString (out.string);
        }
        if (literal ("true"))
        {
            out = Value::makeBool (true);
            return true;
        }
        if (literal ("false"))
        {
            out = Value::makeBool (false);
            return true;
        }
        if (literal ("null"))
        {
            out = Value();
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9'))
            return parseNumber (out);

        return fail (std::string ("unexpected '") + c + "'");
    }

    bool parseObject (Value& out, int depth)
    {
        out = Value::makeObject();
        ++pos; // {
        skip();
        if (pos < text.size() && text[pos] == '}')
        {
            ++pos;
            return true;
        }

        while (true)
        {
            skip();
            if (pos >= text.size() || text[pos] != '"')
                return fail ("expected a quoted key");
            std::string key;
            if (! parseString (key))
                return false;
            skip();
            if (pos >= text.size() || text[pos] != ':')
                return fail ("expected ':'");
            ++pos;
            Value v;
            if (! parseValue (v, depth + 1))
                return false;
            out.object.emplace_back (std::move (key), std::move (v));
            skip();
            if (pos < text.size() && text[pos] == ',')
            {
                ++pos;
                continue;
            }
            if (pos < text.size() && text[pos] == '}')
            {
                ++pos;
                return true;
            }
            return fail ("expected ',' or '}'");
        }
    }

    bool parseArray (Value& out, int depth)
    {
        out = Value::makeArray();
        ++pos; // [
        skip();
        if (pos < text.size() && text[pos] == ']')
        {
            ++pos;
            return true;
        }

        while (true)
        {
            Value v;
            if (! parseValue (v, depth + 1))
                return false;
            out.array.push_back (std::move (v));
            skip();
            if (pos < text.size() && text[pos] == ',')
            {
                ++pos;
                continue;
            }
            if (pos < text.size() && text[pos] == ']')
            {
                ++pos;
                return true;
            }
            return fail ("expected ',' or ']'");
        }
    }

    static void appendUtf8 (std::string& s, unsigned cp)
    {
        if (cp < 0x80)
            s += (char) cp;
        else if (cp < 0x800)
        {
            s += (char) (0xC0 | (cp >> 6));
            s += (char) (0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            s += (char) (0xE0 | (cp >> 12));
            s += (char) (0x80 | ((cp >> 6) & 0x3F));
            s += (char) (0x80 | (cp & 0x3F));
        }
        else
        {
            s += (char) (0xF0 | (cp >> 18));
            s += (char) (0x80 | ((cp >> 12) & 0x3F));
            s += (char) (0x80 | ((cp >> 6) & 0x3F));
            s += (char) (0x80 | (cp & 0x3F));
        }
    }

    bool parseHex4 (unsigned& cp)
    {
        if (pos + 4 > text.size())
            return fail ("bad \\u escape");
        cp = 0;
        for (int i = 0; i < 4; ++i)
        {
            const char h = text[pos++];
            cp <<= 4;
            if (h >= '0' && h <= '9')
                cp |= (unsigned) (h - '0');
            else if (h >= 'a' && h <= 'f')
                cp |= (unsigned) (h - 'a' + 10);
            else if (h >= 'A' && h <= 'F')
                cp |= (unsigned) (h - 'A' + 10);
            else
                return fail ("bad \\u escape");
        }
        return true;
    }

    bool parseString (std::string& out)
    {
        ++pos; // opening quote
        out.clear();

        while (pos < text.size())
        {
            const char c = text[pos++];
            if (c == '"')
                return true;
            if (c != '\\')
            {
                out += c;
                continue;
            }
            if (pos >= text.size())
                break;
            const char e = text[pos++];
            switch (e)
            {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u':
                {
                    unsigned cp;
                    if (! parseHex4 (cp))
                        return false;
                    if (cp >= 0xD800 && cp < 0xDC00 && text.compare (pos, 2, "\\u") == 0)
                    {
                        pos += 2;
                        unsigned lo;
                        if (! parseHex4 (lo))
                            return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    appendUtf8 (out, cp);
                    break;
                }
                default:
                    return fail ("bad escape in string");
            }
        }

        return fail ("unterminated string");
    }

    bool parseNumber (Value& out)
    {
        const std::size_t start = pos;
        if (text[pos] == '-')
            ++pos;
        while (pos < text.size() && ((text[pos] >= '0' && text[pos] <= '9') || text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E' || text[pos] == '+' || text[pos] == '-'))
            ++pos;

        std::istringstream in (text.substr (start, pos - start));
        in.imbue (std::locale::classic());
        double v = 0;
        in >> v;
        if (in.fail() || ! in.eof())
        {
            pos = start;
            return fail ("invalid number");
        }

        out = Value::makeNumber (v);
        return true;
    }
};

void writeString (std::ostringstream& os, const std::string& s)
{
    os << '"';
    for (const char c : s)
    {
        switch (c)
        {
            case '"': os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\n': os << "\\n"; break;
            case '\r': os << "\\r"; break;
            case '\t': os << "\\t"; break;
            default:
                if ((unsigned char) c < 0x20)
                {
                    const char* hex = "0123456789abcdef";
                    os << "\\u00" << hex[(c >> 4) & 0xF] << hex[c & 0xF];
                }
                else
                {
                    os << c;
                }
        }
    }
    os << '"';
}

void writeNumber (std::ostringstream& os, double v)
{
    if (! std::isfinite (v))
    {
        os << "null";
        return;
    }
    if (v == std::floor (v) && std::fabs (v) < 1e15)
    {
        os << (long long) v; // whole numbers without a trailing .0
        return;
    }
    os << std::setprecision (17) << v;
}

bool isFlat (const Value& v)
{
    if (v.type == Value::Type::Array || v.type == Value::Type::Object)
    {
        if (v.type == Value::Type::Object)
            return false;
        for (const auto& e : v.array)
            if (e.type == Value::Type::Array || e.type == Value::Type::Object)
                return false;
    }
    return true;
}

bool isCompactArray (const Value& v)
{
    // [1, 2, 3] and [[0, 1], [2, 3]] stay on one line
    if (v.type != Value::Type::Array)
        return false;
    for (const auto& e : v.array)
        if (! isFlat (e))
            return false;
    return true;
}

void writeValue (std::ostringstream& os, const Value& v, int indent)
{
    const std::string pad ((std::size_t) indent * 2, ' ');
    const std::string padIn ((std::size_t) (indent + 1) * 2, ' ');

    switch (v.type)
    {
        case Value::Type::Null: os << "null"; break;
        case Value::Type::Bool: os << (v.boolean ? "true" : "false"); break;
        case Value::Type::Number: writeNumber (os, v.number); break;
        case Value::Type::String: writeString (os, v.string); break;
        case Value::Type::Array:
            if (v.array.empty())
            {
                os << "[]";
            }
            else if (isCompactArray (v))
            {
                os << '[';
                for (std::size_t i = 0; i < v.array.size(); ++i)
                {
                    if (i > 0)
                        os << ", ";
                    writeValue (os, v.array[i], indent + 1);
                }
                os << ']';
            }
            else
            {
                os << "[\n";
                for (std::size_t i = 0; i < v.array.size(); ++i)
                {
                    os << padIn;
                    writeValue (os, v.array[i], indent + 1);
                    os << (i + 1 < v.array.size() ? ",\n" : "\n");
                }
                os << pad << ']';
            }
            break;
        case Value::Type::Object:
            if (v.object.empty())
            {
                os << "{}";
                break;
            }
            os << "{\n";
            for (std::size_t i = 0; i < v.object.size(); ++i)
            {
                os << padIn;
                writeString (os, v.object[i].first);
                os << ": ";
                writeValue (os, v.object[i].second, indent + 1);
                os << (i + 1 < v.object.size() ? ",\n" : "\n");
            }
            os << pad << '}';
            break;
    }
}

} // namespace

bool parse (const std::string& text, Value& out, std::string& error)
{
    Parser p (text);
    return p.run (out, error);
}

std::string write (const Value& value)
{
    std::ostringstream os;
    os.imbue (std::locale::classic());
    writeValue (os, value, 0);
    os << '\n';
    return os.str();
}

} // namespace json
} // namespace PatternGen
