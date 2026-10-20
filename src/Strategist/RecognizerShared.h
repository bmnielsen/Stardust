#pragma once

/* Shared observation predicates for the per-matchup enemy strategy recognizers */
/* (dedup plan P2 step A). The three recognizer translation units used to carry */
/* verbatim copies of these helpers; they now share this single implementation. */
/* The extraction is behavior-identical: matchup-specific thresholds are passed */
/* in explicitly by thin local wrappers that forward the caller's rt() fields. */

#include "../Instrumentation/Log.h"
#include "Units.h"
#include "Map.h"
#include "UnitUtil.h"

#include <algorithm>
#include <vector>

namespace RecognizerShared
{
    /* P2-C: declarative transition table for the recognizer state machines. */
    /* Within a state, entries are scanned in insertion order; the first true */
    /* guard fires. chase=true re-scans from the new state (mirrors the original */
    /* "strategy = X; continue;" switch hops); chase=false returns the new state */
    /* immediately (mirrors the original "return X;" classifications). */
    template <typename S>
    struct STransition
    {
        S from;
        bool chase;
        S to;
        bool (*guard)();
    };

    /* Semantics mirror the original per-matchup switch loops exactly: an */
    /* unchanged state returns immediately; 10 consecutive chases set exhausted */
    /* (the original logged its loop-guard ERROR there) and return the current. */
    template <typename S>
    S runTransitions(S current, const std::vector<STransition<S>> &table, bool &exhausted)
    {
        exhausted = false;
        for (int i = 0; i < 10; i++)
        {
            bool advanced = false;
            for (const auto &t : table)
            {
                if (t.from != current || !t.guard()) continue;
                if (t.chase)
                {
                    current = t.to;
                    advanced = true;
                    break;
                }
                return t.to;
            }
            if (!advanced) return current;
        }
        exhausted = true;
        return current;
    }

    inline bool countAtLeast(BWAPI::UnitType type, int count)
    {
        auto &timings = UnitMgr::getEnemyUnitTimings(type);
        return timings.size() >= count;
    }

    inline bool noneCreated(BWAPI::UnitType type)
    {
        return UnitMgr::getEnemyUnitTimings(type).empty();
    }

    inline bool createdBeforeFrame(BWAPI::UnitType type, int tick, int count = 1)
    {
        auto &timings = UnitMgr::getEnemyUnitTimings(type);
        if (timings.size() < count) return false;

        return timings[count - 1].first < tick;
    }

    inline bool createdBeforeUnit(BWAPI::UnitType first, int firstCount, BWAPI::UnitType second, int secondCount)
    {
        auto &firstTimings = UnitMgr::getEnemyUnitTimings(first);
        if (firstTimings.size() < firstCount) return false;

        auto &secondTimings = UnitMgr::getEnemyUnitTimings(second);
        return secondTimings.size() < secondCount || firstTimings[firstCount - 1].first <= secondTimings[secondCount - 1].first;
    }

    inline bool justDiscovered(BWAPI::UnitType type)
    {
        auto &timings = UnitMgr::getEnemyUnitTimings(type);
        return std::any_of(timings.begin(), timings.end(), [](const auto timing) { return timing.second == frameNow; });
    }

    /* True when unprovoked enemy workers are inside our main and no real combat */
    /* unit is with them. Window and worker threshold come from the caller's rt(). */
    inline bool isWorkerRush(int workerRushWindow, int workerRushMinWorkers)
    {
        if (frameNow >= workerRushWindow) return false;

        int workers = 0;
        for (const Unit &gameUnit : UnitMgr::allEnemy())
        {
            if (!gameUnit->lastPositionValid) continue;
            if (gameUnit->type.isBuilding()) continue;

            bool isInArea = false;
            for (const auto &area : Map::getMyMainAreas())
            {
                if (BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(gameUnit->prevPos)) == area)
                {
                    isInArea = true;
                    break;
                }
            }
            if (!isInArea) continue;

            /* If there is a normal combat unit in our main, it isn't a worker rush */
            if (UnitHelper::IsCombatUnit(gameUnit->type) && gameUnit->type.canAttack()) return false;

            if (gameUnit->type.isWorker()) workers++;
        }

        return workers > workerRushMinWorkers;
    }
}
