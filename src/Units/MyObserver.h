#pragma once

#include "MyUnit.h"

class MyObserverImpl;

typedef std::shared_ptr<MyObserverImpl> MyObserver;

// Activities that apply to observers attached to an army
// Observers being used for scouting enemy bases are handled fully by the scouting play
enum class ObserverActivity
{
    // The observer has no activity yet
    None,

    // The observer is moving to detect a specific enemy so our army can attack it
    DetectingEnemy,

    // The observer is escorting the army, trying to stay close to its vanguard unit
    EscortingArmy,

    // The observer is scouting the enemy army to get better information for our combat sim
    ScoutingEnemyArmy
};

namespace
{
    std::string toString(const ObserverActivity &activity)
    {
        switch (activity)
        {
            case ObserverActivity::None:
                return "None";
            case ObserverActivity::DetectingEnemy:
                return "DetectingEnemy";
            case ObserverActivity::EscortingArmy:
                return "EscortingArmy";
            case ObserverActivity::ScoutingEnemyArmy:
                return "ScoutingEnemyArmy";
        }
        return "UNKNOWN";
    }
}

class MyObserverImpl : public MyUnitImpl
{
public:
    explicit MyObserverImpl(BWAPI::Unit unit) : MyUnitImpl(unit), activity(ObserverActivity::None), frameActivityUpdated(-2) {}

    [[nodiscard]] ObserverActivity getActivity() const
    {
        if (frameActivityUpdated >= (currentFrame - 1)) return activity;
        return ObserverActivity::None;
    }

    void setActivity(const ObserverActivity newActivity)
    {
#if CHERRYVIS_ENABLED
        if (newActivity != activity)
        {
            CherryVis::log(id) << "Activity changed from " << toString(activity) << " to " << toString(newActivity);
        }
#endif

        activity = newActivity;
        frameActivityUpdated = currentFrame;
    }

private:
    ObserverActivity activity;
    int frameActivityUpdated;
};
