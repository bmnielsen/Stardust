#include "MicroQueens.h"

#include "Bases.h"
#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// The queen is probably about to die. It should cast immediately if it is ever going to.
bool MicroQueens::aboutToDie(const BWAPI::Unit queen) const
{
	return
		queen->getHitPoints() < 30 ||
		queen->isIrradiated() ||
		queen->isPlagued();
}

// This unit is nearby. How much do we want to parasite it?
// Scores >= 100 are worth parasiting now. Scores < 100 are worth it if the queen is about to die.
int MicroQueens::parasiteScore(BWAPI::Unit u) const
{
	if (u->getPlayer() == BWAPI::Broodwar->neutral())
	{
		if (u->isFlying())
		{
			// It's a flying critter--worth tagging.
			return 100;
		}
		return 1;
	}

	// It's an enemy unit.

	BWAPI::UnitType type = u->getType();

	if (type == BWAPI::UnitTypes::Protoss_Arbiter)
	{
		return 110;
	}

	if (type == BWAPI::UnitTypes::Terran_Battlecruiser ||
		type == BWAPI::UnitTypes::Terran_Dropship ||
		type == BWAPI::UnitTypes::Terran_Science_Vessel || 

		type == BWAPI::UnitTypes::Protoss_Carrier ||
		type == BWAPI::UnitTypes::Protoss_Shuttle)
	{
		return 101;
	}

	if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
		type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode || 
		type == BWAPI::UnitTypes::Terran_Valkyrie ||
		
		type == BWAPI::UnitTypes::Protoss_Corsair ||
		type == BWAPI::UnitTypes::Protoss_Archon ||
		type == BWAPI::UnitTypes::Protoss_Dark_Archon ||
		type == BWAPI::UnitTypes::Protoss_Reaver ||
		type == BWAPI::UnitTypes::Protoss_Scout)
	{
		return 70;
	}

	if (type.isWorker() ||
		type == BWAPI::UnitTypes::Terran_Ghost ||
		type == BWAPI::UnitTypes::Terran_Medic ||
		type == BWAPI::UnitTypes::Terran_Wraith ||
		type == BWAPI::UnitTypes::Protoss_Observer)
	{
		return 60;
	}

	// A random enemy is worth something to parasite--but not much.
	return 2;
}

// Score units, pick the one with the highest score and maybe parasite it.
bool MicroQueens::maybeParasite(BWAPI::Unit queen)
{
	// Parasite has range 12. We look for targets within the limit range.
	const int limit = 12 + 1;

	BWAPI::Unitset targets = BWAPI::Broodwar->getUnitsInRadius(queen->getPosition(), limit * 32,
		!BWAPI::Filter::IsBuilding && (BWAPI::Filter::IsEnemy || BWAPI::Filter::IsCritter) &&
		!BWAPI::Filter::IsInvincible && !BWAPI::Filter::IsParasited);

	if (targets.empty())
	{
		return false;
	}

	// Look for the target with the best score.
	int bestScore = 0;
	BWAPI::Unit bestTarget = nullptr;
	for (BWAPI::Unit target : targets)
	{
		int score = parasiteScore(target);
		if (score > bestScore)
		{
			bestScore = score;
			bestTarget = target;
		}
	}

	if (bestTarget)
	{
		// Parasite something important.
		// Or, if the queen is at full energy, parasite something reasonable.
		// Or, if the queen is about to die, parasite anything.
		if (bestScore >= 100 ||
            bestScore >= 50 && queen->getEnergy() >= 200 && !BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Spawn_Broodlings) ||
            bestScore >= 50 && queen->getEnergy() >= 225 && BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Spawn_Broodlings) ||
			aboutToDie(queen))
		{
			//BWAPI::Broodwar->printf("parasite score %d on %s @ %d,%d",
			//	bestScore, UnitTypeName(bestTarget->getType()).c_str(), bestTarget->getPosition().x, bestTarget->getPosition().y);
			setReadyToCast(queen, CasterSpell::Parasite);
			return spell(queen, BWAPI::TechTypes::Parasite, bestTarget);
		}
	}

	return false;
}

