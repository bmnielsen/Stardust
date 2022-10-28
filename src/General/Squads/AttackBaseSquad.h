#pragma once

#include "Squad.h"
#include "Base.h"

namespace
{
    std::string squadLabel(Base *base, const std::string &playLabel)
    {
        std::ostringstream builder;
        if (!playLabel.empty())
        {
            builder << playLabel << ":";
        }
        builder << "Attack base @ " << base->getTilePosition();
        return builder.str();
    }
}

class AttackBaseSquad : public Squad
{
public:
    explicit AttackBaseSquad(Base *base, const std::string &playLabel = "")
            : Squad(squadLabel(base, playLabel))
            , base(base)
            , ignoreCombatSim(false)
    {
        targetPosition = base->getPosition();
    };

    virtual ~AttackBaseSquad() = default;

    Base *base;
    bool ignoreCombatSim;

private:
    void execute(UnitCluster &cluster) override;
};
