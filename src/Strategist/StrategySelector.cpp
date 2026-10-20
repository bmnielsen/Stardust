#include "StrategySelector.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

#include "BuildingPlacement.h"
#include "Config.h"
#include "Opponent.h"

namespace StrategySelector
{
    namespace
    {
        bool listContains(const nlohmann::json &list, const std::string &value)
        {
            return list.is_array()
                   && std::find(list.begin(), list.end(), value) != list.end();
        }

        bool isLegal(const nlohmann::json &strategy)
        {
            auto legality = strategy.find("legality");
            if (legality == strategy.end() || !legality->is_object()) return true;

            auto wall = legality->find("requires_wall");
            if (wall != legality->end() && wall->is_boolean() && wall->get<bool>()
                && !BuildingPlacement::hasForgeGatewayWall())
            {
                return false;
            }

            /* Map blacklist: strategy is illegal on these map hashes (v1.7 first */
            /* use: forge line is 0-2 on Largo vs PurpleWave; sample caveat noted */
            /* in the strategy JSON - revisit at the next full batch). */
            auto maps = legality->find("maps_blacklist");
            if (maps != legality->end() && maps->is_array())
            {
                std::string currentMap = BWAPI::Broodwar->mapHash();
                if (listContains(*maps, currentMap)) return false;
            }

            /* Cross-game fingerprint blacklist: illegal when any blacklisted */
            /* fingerprint appeared in the enemy's recent games (window 4, aligned */
            /* with the observed 3-4 game adaptation beat; v1.6 二.L2). */
            auto bl = legality->find("blacklist_vs");
            if (bl != legality->end() && bl->is_array() && !bl->empty())
            {
                auto recent = Opponent::recentEnemyStrategies(4);
                for (auto &fingerprint : *bl)
                {
                    if (fingerprint.is_string()
                        && recent.count(fingerprint.get<std::string>()))
                    {
                        return false;
                    }
                }
            }

            /* min_rush_tiles arrives with map-feature plumbing (iteration 2). */
            return true;
        }

        std::string recordLabel(const nlohmann::json &strategy, const nlohmann::json &legacy)
        {
            auto rec = strategy.find("record_label");
            if (rec != strategy.end() && rec->is_string()) return rec->get<std::string>();
            return legacy.get<std::string>();
        }
    }

    std::string selectOpening(const std::string &matchup)
    {
        if (!Config::newStrategyEngine()) return "";

        /* Opponent::getName() is stored lowercased with spaces stripped; tier0_vs */
        /* entries are matched against it case-insensitively (values lowercased). */
        std::string enemy = Opponent::getName();

        for (auto &strategy : Config::strategies())
        {
            if (!strategy.is_object()) continue;

            auto mu = strategy.find("matchup");
            if (mu == strategy.end() || !mu->is_string()
                || mu->get<std::string>() != matchup)
                continue;

            /* Draft strategies without a legacy executor are not selectable in v1 */
            auto legacy = strategy.find("legacy_opening");
            if (legacy == strategy.end() || !legacy->is_string()) continue;

            if (!isLegal(strategy)) continue;

            auto tier0 = strategy.find("tier0_vs");
            if (tier0 != strategy.end() && listContains(*tier0, enemy))
            {
                return recordLabel(strategy, *legacy);
            }
        }

        /* No legal Tier-0 hit: scoring-layer selection stays on the legacy path */
        return "";
    }

    /* ===== P3: scoring layer (ported from Opponent::selectOpeningUCB1) ===== */
    /* Exact port of the hotfix prefix (sticky + R1 + R2) and the conditioned */
    /* UCB1 machinery, reading Opponent::openingHistory() instead of the private */
    /* previousGames. Guard-order, filter differences (the hotfix phase uses */
    /* records lacking enemy-aggression data, the UCB1 phase skips them) and the */
    /* 10-hop chase semantics are preserved verbatim. */

