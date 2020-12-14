#include "Maps.h"
#include "BWTest.h"

namespace
{
    std::vector<Maps::MapMetadata> maps = {
            Maps::MapMetadata("maps/sscai/(2)Benzene.scx",
                              "af618ea3ed8a8926ca7b17619eebcb9126f0d8b1",
                              "5fba62687ba00c8868fdd64c7869c4005a142772",
                              {1617, 36298}),
            Maps::MapMetadata("maps/sscai/(2)Destination.scx",
                              "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b",
                              "e39c1c81740a97a733d227e238bd11df734eaf96",
                              {50224, 60306}),
            Maps::MapMetadata("maps/sscai/(2)Heartbreak Ridge.scx",
                              "6f8da3c3cc8d08d9cf882700efa049280aedca8c",
                              "fe25d8b79495870ac1981c2dfee9368f543321e3",
                              {50962, 30202}),
            Maps::MapMetadata("maps/sscai/(3)Neo Moon Glaive.scx",
                              "c8386b87051f6773f6b2681b0e8318244aa086a6",
                              "4236df9e8edaea4614833dd0bf66c11e6dcadcc2",
                              {76916, 31337, 59667}),
            Maps::MapMetadata("maps/sscai/(3)Tau Cross.scx",
                              "9bfc271360fa5bab3707a29e1326b84d0ff58911",
                              "85f6d2a51c1437a7e6743402614879e476c54de7",
                              {6401, 65763, 83220}),
            Maps::MapMetadata("maps/sscai/(4)Andromeda.scx",
                              "1e983eb6bcfa02ef7d75bd572cb59ad3aab49285",
                              "297e9cf5f39e8c2a19fe5e271e7cdffec9145e5f",
                              {32598, 19968, 55343, 52071}),
            Maps::MapMetadata("maps/sscai/(4)Circuit Breaker.scx",
                              "450a792de0e544b51af5de578061cb8a2f020f32",
                              "1221d83d6ff9a87955d3083257b31131261bc366",
                              {80379, 54763, 5122, 90810}),
            Maps::MapMetadata("maps/sscai/(4)Empire of the Sun.scm",
                              "a220d93efdf05a439b83546a579953c63c863ca7",
                              "38b6307d283a5ebc084822a08f932600f7f13588",
                              {42899, 60288, 34521, 55127}),
            Maps::MapMetadata("maps/sscai/(4)Fighting Spirit.scx",
                              "d2f5633cc4bb0fca13cd1250729d5530c82c7451",
                              "dcabb11c83e68f47c5c5bdbea0204167a00e336f",
                              {4349, 43875, 50302, 71176}),
            Maps::MapMetadata("maps/sscai/(4)Icarus.scm",
                              "0409ca0d7fe0c7f4083a70996a8f28f664d2fe37",
                              "31292c55e13ae699373185e80024197e530e294d",
                              {93395, 89861, 63196, 23125}),
            Maps::MapMetadata("maps/sscai/(4)Jade.scx",
                              "df21ac8f19f805e1e0d4e9aa9484969528195d9f",
                              "6b9fcc27634f26d772a8921db8c59dfd38bd1d74",
                              {46733, 50350, 77957, 61667}),
            Maps::MapMetadata("maps/sscai/(4)La Mancha1.1.scx",
                              "e47775e171fe3f67cc2946825f00a6993b5a415e",
                              "0245980146ac10d7c53ca0ad8727292074777afa",
                              {62242, 59437, 45965, 51316}),
            Maps::MapMetadata("maps/sscai/(4)Python.scx",
                              "de2ada75fbc741cfa261ee467bf6416b10f9e301",
                              "db1d92e08b7b45abefc6da1cee9a9978c98ac3eb",
                              {75240, 76533, 11897, 85482}),
            Maps::MapMetadata("maps/sscai/(4)Roadrunner.scx",
                              "9a4498a896b28d115129624f1c05322f48188fe0",
                              "997b31f09ddc7f7425daf08d5af0865df65c24d6",
                              {13397, 6817, 51163, 83305}),
            Maps::MapMetadata("maps/cog/(2)BlueStorm1.2.scx",
                              "aab66dbf9c85f85c47c219277e1e36181fe5f9fc",
                              "5c626c0a69d262ad8dd1b4ec7d21556cf7c9f2c9",
                              {64709, 96831}),
            Maps::MapMetadata("maps/cog/(2)Destination1.1.scx",
                              "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b",
                              "e39c1c81740a97a733d227e238bd11df734eaf96",
                              {74355, 76568}),
            Maps::MapMetadata("maps/cog/(2)Hitchhiker1.1.scx",
                              "69a3b6a5a3d4120e47408defd3ca44c954997948",
                              "26b8aef6c1fd5d15d84cfe3e23645ffbfc1559f7",
                              {84575, 53842}),
            Maps::MapMetadata("maps/cog/(2)MatchPoint1.3.scx",
                              "0a41f144c6134a2204f3d47d57cf2afcd8430841",
                              "7e14d53b944b1365973f2d8768c75358c6b28a8f",
                              {6903, 42130}),
            Maps::MapMetadata("maps/cog/(2)NeoChupungRyeong2.1.scx",
                              "f391700c3551e145852822ff95e27edd3173fae6",
                              "7a1407bd6c87d9c93282a26299f7e349ea609561",
                              {98681, 79118}),
            Maps::MapMetadata("maps/cog/(2)NeoHeartbreakerRidge.scx",
                              "d9757c0adcfd61386dff8fe3e493e9e8ef9b45e3",
                              "ecb9c70c5594a5c6882baaf4857a61824fba0cfa",
                              {622, 9952}),
            Maps::MapMetadata("maps/cog/(2)RideofValkyries1.0.scx",
                              "cd5d907c30d58333ce47c88719b6ddb2cba6612f",
                              "22644b6ffe2ed3732a369e51657e4b555ac1404e",
                              {90613, 55482}),
            Maps::MapMetadata("maps/cog/(3)Alchemist1.0.scm",
                              "8000dc6116e405ab878c14bb0f0cde8efa4d640c",
                              "9e5770c62b523042e8af590c8dc267e6c12637fc",
                              {82681, 68326, 72711}),
            Maps::MapMetadata("maps/cog/(3)GreatBarrierReef1.0.scx",
                              "3506e6d942f9721dc99495a141f41c5555e8eab5",
                              "de0ef11a35683ab676f52f0a6f34b388d41cf400",
                              {65006, 32887, 24549}),
            Maps::MapMetadata("maps/cog/(3)NeoAztec2.1.scx",
                              "19f00ba3a407e3f13fb60bdd2845d8ca2765cf10",
                              "dd8978c6b03f26f54915516f6d9e20d37ff5a15b",
                              {21453, 96014, 11829}),
            Maps::MapMetadata("maps/cog/(3)Pathfinder1.0.scx",
                              "b10e73a252d5c693f19829871a01043f0277fd58",
                              "be8e87452219c387529d3331f0a699471a0bf7c7",
                              {58913, 44718, 7775}),
//            Maps::MapMetadata("maps/cog/(3)Plasma1.0.scx",
//                              "6f5295624a7e3887470f3f2e14727b1411321a67",
//                              "8b3e8ed9ce9620a606319ba6a593ed5c894e51df",
//                              {65191, 40072, 49885}),
            Maps::MapMetadata("maps/cog/(3)TauCross1.1.scx",
                              "9bfc271360fa5bab3707a29e1326b84d0ff58911",
                              "85f6d2a51c1437a7e6743402614879e476c54de7",
                              {46537, 23489, 43850}),
            Maps::MapMetadata("maps/cog/(4)Andromeda1.0.scx",
                              "1e983eb6bcfa02ef7d75bd572cb59ad3aab49285",
                              "297e9cf5f39e8c2a19fe5e271e7cdffec9145e5f",
                              {77813, 16107, 12773, 24584}),
            Maps::MapMetadata("maps/cog/(4)ArcadiaII2.02.scx",
                              "442e456721c94fd085ecd10230542960d57928d9",
                              "83cc5c3944a80915a68190d7b87714d8c0cf8a2f",
                              {65804, 14578, 46693, 33866}),
            Maps::MapMetadata("maps/cog/(4)CircuitBreakers1.0.scx",
                              "450a792de0e544b51af5de578061cb8a2f020f32",
                              "1221d83d6ff9a87955d3083257b31131261bc366",
                              {66096, 34391, 84609, 92912}),
            Maps::MapMetadata("maps/cog/(4)FightingSpirit1.3.scx",
                              "5731c103687826de48ba3cc7d6e37e2537b0e902",
                              "bf84532dcdd21b3328670d766edc209fa1520149",
                              {38392, 46294, 96867, 12601}),
            Maps::MapMetadata("maps/cog/(4)LunaTheFinal2.3.scx",
                              "33527b4ce7662f83485575c4b1fcad5d737dfcf1",
                              "215263bd93c8f0ef9d3ecf880c46890cec8d4655",
                              {80572, 84161, 42442, 99643}),
            Maps::MapMetadata("maps/cog/(4)NeoSniperRidge2.0.scx",
                              "9e9e6a3372251ac7b0acabcf5d041fbf0b755fdb",
                              "b90a29d8dffc603a57b67ec23e1ac24c897d042b",
                              {59026, 13160, 27893, 89448}),
            Maps::MapMetadata("maps/cog/(4)Python1.3.scx",
                              "86afe0f744865befb15f65d47865f9216edc37e5",
                              "466be924200fc61188f59bdf6ddeb949b42f5091",
                              {16843, 62583, 21912, 39896}),
            Maps::MapMetadata("maps/aiide/(2)Destination.scx",
                              "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b",
                              "e39c1c81740a97a733d227e238bd11df734eaf96",
                              {46535,91978}),
            Maps::MapMetadata("maps/aiide/(2)HeartbreakRidge.scx",
                              "6f8da3c3cc8d08d9cf882700efa049280aedca8c",
                              "fe25d8b79495870ac1981c2dfee9368f543321e3",
                              {8025,53402}),
            Maps::MapMetadata("maps/aiide/(2)PolarisRhapsody.scx",
                              "614d0048c6cc9dcf08da1409462f22f2ac4f5a0b",
                              "ec7259201347b0c718298c2d4205a89d0cd49080",
                              {47386,80743}),
            Maps::MapMetadata("maps/aiide/(3)Aztec.scx",
                              "ba2fc0ed637e4ec91cc70424335b3c13e131b75a",
                              "222d038e84c3a0e0cb57e882f6e7cf092b06b150",
                              {27868,15308,56024}),
            Maps::MapMetadata("maps/aiide/(3)Longinus2.scx",
                              "d16719e736252d77fdbb0d8405f7879f564bfe56",
                              "4f1e36312c6b9ac6c482e40cbdacbebdb95bd45e",
                              {38507,8908,56481}),
            Maps::MapMetadata("maps/aiide/(4)CircuitBreaker.scx",
                              "450a792de0e544b51af5de578061cb8a2f020f32",
                              "1221d83d6ff9a87955d3083257b31131261bc366",
                              {59698,75387,285,2037}),
            Maps::MapMetadata("maps/aiide/(4)EmpireoftheSun.scm",
                              "a220d93efdf05a439b83546a579953c63c863ca7",
                              "38b6307d283a5ebc084822a08f932600f7f13588",
                              {76313,91990,34810,49649}),
            Maps::MapMetadata("maps/aiide/(4)FightingSpirit.scx",
                              "d2f5633cc4bb0fca13cd1250729d5530c82c7451",
                              "dcabb11c83e68f47c5c5bdbea0204167a00e336f",
                              {61555,8204,85704,90378}),
            Maps::MapMetadata("maps/aiide/(4)Python.scx",
                              "de2ada75fbc741cfa261ee467bf6416b10f9e301",
                              "db1d92e08b7b45abefc6da1cee9a9978c98ac3eb",
                              {72746,72518,48417}),
            Maps::MapMetadata("maps/aiide/(4)Roadkill.scm",
                              "5386ec02cc3ee913acc55181896c287ae9d5b5c6",
                              "2f55a45d8b9cb7ef86c2f688aeeb3c738406beb9",
                              {93388,9193,98731,38186}),
    };
}

