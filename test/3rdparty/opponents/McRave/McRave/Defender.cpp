#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Defender {

    namespace {

        constexpr tuple commands{ Command::attack };

        void updateDecision(UnitInfo& unit)
        {
            if (!unit.unit() || !unit.unit()->exists()                                                                                          // Prevent crashes            
                || unit.unit()->isLoaded()
                || unit.unit()->isLockedDown() || unit.unit()->isMaelstrommed() || unit.unit()->isStasised() || !unit.unit()->isCompleted())    // If the unit is locked down, maelstrommed, stassised, or not completed
                return;

            // Convert our commands to strings to display what the unit is doing for debugging
            map<int, string> commandNames{
                make_pair(0, "Attack")
            };

            // Iterate commands, if one is executed then don't try to execute other commands
            int height = unit.getType().height() / 2;
            int width = unit.getType().width() / 2;
            int i = Util::iterateCommands(commands, unit);
            auto startText = unit.getPosition() + Position(-4 * int(commandNames[i].length() / 2), height);
            Broodwar->drawTextMap(startText, "%c%s", Text::White, commandNames[i].c_str());
        }

        void updateFormation(UnitInfo& unit)
        {
            // Set formation to closest station chokepoint to align units to
            const auto closestStation = Stations::getClosestStationGround(unit.getPosition(), PlayerState::Self);
            if (closestStation && closestStation->getChokepoint())
                unit.setFormation(Position(closestStation->getChokepoint()->Center()));

            // Add a zone to help with engagements
            Zones::addZone(unit.getPosition(), ZoneType::Defend, 1, unit.getGroundRange());
            Zones::addZone(unit.getPosition(), ZoneType::Defend, 1, unit.getAirRange());
        }

        void updateDefenders()
        {
            // Update all my buildings
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                auto &unit = *u;

                if (unit.getRole() == Role::Defender) {
                    updateFormation(unit);
                    updateDecision(unit);
                }
            }
        }
    }

    void onFrame()
    {
        updateDefenders();
    }
}