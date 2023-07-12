#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Combat::Decision {

    constexpr tuple commands{ Command::misc, Command::special, Command::attack, Command::approach, Command::kite, Command::defend, Command::explore, Command::escort, Command::retreat, Command::move };

    void update(UnitInfo& unit)
    {
        if (!unit.unit() || !unit.unit()->exists()                                                                                          // Prevent crashes            
            || unit.unit()->isLoaded()
            || unit.unit()->isLockedDown() || unit.unit()->isMaelstrommed() || unit.unit()->isStasised() || !unit.unit()->isCompleted())    // If the unit is locked down, maelstrommed, stassised, or not completed
            return;

        // Convert our commands to strings to display what the unit is doing for debugging
        map<int, string> commandNames{
            make_pair(0, "Misc"),
            make_pair(1, "Special"),
            make_pair(2, "Attack"),
            make_pair(3, "Approach"),
            make_pair(4, "Kite"),
            make_pair(5, "Defend"),
            make_pair(6, "Explore"),
            make_pair(7, "Escort"),
            make_pair(8, "Retreat"),
            make_pair(9, "Move")
        };

        // Iterate commands, if one is executed then don't try to execute other commands
        int height = unit.getType().height() / 2;
        int width = unit.getType().width() / 2;
        int i = Util::iterateCommands(commands, unit);
        auto startText = unit.getPosition() + Position(-4 * int(commandNames[i].length() / 2), height);
        Broodwar->drawTextMap(startText, "%c%s", Text::White, commandNames[i].c_str());
    }
}