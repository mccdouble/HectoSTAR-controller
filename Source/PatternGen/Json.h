/*
    ------------------------------------------------------------------

    Minimal JSON reader/writer for pattern design files. Numbers are written
    with full double precision and parsed independently of the C locale, so
    a saved pattern reloads bit for bit.

    ------------------------------------------------------------------
*/

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace PatternGen
{
namespace json
{

struct Value
{
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::string string;
    std::vector<Value> array;
    std::vector<std::pair<std::string, Value>> object; // keeps key order

    Value() = default;
    static Value makeNumber (double v);
    static Value makeBool (bool v);
    static Value makeString (std::string v);
    static Value makeArray();
    static Value makeObject();

    bool isNull() const { return type == Type::Null; }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }

    /** Object member, or nullptr. */
    const Value* get (const std::string& key) const;

    /** Adds (or replaces) an object member and returns it. */
    Value& set (const std::string& key, Value v);

    /** Number/string/bool members with a default when missing or the wrong type
        (null reads as NaN for numbers). */
    double getNumber (const std::string& key, double fallback) const;
    std::string getString (const std::string& key, const std::string& fallback) const;
    bool getBool (const std::string& key, bool fallback) const;
};

/** Parses text. On failure returns false and sets error with line/column. */
bool parse (const std::string& text, Value& out, std::string& error);

/** Pretty-prints with 2-space indents; arrays of plain numbers stay on one line. */
std::string write (const Value& value);

} // namespace json
} // namespace PatternGen
