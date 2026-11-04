/*
    ------------------------------------------------------------------

    Formula waveforms: a small MATLAB-flavoured expression language that
    gives a block's current (uA) as a function of time.

    Variables
        t    time since the block starts (s)
        T    block length (s)
        fs   sample rate (20000)
    Constants
        pi, e
    Operators (MATLAB precedence; ^ is left associative, -2^2 = -4)
        + - * / ^   .* ./ .^   < <= > >= == ~= !=   && || ~ !
        comparisons and logic give 1 or 0
    Functions
        sin cos tan asin acos atan atan2(y,x) sinh cosh tanh
        exp log log2 log10 sqrt abs sign floor ceil round fix
        min(a,b) max(a,b) mod(a,b) rem(a,b) hypot(a,b) clamp(x,lo,hi)
        if(cond,a,b)  step(x) (1 for x >= 0)
        square(x) square(x,duty) sawtooth(x)   (Signal Processing Toolbox)

    mod, rem, round, square and sawtooth behave exactly like MATLAB's, so a
    formula written like a built-in waveform renders the same bytes.

    ------------------------------------------------------------------
*/

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace PatternGen
{

class Formula
{
public:
    /** Parses an expression. Returns nullptr and sets error (with the
        position) if it is not valid. */
    static std::unique_ptr<Formula> compile (const std::string& expression, std::string& error);

    ~Formula();

    /** Value at time t (s since block start) for a block of length T (s). */
    double evaluate (double t, double T) const;

    struct Node;

private:
    Formula() = default;

    std::vector<Node> nodes;
    int root = -1;

    double eval (int index, double t, double T) const;

    friend class FormulaParser;
};

} // namespace PatternGen