// How much do we want to ensnare this unit?
int MicroQueens::ensnareScore(BWAPI::Unit u) const
{
    const BWAPI::UnitType type = u->getType();

    // Stuff that can't be ensnared, and spider mines. A spider mine above the ground is
    // unlikely to be affected by ensnare; it will explode or burrow too soon.
    if (type.isBuilding() ||
        u->isEnsnared() ||
        u->isBurrowed() ||
        type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
        u->isInvincible())
    {
        return 0;
    }

    int score = 1;

    // If it's cloaked, give it a bonus--a bigger bonus if it is not detected.
    if (!u->isDetected())
    {
        score += 80;
    }
    else if (u->isCloaked())
    {
        score += 40;		// because ensnare will keep it detected
    }

    if (type.isWorker())
    {
        score += 5;
    }
    else if (type.whatBuilds().first == BWAPI::UnitTypes::Terran_Barracks)
    {
        score += 10;
    }
    else if (
        type == BWAPI::UnitTypes::Terran_Wraith ||
        type == BWAPI::UnitTypes::Terran_Valkyrie ||
        type == BWAPI::UnitTypes::Protoss_Corsair ||
        type == BWAPI::UnitTypes::Protoss_Scout ||
        type == BWAPI::UnitTypes::Zerg_Mutalisk)
    {
        score += 33;
    }
    else if (
        type == BWAPI::UnitTypes::Terran_Dropship ||
        type == BWAPI::UnitTypes::Protoss_Shuttle)
    {
        score += 33;
    }
    else if (type == BWAPI::UnitTypes::Zerg_Scourge)
    {
        score += 15;
    }
    else if (
        type == BWAPI::UnitTypes::Terran_Vulture ||
        type == BWAPI::UnitTypes::Protoss_Zealot ||
        type == BWAPI::UnitTypes::Protoss_Dragoon ||
        type == BWAPI::UnitTypes::Protoss_Archon ||
        type == BWAPI::UnitTypes::Protoss_Dark_Archon ||
        type == BWAPI::UnitTypes::Zerg_Zergling ||
        type == BWAPI::UnitTypes::Zerg_Hydralisk ||
        type == BWAPI::UnitTypes::Zerg_Ultralisk)
    {
        score += 10;
    }
    else
    {
        score += int(type.topSpeed());      // value up to 6
    }

    return score;
}

// We can ensnare. Look around to see if we should, and if so, do it.
bool MicroQueens::maybeEnsnare(BWAPI::Unit queen)
{
    // Ensnare has range 9 and affects a 4x4 box. We look a little beyond that range for targets.
    const int limit = 9 + 2;

    const bool dying = aboutToDie(queen);

    // Don't bother to look for units to ensnare if no enemy is close enough.
    BWAPI::Unit closest = BWAPI::Broodwar->getClosestUnit(queen->getPosition(),
        BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsBuilding,
        limit * 32);

    if (!dying && !closest)
    {
        return false;
    }

    // Look for the 4x4 box with the best effect.
    int bestScore = 0;
    BWAPI::Position bestPlace;
    for (int tileX = std::max(2, queen->getTilePosition().x - limit); tileX <= std::min(BWAPI::Broodwar->mapWidth() - 3, queen->getTilePosition().x + limit); ++tileX)
    {
        for (int tileY = std::max(2, queen->getTilePosition().y - limit); tileY <= std::min(BWAPI::Broodwar->mapHeight() - 3, queen->getTilePosition().y + limit); ++tileY)
        {
            int score = 0;
            BWAPI::Position place(BWAPI::TilePosition(tileX, tileY));
            const BWAPI::Position offset(2 * 32, 2 * 32);
            BWAPI::Unitset affected = BWAPI::Broodwar->getUnitsInRectangle(place - offset, place + offset);
            for (BWAPI::Unit u : affected)
            {
                if (u->getPlayer() == BWAPI::Broodwar->self())
                {
                    score -= ensnareScore(u);
                }
                else if (u->getPlayer() == BWAPI::Broodwar->enemy())
                {
                    score += ensnareScore(u);
                }
            }
            if (score > bestScore)
            {
                bestScore = score;
                bestPlace = place;
            }
        }
    }

    if (bestScore > 0)
    {
        // BWAPI::Broodwar->printf("ensnare score %d at %d,%d", bestScore, bestPlace.x, bestPlace.y);
    }

    if (bestScore > 100 || dying && bestScore > 0)
    {
        setReadyToCast(queen, CasterSpell::Ensnare);
        return spell(queen, BWAPI::TechTypes::Ensnare, bestPlace);
    }

    return false;
}

