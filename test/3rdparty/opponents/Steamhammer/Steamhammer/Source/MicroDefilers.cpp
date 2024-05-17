#include "MicroDefilers.h"

#include "Bases.h"
#include "The.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// NOTE
// The computations to decide whether and where to swarm and plague are expensive.
// Don't have too many defilers at the same time, or you'll time out.
// A natural fix would be to designate one defiler as the "active" defiler at a
// given time, and keep the others in reserve.

// The defilers in this cluster.
// This will rarely return more than one defiler.
BWAPI::Unitset MicroDefilers::getDefilers(const UnitCluster & cluster) const
{
	BWAPI::Unitset defilers;

	for (BWAPI::Unit unit : getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Defiler &&
			cluster.units.contains(unit))
		{
			defilers.insert(unit);
		}
	}

	return defilers;
}

// The defiler is probably about to die. It should cast immediately if it is ever going to.
bool MicroDefilers::aboutToDie(const BWAPI::Unit defiler) const
{
	return
		defiler->getHitPoints() < 30 ||
		defiler->isIrradiated() ||
		defiler->isUnderStorm() ||
		defiler->isPlagued();
}

// We need to consume and have it researched. Look around for food.
// For now, we consume zerglings.
// NOTE This doesn't take latency into account, so it issues the consume order
//      repeatedly until the latency time has elapsed. It looks funny in game,
//      but there don't seem to be any bad effects.
bool MicroDefilers::maybeConsume(BWAPI::Unit defiler, BWAPI::Unitset & food)
{
	// If there is a zergling in range, snarf it down.
	// Consume has a range of 1 tile = 32 pixels.
	for (BWAPI::Unit zergling : food)
	{
		if (defiler->getDistance(zergling) <= 32 &&
			defiler->canUseTechUnit(BWAPI::TechTypes::Consume, zergling))
		{
		    // BWAPI::Broodwar->printf("consume!");
			(void) the.micro.UseTech(defiler, BWAPI::TechTypes::Consume, zergling);
			food.erase(zergling);
			return true;
		}
	}

	return false;
}

