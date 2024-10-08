#include "PositionAndVelocity.h"

#include <regex>

#include "CsvTools.h"

namespace
{
    const std::regex parser(R"(\(x=([0-9]+) y=([0-9]+) dx=([\-0-9]+) dy=([\-0-9]+) h=([0-9]+) p=([0-9a-f]+)\))");
}

bool PositionAndVelocity::tryParse(const std::string &str, PositionAndVelocity &out)
{
    std::smatch matches;
    if (!std::regex_search(str, matches, parser) || matches.size() != 7) return false;

    out.x = std::stoi(matches[1].str());
    out.y = std::stoi(matches[2].str());
    out.dx = std::stoi(matches[3].str());
    out.dy = std::stoi(matches[4].str());
    out.heading = std::stoi(matches[5].str());
    out.previousPositionsHash = (uint32_t)std::stoul(matches[6].str(), nullptr, 16);

    return true;
}

std::ostream &operator<<(std::ostream &os, const PositionAndVelocity &positionAndVelocity)
{
    // Back up flags so we don't permanently change the stream's formatting
    auto flags = os.flags();

    os << "(x=" << positionAndVelocity.x
       << " y=" << positionAndVelocity.y
       << " dx=" << positionAndVelocity.dx
       << " dy=" << positionAndVelocity.dy
       << " h=" << positionAndVelocity.heading
       << " p=" << std::hex << positionAndVelocity.previousPositionsHash
       << ")";

    os.flags(flags);
    return os;
}

std::ostream &operator<<(std::ostream &os, const std::vector<PositionAndVelocity> &vec)
{
    CsvTools::outputList(os, vec);
    return os;
}
