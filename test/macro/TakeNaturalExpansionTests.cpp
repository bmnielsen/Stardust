#include "BWTest.h"
#include "DoNothingModule.h"

#include "Strategist.h"
#include "Plays/Macro/TakeNaturalExpansion.h"

TEST(TakeNaturalExpansion, CanTakeNaturalExpansionImmediately)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []() {
        return new DoNothingModule();
    };
    test.map = "maps/sscai/(4)Andromeda.scx";
    test.frameLimit = 5000;

    test.onStartMine = []() {
        std::cout << "onStartMine" << std::endl;
        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TakeNaturalExpansion>());
        Strategist::setOpening(openingPlays);
    };

    test.run();
}