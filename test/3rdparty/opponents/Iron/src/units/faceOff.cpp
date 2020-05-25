//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "faceOff.h"
#include "my.h"
#include "his.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }

namespace iron
{
/*
void MapPrinter::Line(WalkPosition A, WalkPosition B, Color col, dashed_t dashedMode)
{
	int N = max(1, roundedDist(A, B));
	if ((dashedMode == dashed) && (N >= 4)) N /= 2;

	for (int i = 0 ; i <= N ; ++i)
		MapPrinter::Get().Point((A*i + B*(N-i))/N, col);
}
*/

static int groundDist(const Position & a, const Position & b, int airDist)
{
	int length;
	const auto & Path = ai()->GetMap().GetPath(a, b, &length);
	if (length < 0) return 1000000;
	if (Path.size() != 1) return length;

	if (airDist > 32*12) return length;
	if (length > 32*12) return length;

	return airDist;
/*
	bool seaFound = false;
	int N = max(1, length/32);
	for (int i = 0 ; i <= N ; ++i)
	{
		auto t = TilePosition((a*i + b*(N-i))/N);
		CHECK_POS(t);
		if (ai()->GetMap().GetTile(t).MinAltitude() == 0) { seaFound = true; break; }
	}
	if (!seaFound) return airDist;

	const ChokePoint * cp = Path.front();
	for (ChokePoint::node node : {ChokePoint::end1, ChokePoint::end2})
	{
		auto c = Position(cp->Pos(node));
		int d = a.getApproxDistance(c) + b.getApproxDistance(c);
		if (d < length) length = d;
	}

	return length;
*/
}


int groundDist(const BWAPIUnit * pUnit, const BWAPIUnit * pTarget)
{
	return groundDist(pUnit->Pos(), pTarget->Pos(), pUnit->GetDistanceToTarget(pTarget));
}


int groundDist(const Position & a, const Position &  b)
{
	return groundDist(a, b, a.getApproxDistance(b));
}


int groundRange(const BWAPI::UnitType type, const BWAPI::Player player)
{
	return type == Terran_Vulture_Spider_Mine ? player->sightRange(type) :
		   type == Protoss_Reaver ? 256 :
		   type == Protoss_Scarab ? 256 :
		   player->weaponMaxRange(type.groundWeapon());	
}


int airRange(const BWAPI::UnitType type, const BWAPI::Player player)
{
	return player->weaponMaxRange(type.airWeapon());	
}


int groundAttack(const BWAPI::UnitType type, const BWAPI::Player player)
{
	return type == Protoss_Reaver ? 100 : player->damage(type.groundWeapon());	
}


int airAttack(const BWAPI::UnitType type, const BWAPI::Player player)
{
	return player->damage(type.airWeapon());	
}


int armor(const BWAPI::UnitType type, const BWAPI::Player player)
{
	return player->armor(type);	
}


int avgCoolDown(const BWAPI::UnitType type, const BWAPI::Player player, bool useAirWeapon)
{
	if (type == Protoss_Reaver) return 100;
	
	if (useAirWeapon)
		if ((type == Terran_Wraith) || (type == Protoss_Scout))
			return 22;

	return player->weaponDamageCooldown(type);
}


int regenerationTime(const BWAPI::UnitType type)
{
	return type.getRace() == Races::Zerg ? 64 : type.getRace() == Races::Protoss ? 37 : 0;	
}


int pleinAttack(const BWAPI::UnitType attackerType, const BWAPI::Player attackerPlayer, bool defenderIsFlying)
{
	return defenderIsFlying ? airAttack(attackerType, attackerPlayer) : groundAttack(attackerType, attackerPlayer);
}


int sizedAttack_times4(const BWAPI::UnitType attackerType, const BWAPI::Player attackerPlayer,
					  const BWAPI::UnitType defenderType, const BWAPI::Player defenderPlayer,
					  bool defenderIsFlying)
{
	int a = pleinAttack(attackerType, attackerPlayer, defenderIsFlying);

	a -= armor(defenderType, defenderPlayer);
	if (a < 0) a = 0;

	if (a == 0) return 0;

	const auto damageType = defenderIsFlying ? attackerType.airWeapon().damageType() : attackerType.groundWeapon().damageType();
	const auto defenderSize = defenderType.size();

	int sa_times4 = a * 4;
	if (damageType == DamageTypes::Concussive) // 100% to small units, 50% damage to medium units, and 25% damage to large units
	{
		if		(defenderSize == UnitSizeTypes::Medium) sa_times4 /= 2;
		else if (defenderSize == UnitSizeTypes::Large) sa_times4 /= 4;
	}
	else if (damageType == DamageTypes::Explosive) // 100% to large units, 75% to medium units, and 50% to small units
	{
		if		(defenderSize == UnitSizeTypes::Medium) sa_times4 = sa_times4 * 3 / 4;
		else if (defenderSize == UnitSizeTypes::Small) sa_times4 /= 2;
	}

	return sa_times4;
}


int avgAttack_times4(const BWAPI::UnitType attackerType, const BWAPI::Player attackerPlayer,
				  const BWAPI::UnitType defenderType, const BWAPI::Player defenderPlayer,
				  int defenderLife, int defenderShields, bool defenderIsFlying)
{
	int sa4 = sizedAttack_times4(attackerType, attackerPlayer, defenderType, defenderPlayer, defenderIsFlying);
	if (sa4 == 0) return 0;
	if (defenderShields == 0) return sa4;

	int a = pleinAttack(attackerType, attackerPlayer, defenderIsFlying);

	const int k = 4096*defenderShields/(a*4) + 4096*defenderLife/sa4;

	assert_throw_plus(k > 0,
		" shields = " + to_string(defenderShields) +
		" pleinAttack = " + to_string(a) +
		" Life = " + to_string(defenderLife) +
		" sizedAttack*4 = " + to_string(sa4)
		);

	return 4096*(defenderLife + defenderShields) / max(1, k);
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class FaceOff
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


FaceOff::FaceOff(MyUnit * mine, His_t * his)
	:m_mine(CI(mine)), m_his(CI(his)),
	m_airDistanceToHitHim(mine->GetDistanceToTarget(his)),
	m_airDistanceToHitMe(his->GetDistanceToTarget(mine)),
	m_groundDistanceToHitHim((mine->GetArea(check_t::no_check) == his->GetArea(check_t::no_check) || mine->Flying() || his->Flying()) ? m_airDistanceToHitHim : groundDist(mine, his)),
	m_groundDistanceToHitMe((mine->GetArea(check_t::no_check) == his->GetArea(check_t::no_check) || mine->Flying() || his->Flying()) ? m_airDistanceToHitMe : groundDist(his, mine)),
	m_myAttack_times4(GetAttack_times4(mine, his)),
	m_myAttack((m_myAttack_times4 + 2) / 4),
	m_hisAttack_times4(GetAttack_times4(his, mine)),
	m_hisAttack((m_hisAttack_times4 + 2) / 4),
	m_myRange(GetRange(mine, his)),
	m_hisRange(GetHisRange(his, mine)),
	m_distanceToMyRange((m_myRange > 3*32 ? m_airDistanceToHitHim : m_groundDistanceToHitHim) - m_myRange),
	m_distanceToHisRange((m_hisRange > 3*32 ? m_airDistanceToHitMe : m_groundDistanceToHitMe) - m_hisRange),
	m_framesToKillMe(GetKillTime(his, mine, m_hisAttack_times4, m_distanceToHisRange)),
	m_framesToKillHim(GetKillTime(mine, his, m_myAttack_times4, m_distanceToMyRange))
{
	if (his->Is(Terran_Bunker))
	{
		m_hisAttack = 6*4;
		m_hisAttack_times4 = m_hisAttack * 4;
		m_hisRange = 6*32;
		m_distanceToHisRange = m_airDistanceToHitMe - m_hisRange;
		m_framesToKillMe = GetKillTime(his, mine, m_hisAttack_times4, m_distanceToHisRange);
		m_framesToKillHim = GetKillTime(mine, his, m_myAttack_times4, m_distanceToMyRange);
	}

	//pFaceOff->His()->Is(Zerg_Lurker) && !pFaceOff->His()->InFog() && (ai()->Frame() - pFaceOff->His()->LastFrameMoving() > 10))
	if (his->Is(Zerg_Lurker))
		if (!mine->Flying())
		if (!his->InFog())
		if (!his->Burrowed())
			if (ai()->Frame() - his->LastFrameMoving() > 10)
			{
				m_myAttack = m_myAttack_times4 = 0;
				m_hisAttack = 20;
				m_hisRange = 6*32;
				m_hisAttack_times4 = m_hisRange * 4;
				m_distanceToHisRange = m_airDistanceToHitMe - m_hisRange;
				m_framesToKillMe = GetKillTime(his, mine, m_hisAttack_times4, m_distanceToHisRange);
				m_framesToKillHim = GetKillTime(mine, his, m_myAttack_times4, m_distanceToMyRange);
			//	ai()->SetDelay(500);
#if DEV
				bw->drawTriangleMap(his->Pos() + Position(0, -15), his->Pos() + Position(-15, 0), his->Pos() + Position(+15, 0), Colors::White);
#endif
			}
}


int FaceOff::GetAttack_times4(const BWAPIUnit * attacker, const BWAPIUnit * defender) const
{CTHIS;
	if ((defender == m_his) && !m_his->InFog() && !m_his->Unit()->isDetected()) return 0;
//	if ((defender == m_mine) && m_mine->Unit()->isCloaked()) return 0;
	if (defender->Is(Terran_Vulture_Spider_Mine) && (defender == m_his) && m_his->InFog()) return 0;
	if (attacker->Is(Terran_Vulture_Spider_Mine) && (defender->Hovering() || defender->Is(Terran_Vulture_Spider_Mine))) return 0;
//	if (attacker->Is(Zerg_Lurker) && !attacker->Burrowed()) return 0;

	//if (!defender->Unit()->isVisible()) return 0;
	if (attacker->RemainingBuildTime() > 0) return 0;
	if (attacker->Unit()->isLockedDown()) return 0;
	if (attacker->Unit()->isMaelstrommed()) return 0;
	if (attacker->Unit()->isStasised()) return 0;
	if (attacker->Unit()->isUnderDisruptionWeb()) return 0;
	if (defender->Unit()->isStasised()) return 0;
	if (defender->Unit()->isUnderDarkSwarm() && GetRange(attacker, defender) > 3*32) return 0;

	if ((defender->Unit()->isCloaked()) && (defender == m_mine) && !attacker->DetectorBuilding())
		if (defender->Life() >= defender->PrevLife(40))
			if (defender->Energy() >= 5)
				return 0;


	return avgAttack_times4(attacker->Type(), attacker->Unit()->getPlayer(),
							defender->Type(), defender->Unit()->getPlayer(),
							defender->Life(), defender->Shields(), defender->Flying());
}


int FaceOff::GetHisRange(const BWAPIUnit * attacker, const BWAPIUnit * defender) const
{CTHIS;
	int range = GetRange(attacker, defender);

	if ((range > 3*32) && !attacker->Is(Terran_Siege_Tank_Siege_Mode))
		range += 6;

	return range;
}


int FaceOff::GetRange(const BWAPIUnit * attacker, const BWAPIUnit * defender) const
{CTHIS;
	return defender->Flying() ? attacker->AirRange() : attacker->GroundRange();
}


frame_t FaceOff::GetKillTime(const BWAPIUnit * attacker, const BWAPIUnit * defender, int damagePerAttack_times4, int distanceToAttackerRange) const
{CTHIS;
	if (damagePerAttack_times4 == 0) return 1000000;

	const int regenTime = defender->RegenerationTime();
	
	frame_t frames = 0;	// avoid warning
	int maxLoop = 2;
	for (int life = defender->LifeWithShields() ; maxLoop ; life += frames / regenTime, --maxLoop)
	{
		int attackCount_minus1 = (life-1) * 4 / damagePerAttack_times4;
		frames = attackCount_minus1 * attacker->AvgCoolDown();
		
		if (!frames || !regenTime) break;
	}

	if (attacker->CoolDown() || attacker->PrevCoolDown())
	{
		return attacker->CoolDown() + frames;
	}
	else if (attacker->RemainingBuildTime() > 0)
	{
		return attacker->RemainingBuildTime() + frames;
	}
	else
	{
		if (distanceToAttackerRange < 0) distanceToAttackerRange = 0;

		return distanceToAttackerRange*13/64				// pixels to frames
				- 4											// compensates for distanceToAttackerRange slightly overestimation
				+ (attacker->Is(Terran_SCV) ? 5 : 0)			// average frames spent in deceleration
				+ frames;
	}
}

	
} // namespace iron



