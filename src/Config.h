#pragma once

#include "Common.h"
#include <nlohmann.h>

namespace Config
{
    /* L0 config loading (B6). Called once from onStart. Loads params.json and the */
    /* strategy library; on any parse/schema failure falls back to built-in defaults */
    /* (legacy engine, empty library) instead of crashing mid-tournament. */
    void initialize();

    /* params.json strategy_engine == "new" (D10-11 engine switch; default legacy) */
    bool newStrategyEngine();

    /* A4 frame governor (S5): "measure" (default, no intervention) or "ladder" */
    /* (yield a frame after an over-limit frame, up to skipMax consecutive). */
    /* governorLadder() is a cached bool - the SEH onFrame wrapper must not */
    /* touch objects with destructors (C2712), so no std::string accessor here. */
    bool governorLadder();
    int governorLimitMs();   // default 55
    int governorSkipMax();   // default 3

    /* S8 producer engine: false = legacy (default). true adds the mid-game */
    /* production capacity governor (gateways scale with mining bases). */
    bool producerEngine();
    int capacityGatesPerBase();  // default 4  (PW formula)
    int capacityMinGates();      // default 3
    int capacityMaxGates();      // default 25
    int capacityFromFrame();     // default 10000 (~7 min; before this legacy behavior)

    // Validated strategy library loaded from strategies/*.json (possibly empty)
    const nlohmann::json &strategies();

    /* S6: recognizer threshold table (recognizers.json; missing file -> empty, */
    /* consumers keep built-in defaults) and the fingerprint vocabulary */
    /* (fingerprints.json, D5 item 6 - versioned frozen contract). */
    const nlohmann::json &recognizers();
    const nlohmann::json &fingerprints();
    int fingerprintVersion(); // params fingerprint_version; 0 when absent

    /* AIIDE2025 absorb (2026-09-17). dt_suspect: 0=off | 1=record (diagnose */
    /* only, writes shushanking_dt_suspect.txt, never changes behavior) | 2=on */
    /* (suspected-DT transition active). d4_transitions: PvZ FFEDragoons */
    /* reactive transitions (ZerglingRush->2 cannons, MutaRush->corsairs). */
    int dtSuspectMode();
    bool d4Transitions();

    /* PW mid-game army audit (2026-09-18): the capacity governor's bank gate is */
    /* superlinear (N*goon price). bank_divisor (default 1.0 = unchanged) divides */
    /* the required bank, letting mid-game gateway expansion happen earlier. */
    double capacityBankDivisor();

    /* BB engagement-discipline hysteresis gate (2026-09-18): attack only when */
    /* initialMine >= attack_ratio * initialEnemy, retreat when <= retreat_ratio. */
    /* Disabled by default (behavior identical to pre-change). */
    bool attackRatioGateEnabled();
    double attackRatioAttack();   /* default 1.25 */
    double attackRatioRetreat();  /* default 0.8 */

    /* P1-A/P1-B (2026-09-20 report v2): PvP late-game transition and one-base */
    /* exit. transition_frame: earliest frame the PvP composition adds High */
    /* Templar (storm) and reavers (50-game evidence: 0 trained while every */
    /* >=16min loss faced an HT/reaver mix). allin_exit_frame: earliest frame a */
    /* one-base line with no army-value lead rotates into MidGame and takes its */
    /* natural (~12 min; the observed all-in decision window is ~14 min). */
    bool pvpLateTransitionEnabled();      /* default true */
    int pvpLateTransitionFrame();         /* default 15000 (~10.5 min) */
    int pvpLateTransitionHTTarget();      /* default 4 */
    int pvpLateTransitionReaverTarget();  /* default 2 */
    bool pvpAllinExitEnabled();           /* default true */
    int pvpAllinExitFrame();              /* default 17000 (~12 min) */

    /* Direction 2 (2026-09-24 plan): proactive scout of the enemy Templar */
    /* Archives after their core completes. PW's source abandons a scouted DT */
    /* line with 0.65-0.85 probability, so keeping a scout alive inside the */
    /* DT window both feeds handleDetection exact timings and forces the */
    /* abandon. Off by default; behavior identical to 3d056f10 when false. */
    bool dtScoutProactive();              /* default false */

    /* Direction 1 (2026-09-24 plan): opponent-response penalty in opening */
    /* selection. Uses the (myOpening x enemyLabel) loss statistics already */
    /* recorded in shushanking_<opponent>.json: when the enemy has beaten one of */
    /* our openings >=2 times with a label they are still playing (recent */
    /* window), that opening is excluded before sticky/R2. Off by default. */
    bool opponentResponse();              /* default false */

