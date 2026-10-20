#pragma once

#include "Common.h"

namespace StrategySelector
{
    /* L2 selection seam (D6-9 v1). Picks an opening label from the JSON strategy */
    /* library; the legacy engine then instantiates the exact same Plays it always */
    /* did for that label (two-layer principle: JSON carries the choice, C++ Plays */
    /* carry the behavior). */
    /* Returns "" - and the caller falls back to the legacy selection path */
    /* unchanged - when: */
    /* * strategy_engine is "legacy" (params.json), or */
    /* * no strategy matches the matchup with a legacy_opening executor, or */
    /* * the candidate is illegal on this map (e.g. requires_wall). */
    /* v1 policy: Tier-0 first (tier0_vs contains the enemy name); scoring stays */
    /* on the legacy path (hot-fixed sticky/R1/R2) until the D10-11 selector. */
    std::string selectOpening(const std::string &matchup);

    /* P3 scoring-layer seam: resolves the opening when no JSON strategy hit. */
    /* selection_scoring=legacy (default) delegates to the proven UCB1+hotfix */
    /* layer in Opponent (behavior-identical to the pre-P3 call sites); */
    /* selection_scoring=new runs the ported scoring layer here over */
    /* Opponent::openingHistory() - parity-checked via selector_bench offline */
    /* before per-matchup retirement of the legacy path. */
    std::string selectOpeningScored(const std::string &matchup);
}
