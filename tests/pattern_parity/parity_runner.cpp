// Renders every test case (*.csv) with the C++ pattern generator and writes
// <case>.bin next to it in the output directory, for comparison with the
// MATLAB reference (run_reference.m).
//
// Usage: parity_runner <case dir> <output dir>
//
// Case file format (one value per field, full double precision):
//   duration,<seconds>
//   device,<4 Shank Chronic | 2 Shank Chronic | 4 Shank Acute | Demo Board>
//   <channel>,<waveType>,<freq>,<amp>,<offset>,<duty>,<delay>,<step>
//   ...

#include "../../Source/PatternGen/PatternGenerator.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace PatternGen;

static std::vector<std::string> split (const std::string& line)
{
    std::vector<std::string> fields;
    std::stringstream ss (line);
    std::string field;
    while (std::getline (ss, field, ','))
        fields.push_back (field);
    return fields;
}

static bool mappingFromName (const std::string& name, ProbeMapping& mapping)
{
    for (auto m : { ProbeMapping::FourShankChronic, ProbeMapping::TwoShankChronic, ProbeMapping::FourShankAcute, ProbeMapping::DemoBoard })
    {
        if (name == probeMappingName (m))
        {
            mapping = m;
            return true;
        }
    }
    return false;
}

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

        std::ifstream in (entry.path());
        std::string line;
        double duration = 0;
        ProbeMapping mapping = ProbeMapping::FourShankChronic;
        std::array<ChannelWaveform, kNumLeds> waves {};

        while (std::getline (in, line))
        {
            if (! line.empty() && line.back() == '\r')
                line.pop_back();

            const auto f = split (line);
            if (f.empty())
                continue;

            if (f[0] == "duration")
            {
                duration = std::strtod (f[1].c_str(), nullptr);
            }
            else if (f[0] == "device")
            {
                if (! mappingFromName (f[1], mapping))
                {
                    std::fprintf (stderr, "%s: unknown device '%s'\n", entry.path().string().c_str(), f[1].c_str());
                    return 1;
                }
            }
            else if (f.size() == 8)
            {
                const int ch = std::atoi (f[0].c_str());
                ChannelWaveform& w = waves[ch - 1];
                w.type = waveTypeFromName (f[1]);
                w.freq = std::strtod (f[2].c_str(), nullptr);
                w.amp = std::strtod (f[3].c_str(), nullptr);
                w.offset = std::strtod (f[4].c_str(), nullptr);
                w.duty = std::strtod (f[5].c_str(), nullptr);
                w.delay = std::strtod (f[6].c_str(), nullptr);
                w.step = std::strtod (f[7].c_str(), nullptr);
            }
        }

        PatternData pattern;
        std::string error;
        if (! renderPattern (waves, duration, pattern, error))
        {
            std::fprintf (stderr, "%s: %s\n", entry.path().string().c_str(), error.c_str());
            return 1;
        }

        const auto bytes = encodePatternFile (pattern, mapping);
        std::ofstream out (outDir / entry.path().stem().concat (".bin"), std::ios::binary);
        out.write ((const char*) bytes.data(), (std::streamsize) bytes.size());
        ++written;
    }

    std::printf ("C++: wrote %d pattern files to %s\n", written, outDir.string().c_str());
    return 0;
}
