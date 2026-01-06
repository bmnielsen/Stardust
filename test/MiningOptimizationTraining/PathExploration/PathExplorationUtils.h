#pragma once

#include <iterator>

namespace PathExplorationUtils
{
    bool detectCollision(auto positions)
    {
        int stallFrames = 0;
        int maxStallFrames = 0;
        for (auto it = positions.begin() + 1; it != positions.end() && std::distance(positions.begin(), it) < 13; it++)
        {
            if ((*it).pos() == (*(it - 1)).pos())
            {
                stallFrames++;
                if (stallFrames > maxStallFrames)
                {
                    maxStallFrames = stallFrames;
                }
            }
            else
            {
                stallFrames = 0;
            }
        }
        return (maxStallFrames > 7);
    }
}