// This unit is nearby. How much does it affect the decision to cast swarm?
// Units that do damage under swarm get a positive score, others get zero.
int MicroDefilers::swarmScore(BWAPI::Unit u) const
{
	BWAPI::UnitType type = u->getType();

	if (type.isWorker())
	{
		// Workers cannot do damage under dark swarm.
		return 0;
	}
	if (type.isBuilding())
	{
		// Even if it is static defense, it cannot do damage under swarm
		// (unless it is a bunker with firebats inside, a case that we ignore).
		return 0;
	}
    if (!UnitUtil::TypeCanAttackGround(type))
    {
        return 0;
    }
    if (type == BWAPI::UnitTypes::Zerg_Lurker)
    {
        // Lurkers are especially great under swarm.
        return 2 * type.supplyRequired();
    }
	if (type == BWAPI::UnitTypes::Protoss_High_Templar ||
		type == BWAPI::UnitTypes::Protoss_Reaver)
	{
		// Special cases.
		return type.supplyRequired();
	}
	if (type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
	{
		return 1;		// it doesn't take supply
	}
	if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
	{
		return 2;		// it does splash damage only
	}
	if (type == BWAPI::UnitTypes::Protoss_Archon)
	{
		return 4;		// it does splash damage only
	}
	if (type.groundWeapon().maxRange() <= 32)
	{
		// It's a melee attacker.
		return type.supplyRequired();
	}

	// Remaining units are ranged units that cannot do damage under swarm.
	// We also ignore spellcasters that could do damage but probably won't, like
	// queens and battlecruisers.
	return 0;
}

// We can cast dark swarm. Do it if it makes sense.
// There are a lot of cases when we might want to swarm. For example:
// - Swarm defensively if the enemy has air units and we have ground units.
// - Swarm if we have melee units and the enemy has ranged units.
// - Swarm offensively to take down cannons/bunkers/sunkens.
// So far, we only implement a simple case: We're attacking toward enemy
// buildings, and the enemy is short of units that can cause damage under swarm.
// The buildings guarantee that the enemy can't simply run away without further
// consequence.

// Score units, pick the ones with higher scores and try to swarm there.
// Swarm has a range of 9 and covers a 6x6 area (according to Liquipedia) or 5x5 (according to BWAPI).
bool MicroDefilers::maybeSwarm(BWAPI::Unit defiler)
{
	// Plague has range 9 and affects a 6x6 box. We look a little beyond that range for targets.
	const int limit = 14;

	const bool dying = aboutToDie(defiler);

	// Usually, swarm only if there is an enemy building to cover or ranged unit to neutralize.
    // (The condition for ranged units is not comprehensive.)
    // NOTE A carrier does not have a ground weapon, but its interceptors do. Swarm under interceptors.
	// If the defiler is about to die, swarm may still be worth it even if it covers nothing.
	if (!dying &&
        BWAPI::Broodwar->getUnitsInRadius(defiler->getPosition(), limit * 32,
        BWAPI::Filter::IsEnemy && (BWAPI::Filter::IsBuilding ||
                                   BWAPI::Filter::IsFlyer && BWAPI::Filter::GroundWeapon != BWAPI::WeaponTypes::None ||
                                   BWAPI::Filter::GetType == BWAPI::UnitTypes::Terran_Marine ||
                                   BWAPI::Filter::GetType == BWAPI::UnitTypes::Protoss_Dragoon ||
                                   BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Hydralisk)
                                   ).empty())
    {
		return false;
	}

	// Look for the box with the best effect.
	// NOTE This is not really the calculation we want. Better would be to find the box
	// that nullifies the most enemy fire where we want to attack, no matter where the fire is from.
    // NOTE We want a dying defiler to swarm over itself only once, not repeatedly!
	int bestScore = 0;
	BWAPI::Position bestPlace = defiler->isUnderDarkSwarm() ? BWAPI::Positions::None : defiler->getPosition();
	for (int tileX = std::max(3, defiler->getTilePosition().x - limit); tileX <= std::min(BWAPI::Broodwar->mapWidth() - 4, defiler->getTilePosition().x + limit); ++tileX)
	{
		for (int tileY = std::max(3, defiler->getTilePosition().y - limit); tileY <= std::min(BWAPI::Broodwar->mapHeight() - 4, defiler->getTilePosition().y + limit); ++tileY)
		{
			int score = 0;
			bool hasRangedEnemy = false;
			BWAPI::Position place(BWAPI::TilePosition(tileX, tileY));
			const BWAPI::Position offset(3 * 32, 3 * 32);
			BWAPI::Unitset affected = BWAPI::Broodwar->getUnitsInRectangle(place - offset, place + offset);
			for (BWAPI::Unit u : affected)
			{
                if (u->isUnderDarkSwarm())
                {
                    // Reduce overlapping swarms.
                    continue;
                }
				
                if (u->getPlayer() == BWAPI::Broodwar->self())
				{
					score += swarmScore(u);
				}
				else if (u->getPlayer() == BWAPI::Broodwar->enemy())
				{
					score -= swarmScore(u);
					if (u->getType().isBuilding() && !u->getType().isAddon())
					{
						score += 2;		// enemy buildings under swarm are targets
					}
					if (u->getType().groundWeapon() != BWAPI::WeaponTypes::None &&
						u->getType().groundWeapon().maxRange() > 32)
					{
						hasRangedEnemy = true;
					}
				}
			}
			if (hasRangedEnemy && score > bestScore)
			{
				bestScore = score;
				bestPlace = place;
			}
		}
	}

	if (bestScore > 0)
	{
		// BWAPI::Broodwar->printf("swarm score %d at %d,%d", bestScore, bestPlace.x, bestPlace.y);
	}

	// NOTE If bestScore is 0, then bestPlace may be the defiler's location (set above).
	if ((bestScore > 10 || dying) && bestPlace.isValid())
	{
		setReadyToCast(defiler, CasterSpell::DarkSwarm);
		return spell(defiler, BWAPI::TechTypes::Dark_Swarm, bestPlace);
	}

	return false;
}

// How valuable is it to plague this unit?
// The caller worries about whether it is our unit or the enemy's.
int MicroDefilers::plagueScore(BWAPI::Unit u) const
{
	if (u->isPlagued() || u->isBurrowed() || u->isInvincible())
	{
		return 0;
	}

	// How many HP will it lose? Assume that it may go down to 1 HP (simple and almost right).
	int endHP = std::max(1, u->getHitPoints() - 295);
	int score = u->getHitPoints() - endHP;

	// If it's cloaked, give it a bonus--a bigger bonus if it is not detected.
	if (u->isVisible() && !u->isDetected())
	{
		score += 100;
	}
	else if (u->isCloaked())
	{
		score += 40;		// because plague will keep it detected
	}
	// If it's a static defense building, give it a bonus.
	else if (UnitUtil::IsStaticDefense(u->getType()))
	{
		score += 50;
	}
	// If it's a building other than static defense, give it a discount.
	else if (u->getType().isBuilding())
	{
        if (u->getType().getRace() == BWAPI::Races::Terran && endHP < u->getType().maxHitPoints() / 3)
        {
            // Will put the terran building into the red, so it starts to burn down.
            // Count the full score plus a small bonus.
            score += 10;
        }
        else
        {
            // Will only hurt the building modestly.
            // Discount the score.
            score = score / 3;
        }
	}
	// If it's a carrier interceptor, give it a bonus. We like plague on interceptor.
	else if (u->getType() == BWAPI::UnitTypes::Protoss_Interceptor)
	{
		score += 40;
	}

    return score;
}

// We can plague. Look around to see if we should, and if so, do it.
bool MicroDefilers::maybePlague(BWAPI::Unit defiler)
{
	// Plague has range 9 and affects a 4x4 box. We look a little beyond that range for targets.
	const int limit = 12;

    const bool dying = aboutToDie(defiler);

    // Don't bother to look for units to plague if no enemy is close enough.
    BWAPI::Unit closest = BWAPI::Broodwar->getClosestUnit(defiler->getPosition(),
        BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsBurrowed,
        limit * 32);

    if (!dying && !closest)
    {
        return false;
    }

	// Look for the box with the best effect.
	int bestScore = 0;
	BWAPI::Position bestPlace;
	for (int tileX = std::max(2, defiler->getTilePosition().x-limit); tileX <= std::min(BWAPI::Broodwar->mapWidth()-3, defiler->getTilePosition().x+limit); ++tileX)
	{
		for (int tileY = std::max(2, defiler->getTilePosition().y - limit); tileY <= std::min(BWAPI::Broodwar->mapHeight()-3, defiler->getTilePosition().y+limit); ++tileY)
		{
			int score = 0;
			BWAPI::Position place(BWAPI::TilePosition(tileX, tileY));
			const BWAPI::Position offset(2 * 32, 2 * 32);
			BWAPI::Unitset affected = BWAPI::Broodwar->getUnitsInRectangle(place - offset, place + offset);
			for (BWAPI::Unit u : affected)
			{
				if (u->getPlayer() == BWAPI::Broodwar->self())
				{
					score -= plagueScore(u);
				}
				else if (u->getPlayer() == BWAPI::Broodwar->enemy())
				{
					score += plagueScore(u);
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
		// BWAPI::Broodwar->printf("plague score %g at %d,%d", bestScore, bestPlace.x, bestPlace.y);
	}

	if (bestScore > 100 || dying && bestScore > 0)
	{
		setReadyToCast(defiler, CasterSpell::Plague);
		return spell(defiler, BWAPI::TechTypes::Plague, bestPlace);
	}

	return false;
}

MicroDefilers::MicroDefilers() 
{ 
}

// Unused but required.
void MicroDefilers::executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster)
{
}

// Consume for energy if possible and necessary; otherwise move.
void MicroDefilers::updateMovement(const UnitCluster & cluster, BWAPI::Unit vanguard)
{
    // Collect the defilers and update their states.
	BWAPI::Unitset defilers = getDefilers(cluster);
	if (defilers.empty())
	{
		return;
	}
    updateCasters(defilers);

    // BWAPI::Broodwar->printf("cluster size %d, defilers %d, energy %d", cluster.size(), defilers.size(), (*defilers.begin())->getEnergy());

	// Collect the food.
	// The food may be far away, not in the current cluster.
	BWAPI::Unitset food;
	for (BWAPI::Unit unit : getUnits())
	{
		if (unit->getType() != BWAPI::UnitTypes::Zerg_Defiler)
		{
			food.insert(unit);
		}
	}

	//BWAPI::Broodwar->printf("defiler update movement, %d food", food.size());

	// Control the defilers.
	for (BWAPI::Unit defiler : defilers)
	{
		bool canMove = !isReadyToCast(defiler);
		if (!canMove)
		{
			// BWAPI::Broodwar->printf("defiler can't move, ready to cast");
		}
		if (defiler->isBurrowed())
		{
            // Must unburrow to cast, whether we canMove or not.
			canMove = false;
			if (!defiler->isIrradiated() && defiler->canUnburrow())
			{
				the.micro.Unburrow(defiler);
			}
		}
		else if (defiler->isIrradiated() && defiler->getEnergy() < 90 && defiler->canBurrow())
		{
			// BWAPI::Broodwar->printf("defiler is irradiated");
			canMove = false;
			the.micro.Burrow(defiler);
		}

		if (canMove && defiler->getEnergy() < 150 &&
			BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Consume) &&
			!food.empty())
		{
			canMove = !maybeConsume(defiler, food);
		}

		if (canMove)
		{
			BWAPI::Position destination;

			// Figure out where the defiler should move to.
			if (vanguard)
			{
				if (defiler->getEnergy() < 100 && cluster.size() > getUnits().size())
				{
					destination = cluster.center;
				}
				else
				{
					destination = vanguard->getPosition();
				}
			}
			else
			{
                // Default destination if all else fails: The front defense line.
                destination = BWAPI::Position(Bases::Instance().frontPoint());
			}

			if (destination.isValid())
			{
				// BWAPI::Broodwar->printf("defiler %d at %d,%d will move near %d,%d",
				//	defiler->getID(), defiler->getPosition().x, defiler->getPosition().y, destination.x, destination.y);
				the.micro.MoveNear(defiler, destination);
            }
			else
			{
				// BWAPI::Broodwar->printf("defiler wants to move, has nowhere to go");
			}
		}
		else
		{
			// BWAPI::Broodwar->printf("defiler cannot move");
		}
	}

    // Control the surviving food--move it to the defiler's position.
	for (BWAPI::Unit zergling : food)
	{
		// Find the nearest defiler with low energy and move toward it.
		BWAPI::Unit bestDefiler = nullptr;
		int bestDist = 999999;
		for (BWAPI::Unit defiler : defilers)
		{
			if (defiler->getType() == BWAPI::UnitTypes::Zerg_Defiler &&
				defiler->getEnergy() < 150)
			{
				int dist = zergling->getDistance(defiler);
				if (dist < bestDist)
				{
					bestDefiler = defiler;
					bestDist = dist;
				}
			}
		}

		if (bestDefiler && zergling->getDistance(bestDefiler) >= 32)
		{
			the.micro.Move(zergling, PredictMovement(bestDefiler, 8));
            // BWAPI::Broodwar->printf("move food %d", zergling->getID());
			// BWAPI::Broodwar->drawLineMap(bestDefiler->getPosition(), zergling->getPosition(), BWAPI::Colors::Yellow);
		}
	}
}

// Cast dark swarm if possible and useful.
void MicroDefilers::updateSwarm(const UnitCluster & cluster)
{
	BWAPI::Unitset defilers = getDefilers(cluster);
	if (defilers.empty())
	{
		return;
	}
    updateCasters(defilers);

	for (BWAPI::Unit defiler : defilers)
	{
		if (!defiler->isBurrowed() &&
			defiler->getEnergy() >= 100 &&
			defiler->canUseTech(BWAPI::TechTypes::Dark_Swarm, defiler->getPosition()) &&
			!isReadyToCastOtherThan(defiler, CasterSpell::DarkSwarm))
		{
			(void) maybeSwarm(defiler);
		}
	}
}

// Cast plague if possible and useful.
void MicroDefilers::updatePlague(const UnitCluster & cluster)
{
	BWAPI::Unitset defilers = getDefilers(cluster);
	if (defilers.empty())
	{
		return;
	}
    updateCasters(defilers);

	for (BWAPI::Unit defiler : defilers)
	{
		if (!defiler->isBurrowed() &&
			defiler->getEnergy() >= 150 &&
			BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Plague) &&
			defiler->canUseTech(BWAPI::TechTypes::Plague, defiler->getPosition()) &&
			!isReadyToCastOtherThan(defiler, CasterSpell::Plague))
		{
            (void) maybePlague(defiler);
		}
	}
}
