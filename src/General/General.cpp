#include "General.h"
#include "Squad.h"
#include "Units.h"
#include "Map.h"
#include "AttackBaseSquad.h"

/*
Things we want to be able to do:
- Have multiple clusters operating independently, General decides whether they should be combined / split
- Intelligent pathing and tactics: avoid narrow chokes, prefer attacking from high ground, lure enemy into bad position
- Lure enemy units on chases
- Fight more effectively on defense, never throw away units piecemeal
- Spread out when fighting tanks


Have an overall combat strategy, e.g. sair-reaver, drop harass, etc. that affects which squads we create and how aggressive we are
Squad types:
- Attack base
- Attack army
- Contain/defend at choke
- Defend base
- Defend wall
- Escort workers
- Special ops: drops, harass, scout blocking, etc.

Ideas for structuring:
- Have a prioritized list of goals similar to the producer, e.g. 1. defend main, 2. contain enemy, 3. scout enemy expansions, 4. harass enemy expansions
- Strategist coordinates needs of the General with the goals it provides the producer
- Need to extend scouting to include mid- and late game (presumably with observers)
- Each goal is represented by a squad. Each squad can have multiple control groups depending on unit composition and distance between them.
- Unit micro is handled individually, though in cases like shuttle/reaver the two will be considered as one unit
*/

namespace General
{
#ifndef _DEBUG
    namespace
    {
#endif

        std::map<BWAPI::Unit, std::shared_ptr<Squad>> unitToSquad;

        // TODO: This will eventually map goal types instead
        std::map<std::string, std::shared_ptr<Squad>> squads;

#ifndef _DEBUG
    }
#endif

    void updateAssignments()
    {
        if (squads.find("attack") == squads.end())
        {
            auto enemyMain = Map::getEnemyMain();
            if (!enemyMain) return;

            squads["attack"] = std::make_shared<AttackBaseSquad>(enemyMain);
        }

        auto attackSquad = squads["attack"];

        attackSquad->updateClusters();

        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (!unit->isCompleted()) continue;
            if (unit->getType().isBuilding()) continue;
            if (unit->getType().isWorker()) continue;
            if (unitToSquad.find(unit) != unitToSquad.end()) continue;

            attackSquad->addUnit(unit);
            unitToSquad[unit] = attackSquad;
        }
    }

    void issueOrders()
    {
        if (squads.find("attack") == squads.end()) return;

        squads["attack"]->execute();
    }
}
