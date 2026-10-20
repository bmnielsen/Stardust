#include "Config.h"

#include <filesystem>
#include <fstream>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "FileTools.h"

namespace Config
{
    namespace
    {
        bool initialized = false;
        bool newEngine = false;
        std::string govMode = "measure";
        int govLimitMs = 55;
        int govSkipMax = 3;
        bool prodEngine = false;
        int capGatesPerBase = 4;
        int capMinGates = 3;
        int capMaxGates = 25;
        int capFromFrame = 10000;
        int fpVersion = 0;
        int dtSuspect = 0;
        bool d4Trans = false;
        double capBankDivisor = 1.0;
        bool atkGate = false;
        double atkRatioAttack = 1.25;
        double atkRatioRetreat = 0.8;
        bool pvpLate = true;
        int pvpLateFrame = 15000;
        int pvpLateHT = 4;
        int pvpLateReaver = 2;
        bool allinExit = true;
        int allinExitFrame = 17000;
        bool dtScoutPro = false;
        bool oppResponse = false;
        bool exGate = false;
        double exGateMinGain = 0.05;
        bool bMicro = false;
        bool stormTiming = false;
        bool stormAmulet = false;
        bool archonMerge = false;
        bool stormCast = false;
        int stormCastMin = 3;
        bool stormWin = false;
        int stormWinFrame = 13000;
        bool fapHt = false;
        bool armyComp = false;
        double armyCompRatio = 0.28;
        bool stormGuard = false;
        double stormGuardPen = 0.55;
        bool windowPush = false;
        double windowPushRatio = 1.15;
        int windowPushFrame = 13000;
        bool expandThrottle = false;
        int expandThrottleFrame = 15600;
        nlohmann::json strategyLibrary = nlohmann::json::array();
        nlohmann::json recognizerTable = nlohmann::json::object();
        nlohmann::json fingerprintVocab = nlohmann::json::object();
        std::string status = "defaults (not loaded)";

        /* Resolution order mirrors FileTools (read -> write -> AI) with our own */
        /* deployment directory first: AI/ShushanKing ships params + strategy JSON */
        /* alongside the DLL (B10 same-generation deployment). The bare */
        /* "bwapi-data/" root covers layouts that unzip the payload directly */
        /* there instead of into the AI subfolder (2026-10-13 robustness ask). */
        const std::vector<std::string> configRoots = {
                "bwapi-data/AI/ShushanKing/",
                "bwapi-data/",
                "bwapi-data/read/",
                "bwapi-data/write/",
        };

        std::string resolve(const std::string &rel);

