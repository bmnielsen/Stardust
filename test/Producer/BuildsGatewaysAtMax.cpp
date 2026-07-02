#include "BWTest.h"
#include "ClearOpponentUnitsModule.h"

#include "TestAttackBasePlay.h"
#include "Strategist.h"
#include "Map.h"

namespace
{
    class TestStrategyEngine : public StrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays, bool transitioningFromRandom, const std::string &openingOverride) override {}

        void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                              std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                              std::vector<std::pair<int, int>> &mineralReservations) override
        {
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "test",
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "test",
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       -1,
                                                                       -1);
        }
    };
}

TEST(Producer, BuildsGatewaysAtMax)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new ClearOpponentUnitsModule();
    };
    test.opponentRace = BWAPI::Races::Terran;
    test.map = Maps::GetOne("Mancha");
    test.randomSeed = 65145;
    test.frameLimit = 5000;

    test.myInitialUnits = {
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(328, 103), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(323, 184), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(192, 306), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(192, 160), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 221), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(107, 271), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 372), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(148, 184), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(138, 171), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(200, 192), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 84), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(107, 180), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 340), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 276), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Forge, BWAPI::Position(528, 288), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Forge, BWAPI::Position(464, 160), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Forge, BWAPI::Position(368, 384), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(107, 267), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(384, 288), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Nexus, BWAPI::Position(288, 240), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 267), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(224, 1312), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(416, 1216), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(254, 1284), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Nexus, BWAPI::Position(320, 1232), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(224, 2212), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(251, 2231), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(192, 2240), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 2268), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 2356), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 2251), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Templar_Archives, BWAPI::Position(432, 2496), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 2452), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(107, 2324), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(201, 2356), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 2315), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 2420), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(162, 2268), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(107, 2315), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(165, 2421), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(107, 2260), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(214, 2366), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(384, 2368), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(155, 2265), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Nexus, BWAPI::Position(288, 2320), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 2314), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(509, 2497), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(107, 2356), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(487, 2453), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1080, 3874), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(320, 3738), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1055, 3846), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(776, 3415), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(320, 3670), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1032, 3924), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(1152, 3520), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(416, 3744), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(1280, 3648), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(224, 3872), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::Position(1120, 3440), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(224, 3712), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(512, 3264), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(352, 3712), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay, BWAPI::Position(208, 3424), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(1184, 3712), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(960, 3936), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1156, 3764), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(1184, 3872), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(896, 4000), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(171, 3723), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Arbiter_Tribunal, BWAPI::Position(208, 3488), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(896, 3936), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1216, 3924), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Forge, BWAPI::Position(80, 3424), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1252, 3764), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(1088, 3520), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1215, 3924), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(512, 3648), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Stargate, BWAPI::Position(1120, 3600), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::Position(352, 3888), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::Position(384, 3408), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(172, 3752), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(992, 3680), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observatory, BWAPI::Position(80, 3488), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1236, 3844), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::Position(864, 3824), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1236, 3892), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Citadel_of_Adun, BWAPI::Position(720, 3456), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Robotics_Facility, BWAPI::Position(496, 3744), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(896, 3744), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1268, 3883), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1268, 3764), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(832, 3744), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1252, 3748), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1236, 3892), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1217, 3846), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(992, 3808), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Nexus, BWAPI::Position(1088, 3792), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(211, 3740), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(448, 3648), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::Position(384, 3504), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1026, 3429), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(234, 3744), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::Position(512, 3408), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(640, 3456), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1160, 3842), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(203, 3924), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::Position(512, 3504), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(171, 3683), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(288, 3456), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 3732), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Cybernetics_Core, BWAPI::Position(496, 3808), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(257, 3843), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(139, 3819), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::Position(480, 3888), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(240, 3740), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(253, 3740), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(416, 3808), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(203, 3924), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(252, 3781), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1236, 3797), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(183, 3819), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1035, 3542), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Nexus, BWAPI::Position(320, 3792), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2393, 3496), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2247, 3493), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2594, 3375), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2329, 3496), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::Position(2429, 3289), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2526, 3338), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2587, 3318), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2946, 3226), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2983, 3288), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2893, 3273), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2971, 3091), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2867, 3118), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2850, 3086), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2676, 3329), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Arbiter, BWAPI::Position(2863, 3204), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2506, 3406), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2977, 3196), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2463, 3445), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(3002, 3229), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2894, 3096), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2879, 3047), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2940, 3132), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2902, 3156), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2934, 3175), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2620, 3326), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2970, 3264), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2719, 3295), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2817, 3232), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2625, 3245), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2805, 3122), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2954, 3113), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2860, 3137), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2918, 3269), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(2801, 3143), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2916, 3228), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2723, 3239), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2898, 3195), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2809, 3094), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2674, 3249), true),
        // UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(2778, 3301), true),
    };

    test.onStartMine = []()
    {
        BWAPI::Broodwar->self()->setMinerals(25000);

        auto baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(116, 89)));

        Strategist::setStrategyEngine(std::make_unique<TestStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    test.run();
}
