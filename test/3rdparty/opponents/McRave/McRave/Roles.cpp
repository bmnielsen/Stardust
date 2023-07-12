#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Roles {

    namespace {

        map<Role, int> myRoles;

        void updateRole(UnitInfo& unit)
        {
            // Don't assign a role to uncompleted units
            if (!unit.unit()->isCompleted() && !unit.getType().isBuilding() && unit.getType() != Zerg_Egg) {
                unit.setRole(Role::None);
                return;
            }

            // Update default role
            if (unit.getRole() == Role::None) {
                if (unit.getType().isWorker())
                    unit.setRole(Role::Worker);
                else if ((unit.getType().isBuilding() && !unit.canAttackGround() && !unit.canAttackAir() && unit.getType() != Zerg_Creep_Colony) || unit.getType() == Zerg_Larva || unit.getType() == Zerg_Egg)
                    unit.setRole(Role::Production);
                else if (unit.getType().isBuilding() && (unit.canAttackGround() || unit.canAttackAir() || unit.getType() == Zerg_Creep_Colony))
                    unit.setRole(Role::Defender);
                else if ((unit.getType().isDetector() && !unit.getType().isBuilding()) || unit.getType() == Protoss_Arbiter)
                    unit.setRole(Role::Support);
                else if (unit.getType().spaceProvided() > 0)
                    unit.setRole(Role::Transport);
                else
                    unit.setRole(Role::Combat);
            }

            // Check if a worker morphed into a building
            if (unit.getRole() == Role::Worker && unit.getType().isBuilding()) {
                if (unit.getType().isBuilding() && !unit.canAttackGround() && !unit.canAttackAir() && unit.getType() != Zerg_Creep_Colony)
                    unit.setRole(Role::Production);
                else
                    unit.setRole(Role::Defender);
            }
        }

        void updateSelf()
        {
            myRoles.clear();
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                UnitInfo &unit = *u;
                updateRole(unit);
                myRoles[unit.getRole()]++;
            }

        }
    }

    void onFrame()
    {
        updateSelf();
    }

    int getMyRoleCount(Role role) { return myRoles[role]; }
}