// This unit is nearby. How much do we want to broodling it?
// Scores >= 100 are worth killing now.
// Scores >= 50 are worth it if the queen has 250 energy.
// Scores > 0 are worth it if the queen is about to die.
int MicroQueens::broodlingScore(BWAPI::Unit queen, BWAPI::Unit u) const
{
    // Extra points if the unit is defense matrixed.
    // Fewer points if the unit is already damaged. Shield damage counts.
    // More points if the unit is in range, so that the queen does not have to move.
    const int bonus =
        (u->isDefenseMatrixed() ? 20 : 0) +
        int(std::round(30.0 * double(u->getHitPoints() + u->getShields()) / (u->getType().maxHitPoints() + u->getType().maxShields()))) - 30 +
        ((queen->getDistance(u) <= 288) ? 25 : 0);

    if (u->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
        u->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
        u->getType() == BWAPI::UnitTypes::Protoss_High_Templar ||
        u->getType() == BWAPI::UnitTypes::Zerg_Defiler)
    {
        return 110 + bonus;
    }

    if (u->getType() == BWAPI::UnitTypes::Zerg_Ultralisk)
    {
        return 100 + bonus;
    }

    if (u->getType().gasPrice() > 0)
    {
        return 50 + u->getType().gasPrice() / 10 + bonus;
    }

    return u->getType().mineralPrice() / 10 + bonus;
}

// Score units, pick the one with the highest score and maybe broodling it.
bool MicroQueens::maybeBroodling(BWAPI::Unit queen)
{
    // Spawn broodlings has range 9. We look for targets within the limit range.
    const int limit = 9 + 3;

    // Ignore the possibility that you may want to broodling a non-enemy unit.
    // E.g., a neutral critter, so the broodlings can scout or tear down an unattended building.
    // Or maybe your own larva, say for your defiler to consume.
    BWAPI::Unitset targets = BWAPI::Broodwar->getUnitsInRadius(queen->getPosition(), limit * 32,
        BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsBuilding && !BWAPI::Filter::IsFlyer &&
        !BWAPI::Filter::IsRobotic &&            // not probe or reaver
        BWAPI::Filter::GetType != BWAPI::UnitTypes::Protoss_Archon &&
        BWAPI::Filter::GetType != BWAPI::UnitTypes::Protoss_Dark_Archon &&
        BWAPI::Filter::IsDetected &&
        !BWAPI::Filter::IsInvincible);

    if (targets.empty())
    {
        return false;
    }

    // Look for the target with the best score.
    int bestScore = 0;
    BWAPI::Unit bestTarget = nullptr;
    for (BWAPI::Unit target : targets)
    {
        int score = broodlingScore(queen, target);
        if (score > bestScore)
        {
            bestScore = score;
            bestTarget = target;
        }
    }

    if (bestTarget)
    {
        // Broodling something important.
        // Or, if the queen is at full energy, broodling something reasonable.
        // Or, if the queen is about to die, broodling anything.
        if (bestScore >= 100 ||
            bestScore >= 50 && queen->getEnergy() == 250 ||
            aboutToDie(queen))
        {
            // BWAPI::Broodwar->printf("broodling score %d on %s @ %d,%d",
            //    bestScore, UnitTypeName(bestTarget->getType()).c_str(), bestTarget->getPosition().x, bestTarget->getPosition().y);
            setReadyToCast(queen, CasterSpell::Broodling);
            return spell(queen, BWAPI::TechTypes::Spawn_Broodlings, bestTarget);
        }
    }

    return false;
}

// Each queen tries to avoid danger and keep a distance from other queens, when possible,
// while moving to the general area of the target position.
BWAPI::Position MicroQueens::getQueenDestination(BWAPI::Unit queen, const BWAPI::Position & target) const
{
    BWAPI::Race enemyRace = BWAPI::Broodwar->enemy()->getRace();
    const int dangerRadius = enemyRace == BWAPI::Races::Terran ? 9 : (enemyRace == BWAPI::Races::Protoss ? 7 : 6);
    const int keepAwayRadius = 4;

    BWAPI::Unit danger = BWAPI::Broodwar->getClosestUnit(queen->getPosition(),
        BWAPI::Filter::IsEnemy && BWAPI::Filter::AirWeapon != BWAPI::WeaponTypes::None,
        32 * dangerRadius);

    if (danger)
    {
        return DistanceAndDirection(queen->getPosition(), danger->getPosition(), -dangerRadius * 32);
    }

    int closestDist = 32 * keepAwayRadius;
    BWAPI::Unit sister = nullptr;
    for (BWAPI::Unit q : getUnits())
    {
        if (q != queen && queen->getDistance(q) < closestDist)
        {
            closestDist = queen->getDistance(q);
            sister = q;
        }
    }

    if (sister)
    {
        return DistanceAndDirection(queen->getPosition(), sister->getPosition(), -keepAwayRadius * 32);
    }

    return target;
}