namespace Maps
{
    std::vector<MapMetadata> Get(const std::string &search, int players)
    {
        std::vector<MapMetadata> result;
        for (auto &map : maps)
        {
            if (!search.empty() && map.filename.find(search) == std::string::npos) continue;
            if (players > 0 && map.startLocationSeeds.size() != players) continue;

            result.push_back(map);
        }

        return result;
    }

    std::shared_ptr<MapMetadata> GetOne(const std::string &search, int players)
    {
        auto maps = Get(search, players);
        if (maps.empty())
        {
            std::cerr << "Map search returned no results: search=" << search << "; players=" << players << std::endl;
            return nullptr;
        }

        return std::make_shared<MapMetadata>(*maps.begin());
    }

    void RunOnEach(const std::vector<MapMetadata> &maps, const std::function<void(BWTest)> &runner)
    {
        for (const auto &map : maps)
        {
            BWTest test;
            test.map = std::make_shared<MapMetadata>(map);

            std::ostringstream replayName;
            replayName << ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
            replayName << "_" << ::testing::UnitTest::GetInstance()->current_test_info()->name();
            replayName << "_" << test.map->shortname();
            test.replayName = replayName.str();

            runner(test);
        }
    }

    void RunOnEachStartLocation(const std::vector<MapMetadata> &maps, const std::function<void(BWTest)> &runner)
    {
        for (const auto &map : maps)
        {
            for (auto seed : map.startLocationSeeds)
            {
                BWTest test;
                test.map = std::make_shared<MapMetadata>(map);
                test.randomSeed = seed;

                std::ostringstream replayName;
                replayName << ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
                replayName << "_" << ::testing::UnitTest::GetInstance()->current_test_info()->name();
                replayName << "_" << test.map->shortname();
                replayName << "_" << test.randomSeed;
                test.replayName = replayName.str();

                runner(test);
            }
        }
    }
}
