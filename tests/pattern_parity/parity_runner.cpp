// Renders every test case (*.csv) with the C++ pattern generator and writes
// <case>.bin next to it in the output directory, for comparison with the
// MATLAB reference (run_reference.m). Case format: see test_cases.h.
//
// Usage: parity_runner <case dir> <output dir>

#include "test_cases.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace PatternGen;

int main (int argc, char** argv)
{
    if (argc != 3)
    {
        std::fprintf (stderr, "usage: parity_runner <case dir> <output dir>\n");
        return 2;
    }

    const fs::path caseDir = argv[1];
    const fs::path outDir = argv[2];
    fs::create_directories (outDir);

    int written = 0;

    for (const auto& entry : fs::directory_iterator (caseDir))
    {
        if (entry.path().extension() != ".csv")
            continue;

        TestCase tc;
        std::string error;
        if (! readTestCase (entry.path().string(), tc, error))
        {
            std::fprintf (stderr, "%s\n", error.c_str());
            return 1;
        }

        PatternData pattern;
        if (! renderPattern (tc.waves, tc.duration, pattern, error))
        {
            std::fprintf (stderr, "%s: %s\n", entry.path().string().c_str(), error.c_str());
            return 1;
        }

        const auto bytes = encodePatternFile (pattern, tc.mapping);
        std::ofstream out (outDir / entry.path().stem().concat (".bin"), std::ios::binary);
        out.write ((const char*) bytes.data(), (std::streamsize) bytes.size());
        ++written;
    }

    std::printf ("C++: wrote %d pattern files to %s\n", written, outDir.string().c_str());
    return 0;
}
