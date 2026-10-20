#pragma once

#include "Common.h"

#include <optional>

namespace Opponent
{
    void initialize();

    void update();

    void gameEnd(bool isWinner);

    std::string &getName();

    bool isUnknownRace();

    bool hasRaceJustBeenDetermined();

    bool canBeRace(BWAPI::Race race);

    void setGameValue(const std::string &key, int value);

    void incrementGameValue(const std::string &key, int delta = 1);

    bool isGameValueSet(const std::string &key);

    void addMyStrategyChange(const std::string &strategy);

    void addEnemyStrategyChange(const std::string &strategy);

    int minValueInPreviousGames(const std::string &key, int defaultNoData, int maxCount = INT_MAX, int minCount = 0);

    double winLossRatio(double defaultValue, int maxCount = INT_MAX);

    /* P3 scoring-layer seam: per-game records the opening scorer consumes, */
    /* newest first. myOpening = first myStrategy label; onThisMap/enemyAggressive */
    /* derived exactly as the legacy scorer's helpers. Games lacking a won-bool or */
    /* an opening label are skipped; enemyAggressive is nullopt when the record */
    /* lacks the data (the legacy UCB1 phase skips those games; the sticky/R1/R2 */
    /* phase does not). */
    struct OpeningGameRecord
    {
        std::string myOpening;
        bool won;
        bool onThisMap;
        std::optional<bool> enemyAggressive;
        /* P0-2/P0-3 (2026-09-18 report): every non-Unknown enemy label the */
        /* game recorded; consumed by the label-conditioned opening prior. */
        std::vector<std::string> enemyLabels;
    };
    std::vector<OpeningGameRecord> openingHistory();

    /* Distinct enemy strategy labels (fingerprints) observed in the most recent n */
    /* games, excluding "Unknown". Reuses the recorded enemyStrategy stream - no new */
    /* profile field. Feeds the L2 cross-game fingerprint blacklist (v1.6 B11). */
    std::set<std::string> recentEnemyStrategies(int n);
}
