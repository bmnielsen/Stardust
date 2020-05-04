#pragma once

#include <fap.h>

#include "Common.h"

#include "Squad.h"

/*
General approach to combat:
1. Strategist provides the General with a prioritized list of combat goals, e.g. attack this base, defend that base, scout expansions, etc.
2. General ensures squads are created for each goal that is currently active and allocates the units it thinks are needed to fulfill the goal
 - In some cases it may be OK to skip a goal if we know we don't have enough units for it
 - There should always be a "fallback" goal where we can put units that we don't need for anything else
3. Each squad splits its units into smaller clusters of units that are close together
 - The various clusters in a squad can have different goals, e.g. some units may be retreating while others are
   moving up to the front line, units moving towards the front line may engage enemies they meet along the way
4. Each cluster is then microed independently depending on the cluster's current goal

Types of goals:
- Attack main base
  - Will by default get all units not assigned elsewhere
  - If not enough units are available, tries to set up a contain somewhere (at main choke, natural choke, or somewhere along the retreat path)
  - Use intelligent pathing, e.g. try to go around dangerous areas of the map, sneak into the enemy base, etc.
- Attack expansion
  - Only gets a minimal number of units needed to shut it down
  - Units should be willing to retreat if the enemy reinforces
- Attack army
  - Mostly for PvT, it will probably be nice to have a dedicated goal for push breaking
  - Allows us to coordinate stuff like attacking with zealots from the sides, zealot bombs, etc.
- Defend base
  - May get no units if we do not see any threat
  - Strategist will need to take static defenses into consideration
- Escort worker transfer
  - If the route between bases is considered hazardous, assign a goon or two to escort the workers
- Drop
  - Should be able to drop multiple times, do elevator, etc.
- Recon / harass
  - Especially interesting for corsairs in PvZ, will want to harass overlords
- Clear spider mine
  - If we have an observer nearby, send it with a probe, otherwise send a dt if available, otherwise send a zealot
- Block scouting

Technical stuff we need:
- Quick lookups of units close to a position
  - Useful for both generating our clusters and for getting nearby enemy units
- Various levels of combat sim, probably want a quick evaluator and the full simulation
- Threat-aware pathfinding
- Flocking, needs to both be able to move units so they can fight effectively and keep them spread out to avoid splash
- Health prediction, i.e. track upcoming hits and use this to avoid overkill and balance health of our units
*/

namespace General
{
    void initialize();

    void updateClusters();

    void issueOrders();

    void addSquad(const std::shared_ptr<Squad> &squad);

    void removeSquad(const std::shared_ptr<Squad> &squad);
}

namespace CombatSim
{
    void initialize();

    int unitValue(const FAP::FAPUnit<> &unit);
}