    /* Direction C (2026-09-25 battle forensics Q1): FAP exchange-ratio gate. */
    /* SD loss games self-feed battles the sim predicts as net losses; this */
    /* gate refuses opening (and disengages continuing) fights whose predicted */
    /* percentGain is below min_percent_gain. Off by default. */
    bool exchangeGateEnabled();           /* default false */
    double exchangeGateMinGain();         /* default 0.05 */

    /* Direction B (2026-09-26 battle forensics Q4): combat-kiting separation */
    /* consumes enemyAoeRadius (reaver scarabs / storm). When on, dragoon */
    /* kiting spreads against splash with the Ball-formation formula and stops */
    /* skipping in-range friends from separation. Off = stock behavior. */
    bool battleMicroEnabled();            /* default false */

    /* Direction "storm timing" (2026-09-28 forensics): cash in the 4.5-minute */
    /* Archives lead. storm_timing moves the HT transition earlier (params); */
    /* storm_amulet adds the Khaydarin Amulet upgrade (never researched in 60 */
    /* games; PW wins bought it 88%); archon_merge merges spent HTs into an */
    /* Archon instead of leaving them to die. All off by default. */
    bool stormTimingEnabled();            /* default false */
    bool stormAmuletEnabled();            /* default false */
    bool archonMergeEnabled();            /* default false */

    /* Direction "storm casting" (2026-09-29, batch18 step-0 evidence): SD */
    /* researched storm in 16/20 games and trained ~7 HTs/game yet issued zero */
    /* storm casts - the tech had no executor. When on, squad HTs are pulled */
    /* out of the combat clusters (arbiter pattern), held behind the vanguard */
    /* and cast Psionic Storm at valuable enemy clumps. Off = stock behavior. */
    bool stormCastEnabled();              /* default false */
    int stormCastMinEnemies();            /* default 3 */

    /* Direction "storm window" (2026-09-30, official 341-game data): FFE wins
     * 97% of games decided before the enemy storm line forms and 29% after.
     * storm_window raises squad aggression while fresh scouting shows the enemy
     * has no Templar Archives (the window to cash in), and lowers it once the
     * Archives is up. fap_ht_value counts HTs into the sim's initial value
     * totals (they are excluded as weaponless, so storm armies read as free
     * value). Both off by default. */
    bool stormWindowEnabled();            /* default false */
    int stormWindowMinFrame();            /* default 13000 */
    bool fapHtValueEnabled();             /* default false */

    /* Direction "army composition" (O4, 2026-09-30): measured zealot:dragoon is ~1:8 */
    /* because PvP midgame zealots only get leftover resources. When on, mirror the */
    /* PvZ ratio mix - while the live zealot share is below the configured ratio, */
    /* zealots are the unlimited main army line, otherwise dragoons. Off = stock. */
    bool armyCompositionEnabled();        /* default false */
    double armyCompositionZealotRatio();  /* default 0.28 */

    /* The 50%-plan packages (2026-10-08 funnel analysis): the pooled b18+ */
    /* funnel says the win-rate gap lives in D (win rate after dragging past */
    /* 15 min with a lead, 13.6%), B (converting a lead before 15 min, 9.1%) */
    /* and early third-base expansion being a negative asset (16.9%). */
    /* storm_guard: once the enemy storm line is sighted, drop squad aggression */
    /* to the configured penalty and raise the DT harass line. window_push: */
    /* one-shot all-in - when our army is ahead in the pre-storm window, lock */
    /* the main army onto the enemy main, ignore combat sim, freeze workers */
    /* and spam gateways. expand_throttle: block new expansions beyond the */
    /* natural before min_frame. All off by default. */
    bool stormGuardEnabled();             /* default false */
    double stormGuardPenalty();           /* default 0.55 */
    bool windowPushEnabled();             /* default false */
    double windowPushArmyRatio();         /* default 1.15 */
    int windowPushMinFrame();             /* default 13000 */
    bool expandThrottleEnabled();         /* default false */
    int expandThrottleMinFrame();         /* default 15600 */

    /* Runtime latch for the one-shot window push: set by the strategy engine */
    /* when the push fires, read by the attack-play selector, worker saturation */
    /* and producer capacity logic. Reset only by process restart (one shot */
    /* per game by design). */
    namespace WindowPush
    {
        extern bool fired;
    }

    /* One-line load status for the diagnostics channel */
    std::string statusLine();
}