void MicroQueens::updateMovement(BWAPI::Unit vanguard)
{
    for (BWAPI::Unit queen : getUnits())
    {
        // If it's intending to cast, we don't want to interrupt by moving.
        if (isReadyToCast(queen))
        {
            continue;
        }

        // Default destination if all else fails: The main base.
        BWAPI::Position destination = Bases::Instance().myMainBase()->getPosition();

        // If we can see an infestable command center, move toward it.
        int nearestRange = 999999;
        BWAPI::Unit nearestCC = nullptr;
        for (BWAPI::Unit enemy : BWAPI::Broodwar->enemy()->getUnits())
        {
            if (enemy->getType() == BWAPI::UnitTypes::Terran_Command_Center &&
                enemy->getHitPoints() < 750 &&
                enemy->isCompleted() &&
                queen->getDistance(enemy) < nearestRange)
            {
                nearestRange = queen->getDistance(enemy);
                nearestCC = enemy;
            }
        }
        if (nearestCC)
        {
            destination = nearestCC->getPosition();
        }

        // Broodling costs 150 energy. Ensnare and parasite each cost 75.
        else if (vanguard &&
            queen->getEnergy() >= (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Spawn_Broodlings) ? 135 : 65))
        {
            destination = vanguard->getPosition();
        }
        else if (queen->getEnergy() >= (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Spawn_Broodlings) ? 150 : 75))
        {
            // No vanguard, but we have energy. Retreat to the front defense line and try to be useful.
            // This can happen when all units are assigned to defense squads.
            destination = BWAPI::Position(Bases::Instance().frontPoint());
        }

        if (destination.isValid())
        {
            the.micro.MoveNear(queen, getQueenDestination(queen, destination));
        }
    }
}

// Cast broodling or parasite if possible and useful.
void MicroQueens::updateAction()
{
	for (BWAPI::Unit queen : getUnits())
	{
        bool dying = aboutToDie(queen);

        if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Spawn_Broodlings))
        {
            // If we have broodling, then broodling is the highest priority.
            if (queen->getEnergy() >= 150 && maybeBroodling(queen))
            {
                // Broodling is set to be cast. No more to do.
            }
            else if (queen->getEnergy() >= 225 ||               // enough that broodling can happen after another spell
                dying && queen->getEnergy() >= 75)
            {
                // We have energy for either ensnare or parasite.
                if (!BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Ensnare) || !maybeEnsnare(queen))
                {
                    // Ensnare is not researched, or was not cast on this attempt. Consider parasite.
                    if (queen->getEnergy() == 250 || dying)
                    {
                        (void) maybeParasite(queen);
                    }
                }
            }
        }
        else if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Ensnare))
        {
            // Ensnare but not broodling is available. Ensnare is higher priority than parasite.
            if (queen->getEnergy() >= 75 && maybeEnsnare(queen))
            {
                // Ensnare is set to be cast. No more to do.
            }
            else if (queen->getEnergy() >= 150 ||               // enough that ensnare can happen after parasite
                dying && queen->getEnergy() >= 75)
            {
                (void) maybeParasite(queen);
            }
        }
		else if (queen->getEnergy() >= 75)
		{
            // Parasite is the only possibility.
			(void) maybeParasite(queen);
		}
	}
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

MicroQueens::MicroQueens()
{ 
}

// Unused but required.
void MicroQueens::executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster)
{
}

// Control the queens.
// Queens are not clustered.
void MicroQueens::update(BWAPI::Unit vanguard)
{
	if (getUnits().empty())
	{
		return;
	}

	updateCasters(getUnits());

	const int phase = BWAPI::Broodwar->getFrameCount() % 12;

	if (phase == 0)
	{
		updateMovement(vanguard);
	}
	else if (phase == 6)
	{
		updateAction();
	}
}