    const std::vector<std::string> &candidatesFor(const std::string &matchup)
    {
        static const std::map<std::string, std::vector<std::string>> candidates = {
                {"PvP", {"EarlyGameDefense", "TwoGateDT", "ForgeExpandDT", "OneGateCoreNoZealot"}},
                {"PvZ", {"FFEDragoons", "SairSpeedlot", "EarlyGameDefense"}},
                {"PvU", {"ForgeFastExpand", "TwoGateZealots"}},
        };
        static const std::vector<std::string> empty;
        auto iterRef = candidates.find(matchup);
        return iterRef != candidates.end() ? iterRef->second : empty;
    }

    std::string selectOpeningUCB1Ported(const std::vector<std::string> &openings, double decayFactor)
    {
        const auto history = Opponent::openingHistory(); // newest first

        using Stats = std::tuple<int, int, double, double>; // wins, losses, reward, potential
        using StatsMap = std::map<std::string, Stats>;

        /* --- Hot fix layer: sticky default + R1/R2 --- */
        std::vector<std::string> legalOpenings;
        {
            /* P0-2/P0-3 (2026-09-18 report): label-conditioned prior. recent */
            /* = distinct enemy labels over the newest 4 games (same window as */
            /* the fingerprint blacklist); cond = per-opening W/L over games */
            /* whose labels intersect the window. P0-3 tightens R1 to a single */
            /* loss when the lost game's labels match the window AND the */
            /* opening is not in good standing vs the window (>=2 games and */
            /* >=50% escapes - protects proven workhorses from one-loss */
            /* churn). P0-2 drops openings below 40% on >=2 window games */
            /* (0% on >=3) before sticky/R2 so a stale sticky line or */
            /* all-history rollback cannot force an opening the current style */
            /* beats. Cold start (no intersecting labels) keeps the */
            /* pre-change behavior; sticky/R2/UCB1 order untouched. */
            auto recent = Opponent::recentEnemyStrategies(4);
            std::map<std::string, std::pair<int, int>> conditioned; // wins, losses vs current window
            for (const auto &rec : history)
            {
                bool intersect = false;
                for (auto &label : rec.enemyLabels)
                {
                    if (recent.count(label))
                    {
                        intersect = true;
                        break;
                    }
                }
                if (!intersect) continue;
                auto &[cw, cl] = conditioned[rec.myOpening];
                if (rec.won) cw++; else cl++;
            }

            std::map<std::string, int> recentLossStreak; // consecutive losses from that opening's newest game
            std::map<std::string, std::vector<std::string>> newestLossLabels; // P0-3: labels of that newest loss
            std::set<std::string> streakDone;            // openings whose streak was closed by a win
            std::map<std::string, std::pair<int, int>> record; // wins, losses over all games
            std::string stickyOpening;
            bool haveMostRecent = false;
            for (const auto &rec : history)
            {
                if (!streakDone.contains(rec.myOpening))
                {
                    if (rec.won) streakDone.insert(rec.myOpening);
                    else
                    {
                        if (!recentLossStreak.contains(rec.myOpening))
                            newestLossLabels[rec.myOpening] = rec.enemyLabels;
                        recentLossStreak[rec.myOpening]++;
                    }
                }
                if (rec.won) record[rec.myOpening].first++; else record[rec.myOpening].second++;

                if (!haveMostRecent)
                {
                    haveMostRecent = true;
                    if (rec.won) stickyOpening = rec.myOpening;
                }
            }

            auto competentVsWindow = [&](const std::string &opening)
            {
                auto iterRef = conditioned.find(opening);
                if (iterRef == conditioned.end()) return false;
                auto &[cw, cl] = iterRef->second;
                int games = cw + cl;
                return games >= 2 && (double)cw / (double)games >= 0.5;
            };

            for (const auto &opening : openings)
            {
                auto st = recentLossStreak.find(opening);
                bool banned = st != recentLossStreak.end() && st->second >= 2; // R1 banned
                if (!banned && st != recentLossStreak.end() && st->second >= 1)
                {
                    /* P0-3: a single newest loss vs the live style rotates us */
                    /* away once - unless the opening is already in good */
                    /* standing vs the window (competentVsWindow), which */
                    /* shields proven workhorses from one-loss churn. */
                    auto lab = newestLossLabels.find(opening);
                    if (lab != newestLossLabels.end())
                    {
                        for (auto &label : lab->second)
                        {
                            if (recent.count(label) && !competentVsWindow(opening))
                            {
                                banned = true;
                                break;
                            }
                        }
                    }
                }
                if (!banned) legalOpenings.push_back(opening);
            }
            if (legalOpenings.empty()) legalOpenings = openings; // degenerate: everything banned

            /* P0-2: exclude openings the current window beats. */
            {
                std::vector<std::string> filtered;
                for (const auto &opening : legalOpenings)
                {
                    auto iterRef = conditioned.find(opening);
                    bool exclude = false;
                    if (iterRef != conditioned.end())
                    {
                        auto &[cw, cl] = iterRef->second;
                        int games = cw + cl;
                        if (games >= 3 && cw == 0) exclude = true;
                        else if (games >= 2 && (double)cw / (double)games < 0.4) exclude = true;
                    }
                    if (!exclude) filtered.push_back(opening);
                }
                if (!filtered.empty()) legalOpenings = filtered; // degenerate guard
            }

            /* Direction 1 (2026-09-24 plan): opponent-response penalty. */
            /* Complements P0-2/P0-3 (which react to our own loss streaks) by */
            /* reacting to the opponent's stable counter: an opening is excluded */
            /* when some label the enemy is still playing (recent window) has */
            /* beaten it >=2 times without ever losing to it. Grounded in the */
            /* (myOpening x enemyLabel) pairs the archive already records. */
            if (Config::opponentResponse())
            {
                std::map<std::pair<std::string, std::string>, int> responseGames;
                std::map<std::pair<std::string, std::string>, int> responseLosses;
                for (const auto &rec : history)
                {
                    for (auto &label : rec.enemyLabels)
                    {
                        auto key = std::make_pair(rec.myOpening, label);
                        responseGames[key]++;
                        if (!rec.won) responseLosses[key]++;
                    }
                }
                std::vector<std::string> kept;
                for (const auto &opening : legalOpenings)
                {
                    bool countered = false;
                    for (auto &kv : responseGames)
                    {
                        if (kv.first.first != opening) continue;
                        if (kv.second >= 2
                            && responseLosses[kv.first] == kv.second
                            && recent.count(kv.first.second))
                        {
                            countered = true;
                            break;
                        }
                    }
                    if (countered)
                    {
                        AppLog::Get() << "Opening selection: opponent-response excludes " << opening;
                        continue;
                    }
                    kept.push_back(opening);
                }
                if (!kept.empty()) legalOpenings = kept; // degenerate guard
            }

            if (!stickyOpening.empty()
                && std::find(legalOpenings.begin(), legalOpenings.end(), stickyOpening) != legalOpenings.end())
            {
                AppLog::Get() << "Opening selection (hotfix): sticky win line -> " << stickyOpening;
                return stickyOpening;
            }

            std::string rollback;
            double rollbackRate = 0.5;
            int rollbackWins = -1;
            for (const auto &opening : legalOpenings)
            {
                auto iterRef = record.find(opening);
                if (iterRef == record.end()) continue;
                auto [wins, losses] = iterRef->second;
                int games = wins + losses;
                if (games < 2) continue;
                double rate = (double)wins / (double)games;
                if (rate < 0.5) continue;
                if (rate > rollbackRate || (rate == rollbackRate && wins > rollbackWins))
                {
                    rollback = opening;
                    rollbackRate = rate;
                    rollbackWins = wins;
                }
            }
            if (!rollback.empty())
            {
                AppLog::Get() << "Opening selection (hotfix): R2 rollback -> " << rollback;
                return rollback;
            }
        }

        /* --- Conditioned UCB1 machinery --- */
        StatsMap all;
        StatsMap vsAggressive;
        StatsMap vsCalm;
        double allPotential = 0.0;
        double aggressivePotential = 0.0;
        double calmPotential = 0.0;
        int aggressiveRecent = 0;
        int recentWindow = 0;
        int count = 0;
        for (const auto &rec : history)
        {
            /* The legacy UCB1 phase skips records lacking enemy-aggression data */
            if (!rec.enemyAggressive.has_value()) continue;

            double thisPotential = (rec.onThisMap ? 2.0 : 1.0);
            thisPotential *= std::exp(decayFactor * -1 * (count + 1));

            auto &[wins, losses, reward, potential] = all[rec.myOpening];
            potential += thisPotential;
            allPotential += thisPotential;
            if (rec.won) { wins++; reward += thisPotential; } else { losses++; }

            {
                auto &[aWins, aLosses, aReward, aPotential] =
                        rec.enemyAggressive.value() ? vsAggressive[rec.myOpening] : vsCalm[rec.myOpening];
                aPotential += thisPotential;
                if (rec.enemyAggressive.value()) aggressivePotential += thisPotential;
                else calmPotential += thisPotential;
                if (rec.won) { aWins++; aReward += thisPotential; } else { aLosses++; }

                if (recentWindow < 6)
                {
                    recentWindow++;
                    if (rec.enemyAggressive.value()) aggressiveRecent++;
                }
            }
        }

        /* Conditioned table only when the tendency is clear and there is enough weight */
        bool tendencyAggressive = recentWindow >= 3 && aggressiveRecent * 2 > recentWindow;
        bool tendencyCalm = recentWindow >= 3 && aggressiveRecent * 3 < recentWindow;
        const StatsMap *table = &all;
        double tablePotential = allPotential;
        if (tendencyAggressive && aggressivePotential >= 1.5)
        {
            table = &vsAggressive;
            tablePotential = aggressivePotential;
        }
        else if (tendencyCalm && calmPotential >= 1.5)
        {
            table = &vsCalm;
            tablePotential = calmPotential;
        }

        /* Never-tried openings are always explored first */
        for (const auto &opening : legalOpenings)
        {
            if (table->find(opening) == table->end()) return opening;
        }

        /* Unbeaten openings get priority, but only with a real sample (>= 2 games) */
        for (const auto &opening : legalOpenings)
        {
            auto iterRef = table->find(opening);
            auto &[wins, losses, reward, potential] = iterRef->second;
            if (losses == 0 && (wins + losses) >= 2) return opening;
        }

        /* UCB1 (exploration coefficient 0.5) */
        double bestScore = -1.0;
        std::string bestOpening;
        for (const auto &opening : legalOpenings)
        {
            auto iterRef = table->find(opening);
            double wins = 0, losses = 0, reward = 0, potential = 0.5;
            if (iterRef != table->end())
            {
                auto &[w, l, r, p] = iterRef->second;
                wins = (double)w; losses = (double)l; reward = r; potential = std::max(p, 0.5);
            }

            double score = (reward / potential)
                           + std::sqrt(0.5 * std::log(std::max(tablePotential, 2.0)) / potential);

            if (score > bestScore)
            {
                bestScore = score;
                bestOpening = opening;
            }
        }

        return bestOpening;
    }
}

namespace StrategySelector
{
    std::string selectOpeningScored(const std::string &matchup)
    {
        /* P3 retirement (2026-09-17): the ported scoring layer above is the single */
        /* implementation. The legacy Opponent::selectOpeningUCB1 was proven */
        /* decision-identical offline (parity.py, 1,725 decision points) and the */
        /* new path passed a 54-game non-inferiority batch (43/54 >= 42). */
        const auto &openings = candidatesFor(matchup);
        return selectOpeningUCB1Ported(openings, 0.05);
    }
}
