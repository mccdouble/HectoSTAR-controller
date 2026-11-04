// Reads the parity test case files written by make_cases.py:
//   duration,<seconds>
//   device,<4 Shank Chronic | 2 Shank Chronic | 4 Shank Acute | Demo Board>
//   <channel>,<waveType>,<freq>,<amp>,<offset>,<duty>,<delay>,<step>

#pragma once

#include "../../Source/PatternGen/PatternGenerator.h"

#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct TestCase
{
    double duration = 0;
    PatternGen::ProbeMapping mapping = PatternGen::ProbeMapping::FourShankChronic;
    std::array<PatternGen::ChannelWaveform, PatternGen::kNumLeds> waves {};
    std::vector<int> channels; // table rows, in file order
};

inline std::vector<std::string> splitCsv (const std::string& line)
{
    std::vector<std::string> fields;
    std::stringstream ss (line);
    std::string field;
    while (std::getline (ss, field, ','))
        fields.push_back (field);
    return fields;
}

inline bool readTestCase (const std::string& path, TestCase& tc, std::string& error)
{
    using namespace PatternGen;

    std::ifstream in (path);
    if (! in)
    {
        error = "cannot open " + path;
        return false;
    }

    std::string line;
    while (std::getline (in, line))
    {
        if (! line.empty() && line.back() == '\r')
            line.pop_back();

        const auto f = splitCsv (line);
        if (f.empty())
            continue;

        if (f[0] == "duration")
        {
            tc.duration = std::strtod (f[1].c_str(), nullptr);
        }
        else if (f[0] == "device")
        {
            bool found = false;
            for (auto m : { ProbeMapping::FourShankChronic, ProbeMapping::TwoShankChronic, ProbeMapping::FourShankAcute, ProbeMapping::DemoBoard })
            {
                if (f[1] == probeMappingName (m))
                {
                    tc.mapping = m;
                    found = true;
                }
            }
            if (! found)
            {
                error = path + ": unknown device '" + f[1] + "'";
                return false;
            }
        }
        else if (f.size() == 8)
        {
            const int ch = std::atoi (f[0].c_str());
            ChannelWaveform& w = tc.waves[ch - 1];
            w.type = waveTypeFromName (f[1]);
            w.freq = std::strtod (f[2].c_str(), nullptr);
            w.amp = std::strtod (f[3].c_str(), nullptr);
            w.offset = std::strtod (f[4].c_str(), nullptr);
            w.duty = std::strtod (f[5].c_str(), nullptr);
            w.delay = std::strtod (f[6].c_str(), nullptr);
            w.step = std::strtod (f[7].c_str(), nullptr);
            tc.channels.push_back (ch);
        }
    }

    return true;
}
