#include "PositionAndVelocity.h"

bool PositionAndVelocity::isStableArrivalPosition(
        const std::vector<std::shared_ptr<const PositionAndVelocity>> &positions,
        std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator positionIt)
{
    // Guard against invalid input
    if (positionIt == positions.end()) return false;

    // If mining or delivery started on the next frame, this is by definition an arrival position
    if ((positionIt + 1) == positions.end()) return true;

    // We now examine the headings to determine whether this is a stable arrival position
    // There are two general patterns we see for stable arrivals:
    // - Worker turns towards the target on arrival, then waits and performs its action at the same heading
    // - Worker keeps its movement heading on arrival, then turns towards the target when it performs its action
    // For unstable arrivals, the worker changes headings multiple times, then turns towards the target when it performs its action
    // We can therefore determine when the arrival is stable by checking whether the heading changes more than once
    auto lastHeading = (*positionIt)->heading;
    int headingChanges = 0;
    for (auto it = (positionIt + 1); it != positions.end(); it++)
    {
        if ((*it)->heading != lastHeading) headingChanges++;
        lastHeading = (*it)->heading;
    }
    return headingChanges <= 1;
}

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
