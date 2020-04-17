// For each map in the SSCAIT pool, verify that the natural location for each start location
// is the same as the old BWTA-based Locutus.
// This is a simple way of making sure basic base placement is working.

#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"

void verifyNaturalLocations(const std::string &map, std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne(map);
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    test.onStartMine = [&]()
    {
        for (auto startLocation : BWAPI::Broodwar->getStartLocations())
        {
            auto natural = Map::getNaturalForStartLocation(startLocation);
            EXPECT_NE(natural, nullptr);
            EXPECT_EQ(natural->getTilePosition(), expectedNaturalLocations[startLocation]);
        }
    };

    test.run();
}

TEST(NaturalLocations, Andromeda)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(7, 118),   BWAPI::TilePosition(21, 105)},
            {BWAPI::TilePosition(117, 7),   BWAPI::TilePosition(103, 21)},
            {BWAPI::TilePosition(117, 119), BWAPI::TilePosition(103, 105)},
            {BWAPI::TilePosition(7, 6),     BWAPI::TilePosition(21, 21)}
    };

    verifyNaturalLocations("Andromeda", expectedNaturalLocations);
}

TEST(NaturalLocations, Python)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(42, 119), BWAPI::TilePosition(75, 118)},
            {BWAPI::TilePosition(7, 86),   BWAPI::TilePosition(7, 57)},
            {BWAPI::TilePosition(117, 40), BWAPI::TilePosition(117, 66)},
            {BWAPI::TilePosition(83, 6),   BWAPI::TilePosition(47, 6)}
    };

    verifyNaturalLocations("Python", expectedNaturalLocations);
}

TEST(NaturalLocations, EmpireOfTheSun)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(117, 119), BWAPI::TilePosition(93, 118)},
            {BWAPI::TilePosition(117, 6),   BWAPI::TilePosition(93, 6)},
            {BWAPI::TilePosition(7, 119),   BWAPI::TilePosition(31, 118)},
            {BWAPI::TilePosition(7, 6),     BWAPI::TilePosition(31, 6)}
    };

    verifyNaturalLocations("Empire of the Sun.scm", expectedNaturalLocations);
}

TEST(NaturalLocations, FightingSpirit)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(117, 7),   BWAPI::TilePosition(88, 13)},
            {BWAPI::TilePosition(117, 117), BWAPI::TilePosition(110, 88)},
            {BWAPI::TilePosition(7, 6),     BWAPI::TilePosition(14, 38)},
            {BWAPI::TilePosition(7, 116),   BWAPI::TilePosition(36, 112)}
    };

    verifyNaturalLocations("Fighting Spirit", expectedNaturalLocations);
}

TEST(NaturalLocations, LaMancha)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(116, 6),   BWAPI::TilePosition(92, 7)},
            {BWAPI::TilePosition(116, 117), BWAPI::TilePosition(116, 89)},
            {BWAPI::TilePosition(7, 6),     BWAPI::TilePosition(8, 37)},
            {BWAPI::TilePosition(8, 117),   BWAPI::TilePosition(32, 117)}
    };

    verifyNaturalLocations("La Mancha1.1", expectedNaturalLocations);
}

TEST(NaturalLocations, Destination)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(64, 118), BWAPI::TilePosition(29, 107)},
            {BWAPI::TilePosition(31, 7),   BWAPI::TilePosition(63, 19)}
    };

    verifyNaturalLocations("Destination", expectedNaturalLocations);
}

TEST(NaturalLocations, NeoMoonGlaive)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(117, 96), BWAPI::TilePosition(115, 66)},
            {BWAPI::TilePosition(7, 90),   BWAPI::TilePosition(28, 107)},
            {BWAPI::TilePosition(67, 6),   BWAPI::TilePosition(39, 14)}
    };

    verifyNaturalLocations("Neo Moon Glaive", expectedNaturalLocations);
}

TEST(NaturalLocations, HeartbreakRidge)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(117, 56), BWAPI::TilePosition(111, 28)},
            {BWAPI::TilePosition(7, 37),   BWAPI::TilePosition(13, 66)}
    };

    verifyNaturalLocations("Heartbreak Ridge", expectedNaturalLocations);
}

TEST(NaturalLocations, TauCross)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(117, 9),  BWAPI::TilePosition(103, 37)},
            {BWAPI::TilePosition(93, 118), BWAPI::TilePosition(51, 113)},
            {BWAPI::TilePosition(7, 44),   BWAPI::TilePosition(14, 13)}
    };

    verifyNaturalLocations("Tau Cross", expectedNaturalLocations);
}

TEST(NaturalLocations, CircuitBreaker)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(7, 118),   BWAPI::TilePosition(7, 92)},
            {BWAPI::TilePosition(117, 118), BWAPI::TilePosition(117, 92)},
            {BWAPI::TilePosition(117, 9),   BWAPI::TilePosition(117, 34)},
            {BWAPI::TilePosition(7, 9),     BWAPI::TilePosition(7, 34)}
    };

    verifyNaturalLocations("Circuit Breaker", expectedNaturalLocations);
}

TEST(NaturalLocations, Roadrunner)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(7, 90),   BWAPI::TilePosition(7, 66)},
            {BWAPI::TilePosition(117, 35), BWAPI::TilePosition(117, 60)},
            {BWAPI::TilePosition(98, 119), BWAPI::TilePosition(71, 118)},
            {BWAPI::TilePosition(27, 6),   BWAPI::TilePosition(53, 6)}
    };

    verifyNaturalLocations("Roadrunner", expectedNaturalLocations);
}

TEST(NaturalLocations, Jade)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(8, 117),   BWAPI::TilePosition(7, 86)},
            {BWAPI::TilePosition(7, 7),     BWAPI::TilePosition(35, 6)},
            {BWAPI::TilePosition(117, 7),   BWAPI::TilePosition(117, 38)},
            {BWAPI::TilePosition(117, 117), BWAPI::TilePosition(89, 118)}
    };

    verifyNaturalLocations("Jade", expectedNaturalLocations);
}

TEST(NaturalLocations, Icarus)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(43, 8),   BWAPI::TilePosition(78, 6)},
            {BWAPI::TilePosition(81, 118), BWAPI::TilePosition(46, 118)},
            {BWAPI::TilePosition(8, 77),   BWAPI::TilePosition(7, 49)},
            {BWAPI::TilePosition(116, 47), BWAPI::TilePosition(117, 77)}
    };

    verifyNaturalLocations("Icarus.scm", expectedNaturalLocations);
}

TEST(NaturalLocations, Benzene)
{
    std::map<BWAPI::TilePosition, BWAPI::TilePosition> expectedNaturalLocations = {
            {BWAPI::TilePosition(7, 96),   BWAPI::TilePosition(11, 70)},
            {BWAPI::TilePosition(117, 13), BWAPI::TilePosition(113, 40)}
    };

    verifyNaturalLocations("Benzene", expectedNaturalLocations);
}
