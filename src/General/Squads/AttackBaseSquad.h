#pragma once

#include "Squad.h"
#include "Base.h"

namespace
{
    std::string squadLabel(Base *baseLoc, const std::string &playLabel)
    {
        std::ostringstream builder;
        if (!playLabel.empty())
        {
            builder << playLabel << ":";
        }
        builder << "Attack base @ " << baseLoc->getTilePosition();
        return builder.str();
    }
}

class AttackBaseSquad : public UnitSquad
{
public:
    explicit AttackBaseSquad(Base *baseLoc, const std::string &playLabel = "")
            : UnitSquad(squadLabel(baseLoc, playLabel))
            , baseLoc(baseLoc)
            , ignoreCombatSim(false)
    {
        targetPosition = baseLoc->getPosition();
    };

    virtual ~AttackBaseSquad() = default;

    Base *baseLoc;
    bool ignoreCombatSim;

private:
    void execute(UnitCluster &clump) override;
};