        /* Directory of this DLL itself: params/strategies shipped alongside */
        /* the binary are found regardless of what folder name the installer */
        /* uses (AIIDE alias deployment renames the AI subfolder). Queried */
        /* once per process; failure falls through to the fixed roots. */
        std::string selfDirectory()
        {
            HMODULE mod = nullptr;
            if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                    reinterpret_cast<LPCSTR>(&resolve), &mod))
                return "";
            char buf[MAX_PATH] = {0};
            DWORD n = GetModuleFileNameA(mod, buf, MAX_PATH);
            if (n == 0 || n >= MAX_PATH) return "";
            std::string p(buf, n);
            std::size_t slash = p.find_last_of("\\/");
            return (slash == std::string::npos) ? std::string()
                                                : p.substr(0, slash + 1);
        }

        std::string resolve(const std::string &rel)
        {
            static const std::string dllDir = selfDirectory();
            if (!dllDir.empty())
            {
                std::string p = dllDir + rel;
                if (std::filesystem::exists(p)) return p;
            }
            for (auto &root : configRoots)
            {
                std::string p = root + rel;
                if (std::filesystem::exists(p)) return p;
            }
            return "";
        }

        std::string readAll(const std::string &path)
        {
            std::ifstream in(path);
            if (!in.good()) return "";
            std::ostringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }
    }

    void initialize()
    {
        if (initialized) return;
        initialized = true;

        /* params.json (B6): version-gated; any failure -> built-in defaults */
        std::string path = resolve("params.json");
        if (path.empty())
        {
            status = "FALLBACK defaults (params.json not found)";
        }
        else
        {
            try
            {
                auto params = nlohmann::json::parse(readAll(path));
                if (!params.is_object()
                    || params.find("version") == params.end()
                    || !params["version"].is_number_integer())
                {
                    status = "FALLBACK defaults (params.json schema reject: " + path + ")";
                }
                else
                {
                    std::string engine = params.value("strategy_engine", "legacy");
                    newEngine = engine == "new";
                    status = "ok " + path + " v" + std::to_string(params["version"].get<int>())
                             + " engine=" + engine;

                    /* A4 frame governor (S5): {"mode": "measure|ladder", */
                    /* "limit_ms": int, "skip_max": int}; missing keys keep defaults */
                    /* S8 producer engine + capacity governor params */
                    prodEngine = params.value("producer_engine", "legacy") == "new";
                    auto cap = params.find("producer_capacity");
                    if (cap != params.end() && cap->is_object())
                    {
                        capGatesPerBase = cap->value("gates_per_base", capGatesPerBase);
                        capMinGates = cap->value("min_gates", capMinGates);
                        capMaxGates = cap->value("max_gates", capMaxGates);
                        capFromFrame = cap->value("from_frame", capFromFrame);
                    }

                    /* P2-B shadow / P3 selection_scoring: retired keys (authority
                     * flipped 2026-09-16/17); params entries ignored if present. */

                    /* AIIDE2025 absorb: dt_suspect "off"|"record"|"on"; d4_transitions "off"|"on" */
                    std::string dtSus = params.value("dt_suspect", "off");
                    dtSuspect = dtSus == "on" ? 2 : (dtSus == "record" ? 1 : 0);
                    if (dtSuspect) status += " dtSuspect=" + dtSus;
                    d4Trans = params.value("d4_transitions", "off") == "on";
                    if (d4Trans) status += " d4=on";

                    /* PW/BB audit knobs */
                    auto cap2 = params.find("producer_capacity");
                    if (cap2 != params.end() && cap2->is_object())
                    {
                        capBankDivisor = cap2->value("bank_divisor", capBankDivisor);
                        if (capBankDivisor < 1.0) capBankDivisor = 1.0;
                    }
                    auto gate = params.find("attack_ratio_gate");
                    if (gate != params.end() && gate->is_object())
                    {
                        atkGate = gate->value("enabled", false);
                        atkRatioAttack = gate->value("attack", atkRatioAttack);
                        atkRatioRetreat = gate->value("retreat", atkRatioRetreat);
                        if (atkGate) status += " atkGate=on";
                    }

                    /* P1-A/P1-B (2026-09-20 report v2): PvP late-game transition */
                    /* and one-base exit gates. */
                    auto late = params.find("pvp_lategame");
                    if (late != params.end() && late->is_object())
                    {
                        pvpLate = late->value("enabled", pvpLate);
                        pvpLateFrame = late->value("transition_frame", pvpLateFrame);
                        pvpLateHT = late->value("ht_target", pvpLateHT);
                        pvpLateReaver = late->value("reaver_target", pvpLateReaver);
                        allinExit = late->value("allin_exit_enabled", allinExit);
                        allinExitFrame = late->value("allin_exit_frame", allinExitFrame);
                        if (pvpLate) status += " late=on";
                        if (allinExit) status += " allinExit=on";
                    }

                    /* Direction 1/2 (2026-09-24 plan): both default false, so an */
                    /* unmodified params.json keeps 3d056f10 behavior. */
                    dtScoutPro = params.value("dt_scout_proactive", dtScoutPro);
                    if (dtScoutPro) status += " dtScout=pro";
                    oppResponse = params.value("opponent_response", oppResponse);
                    if (oppResponse) status += " oppResp=on";

                    /* Direction C (2026-09-25 forensics): FAP exchange-ratio gate. */
                    /* Battle forensics Q1: SD loss games have 3x battles and higher */
                    /* own density = self-feeding fights. Gate refuses engagements the */
                    /* sim predicts as a net percentage loss. Off by default. */
                    auto exg = params.find("exchange_gate");
                    if (exg != params.end() && exg->is_object())
                    {
                        exGate = exg->value("enabled", exGate);
                        exGateMinGain = exg->value("min_percent_gain", exGateMinGain);
                        if (exGate) status += " exGate=on";
                    }

                    /* Direction B (2026-09-26 forensics Q4): combat-kiting splash */
                    /* spread. Off by default. */
                    auto bm = params.find("battle_micro");
                    if (bm != params.end() && bm->is_object())
                    {
                        bMicro = bm->value("enabled", bMicro);
                    }
                    if (bMicro) status += " bMicro=on";

                    /* Direction "storm timing" (2026-09-28 forensics): cash in the */
                    /* Archives lead. Off by default. */
                    auto st = params.find("storm_timing");
                    if (st != params.end() && st->is_object())
                    {
                        stormTiming = st->value("enabled", stormTiming);
                        if (stormTiming) status += " stormTiming=on";
                    }
                    auto sa = params.find("storm_amulet");
                    if (sa != params.end() && sa->is_object())
                    {
                        stormAmulet = sa->value("enabled", stormAmulet);
                        if (stormAmulet) status += " stormAmulet=on";
                    }
                    auto am = params.find("archon_merge");
                    if (am != params.end() && am->is_object())
                    {
                        archonMerge = am->value("enabled", archonMerge);
                    }
                    if (archonMerge) status += " archonMerge=on";

                    /* Direction "storm casting" (2026-09-29): HT storm-cast micro. */
                    /* Off by default. */
                    auto sc = params.find("storm_cast");
                    if (sc != params.end() && sc->is_object())
                    {
                        stormCast = sc->value("enabled", stormCast);
                        stormCastMin = sc->value("min_enemies", stormCastMin);
                    }
                    if (stormCast) status += " stormCast=on/" + std::to_string(stormCastMin);

                    /* Direction "storm window" (2026-09-30): cash in before the */
                    /* enemy storm line forms. Off by default. */
                    auto sw = params.find("storm_window");
                    if (sw != params.end() && sw->is_object())
                    {
                        stormWin = sw->value("enabled", stormWin);
                        stormWinFrame = sw->value("min_frame", stormWinFrame);
                    }
                    if (stormWin) status += " stormWindow=on/" + std::to_string(stormWinFrame);

                    /* Direction "fap_ht_value" (2026-09-30): count HTs into the */
                    /* sim initial value totals. Off by default. */
                    auto fh = params.find("fap_ht_value");
                    if (fh != params.end() && fh->is_object())
                    {
                        fapHt = fh->value("enabled", fapHt);
                    }
                    if (fapHt) status += " fapHtValue=on";

                    /* Direction "army composition" (O4, 2026-09-30): zealot ratio */
                    /* mix for the PvP midgame army. Off by default. */
                    auto ac = params.find("army_composition");
                    if (ac != params.end() && ac->is_object())
                    {
                        armyComp = ac->value("enabled", armyComp);
                        armyCompRatio = ac->value("zealot_ratio", armyCompRatio);
                    }
                    if (armyComp) status += " armyComp=on/" + std::to_string(armyCompRatio).substr(0, 4);

                    /* 50%-plan packages (2026-10-08 funnel analysis). Off by default. */
                    auto sg = params.find("storm_guard");
                    if (sg != params.end() && sg->is_object())
                    {
                        stormGuard = sg->value("enabled", stormGuard);
                        stormGuardPen = sg->value("penalty", stormGuardPen);
                    }
                    if (stormGuard) status += " stormGuard=on/" + std::to_string(stormGuardPen).substr(0, 4);
                    auto wp = params.find("window_push");
                    if (wp != params.end() && wp->is_object())
                    {
                        windowPush = wp->value("enabled", windowPush);
                        windowPushRatio = wp->value("army_ratio", windowPushRatio);
                        windowPushFrame = wp->value("min_frame", windowPushFrame);
                    }
                    if (windowPush) status += " windowPush=on/" + std::to_string(windowPushRatio).substr(0, 4)
                                              + "@" + std::to_string(windowPushFrame);
                    auto et = params.find("expand_throttle");
                    if (et != params.end() && et->is_object())
                    {
                        expandThrottle = et->value("enabled", expandThrottle);
                        expandThrottleFrame = et->value("min_frame", expandThrottleFrame);
                    }
                    if (expandThrottle) status += " expandThrottle=on@" + std::to_string(expandThrottleFrame);

                    /* S6: recognizer thresholds loading happens after this block */
                    auto gov = params.find("frame_governor");
                    if (gov != params.end() && gov->is_object())
                    {
                        govMode = gov->value("mode", govMode);
                        govLimitMs = gov->value("limit_ms", govLimitMs);
                        govSkipMax = gov->value("skip_max", govSkipMax);
                        status += " gov=" + govMode + "/" + std::to_string(govLimitMs)
                                  + "ms/skip" + std::to_string(govSkipMax);
                    }
                }
            }
            catch (std::exception &ex)
            {
                status = std::string("FALLBACK defaults (params.json parse: ") + ex.what() + ")";
            }
        }

        // strategies/*.json: each file validated independently; invalid files are
        /* skipped. Library default = empty, which the legacy engine ignores, so a */
        /* bad strategy file can never keep the bot from playing. */
        int loaded = 0;
        int skipped = 0;
        for (auto &root : configRoots)
        {
            std::string dir = root + "strategies";
            if (!std::filesystem::is_directory(dir)) continue;
            for (auto &entry : std::filesystem::directory_iterator(dir))
            {
                if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
                try
                {
                    auto strat = nlohmann::json::parse(readAll(entry.path().string()));
                    if (strat.is_object()
                        && strat.find("name") != strat.end() && strat["name"].is_string()
                        && strat.find("matchup") != strat.end() && strat["matchup"].is_string()
                        && (strat.find("opening") == strat.end() || strat["opening"].is_array()))
                    {
                        strategyLibrary.push_back(strat);
                        loaded++;
                    }
                    else
                    {
                        skipped++;
                    }
                }
                catch (std::exception &)
                {
                    skipped++;
                }
            }
            break; // first root containing a strategies dir wins
        }
        status += " strategies=" + std::to_string(loaded);
        if (skipped) status += " (skipped " + std::to_string(skipped) + " invalid)";

        /* S6: recognizer thresholds + fingerprint vocabulary (both optional; */
        /* parse failures degrade to empty tables, consumers keep defaults) */
        auto loadTopLevel = [&](const std::string &rel, nlohmann::json &out, const char *tag)
        {
            std::string p = resolve(rel);
            if (p.empty()) return;
            try
            {
                auto j = nlohmann::json::parse(readAll(p));
                if (j.is_object()) out = j;
                else status += std::string(" ") + tag + "(schema reject)";
            }
            catch (std::exception &)
            {
                status += std::string(" ") + tag + "(parse fail)";
            }
        };
        loadTopLevel("recognizers.json", recognizerTable, "recognizers");
        loadTopLevel("fingerprints.json", fingerprintVocab, "fingerprints");
        fpVersion = fingerprintVocab.value("version", 0);
    }

    bool newStrategyEngine()
    {
        return newEngine;
    }

    std::string governorMode()
    {
        return govMode;
    }

    bool governorLadder()
    {
        return govMode == "ladder";
    }

    int governorLimitMs()
    {
        return govLimitMs;
    }

    int governorSkipMax()
    {
        return govSkipMax;
    }

    bool producerEngine()
    {
        return prodEngine;
    }

    int capacityGatesPerBase()
    {
        return capGatesPerBase;
    }

    int capacityMinGates()
    {
        return capMinGates;
    }

    int capacityMaxGates()
    {
        return capMaxGates;
    }

    int capacityFromFrame()
    {
        return capFromFrame;
    }

    const nlohmann::json &strategies()
    {
        return strategyLibrary;
    }

    const nlohmann::json &recognizers()
    {
        return recognizerTable;
    }

    const nlohmann::json &fingerprints()
    {
        return fingerprintVocab;
    }

    int fingerprintVersion()
    {
        return fpVersion;
    }

    int dtSuspectMode()
    {
        return dtSuspect;
    }

    bool d4Transitions()
    {
        return d4Trans;
    }

    double capacityBankDivisor()
    {
        return capBankDivisor;
    }

    bool attackRatioGateEnabled()
    {
        return atkGate;
    }

    double attackRatioAttack()
    {
        return atkRatioAttack;
    }

    double attackRatioRetreat()
    {
        return atkRatioRetreat;
    }

    bool pvpLateTransitionEnabled()
    {
        return pvpLate;
    }

    int pvpLateTransitionFrame()
    {
        return pvpLateFrame;
    }

    int pvpLateTransitionHTTarget()
    {
        return pvpLateHT;
    }

    int pvpLateTransitionReaverTarget()
    {
        return pvpLateReaver;
    }

    bool pvpAllinExitEnabled()
    {
        return allinExit;
    }

    int pvpAllinExitFrame()
    {
        return allinExitFrame;
    }

    bool dtScoutProactive()
    {
        return dtScoutPro;
    }

    bool opponentResponse()
    {
        return oppResponse;
    }

    bool exchangeGateEnabled()
    {
        return exGate;
    }

    double exchangeGateMinGain()
    {
        return exGateMinGain;
    }

    bool battleMicroEnabled()
    {
        return bMicro;
    }

    bool stormTimingEnabled()
    {
        return stormTiming;
    }

    bool stormAmuletEnabled()
    {
        return stormAmulet;
    }

    bool archonMergeEnabled()
    {
        return archonMerge;
    }

    bool stormCastEnabled()
    {
        return stormCast;
    }

    int stormCastMinEnemies()
    {
        return stormCastMin;
    }

    bool stormWindowEnabled()
    {
        return stormWin;
    }

    int stormWindowMinFrame()
    {
        return stormWinFrame;
    }

    bool fapHtValueEnabled()
    {
        return fapHt;
    }

    bool armyCompositionEnabled()
    {
        return armyComp;
    }

    double armyCompositionZealotRatio()
    {
        return armyCompRatio;
    }

    bool stormGuardEnabled()
    {
        return stormGuard;
    }

    double stormGuardPenalty()
    {
        return stormGuardPen;
    }

    bool windowPushEnabled()
    {
        return windowPush;
    }

    double windowPushArmyRatio()
    {
        return windowPushRatio;
    }

    int windowPushMinFrame()
    {
        return windowPushFrame;
    }

    bool expandThrottleEnabled()
    {
        return expandThrottle;
    }

    int expandThrottleMinFrame()
    {
        return expandThrottleFrame;
    }

    namespace WindowPush
    {
        bool fired = false;
    }

    std::string statusLine()
    {
        return "Config: " + status;
    }
}
