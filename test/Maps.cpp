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
                              {13397, 6817, 51163, 83305})
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
                runner(test);
            }
        }
    }
}
