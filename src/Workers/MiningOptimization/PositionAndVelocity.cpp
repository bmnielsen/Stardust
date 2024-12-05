#include "PositionAndVelocity.h"

bool PositionAndVelocity::tryParse(const std::string &str, PositionAndVelocity &out)
{
    std::stringstream stream(str);
    std::string item;

    int i = 0;
    for (; i < 6 && std::getline(stream, item, ' '); i++)
    {
        switch (i)
        {
            case 0:
                out.x = (uint16_t)std::stoul(item.substr(3));
                break;
            case 1:
                out.y = (uint16_t)std::stoul(item.substr(2));
                break;
            case 2:
                out.dx = (int8_t)std::stoi(item.substr(3));
                break;
            case 3:
                out.dy = (int8_t)std::stoi(item.substr(3));
                break;
            case 4:
                out.heading = (uint8_t)std::stoul(item.substr(2));
                break;
            case 5:
                out.previousPositionsHash = (uint16_t)std::stoul(item.substr(2, item.size() - 3), nullptr, 16);
                break;
        }
    }

    return i == 6;
}

std::ostream &operator<<(std::ostream &os, const PositionAndVelocity &positionAndVelocity)
{
    // Back up flags so we don't permanently change the stream's formatting
    auto flags = os.flags();

    os << "(x=" << positionAndVelocity.x
       << " y=" << positionAndVelocity.y
       << " dx=" << (int)positionAndVelocity.dx
       << " dy=" << (int)positionAndVelocity.dy
       << " h=" << (unsigned int)positionAndVelocity.heading
       << " p=" << std::hex << positionAndVelocity.previousPositionsHash
       << ")";

    os.flags(flags);
    return os;
}
