//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef FACE_OFF_H
#define FACE_OFF_H

#include "../debug.h"
#include "../defs.h"

namespace iron
{

class BWAPIUnit;
class MyUnit;
class HisBWAPIUnit;
class MyBWAPIUnit;

int groundDist(const Position & a, const Position &  b);
int groundDist(const BWAPIUnit * pUnit, const BWAPIUnit * pTarget);

inline int zoneDist(const Position & a, const Position & b, zone_t zone)
{
	return (zone == zone_t::air) ? roundedDist(a, b) : groundDist(a, b);
}


int groundRange(const BWAPI::UnitType type, const BWAPI::Player player);
int airRange(const BWAPI::UnitType type, const BWAPI::Player player);
int groundAttack(const BWAPI::UnitType type, const BWAPI::Player player);
int airAttack(const BWAPI::UnitType type, const BWAPI::Player player);
int armor(const BWAPI::UnitType type, const BWAPI::Player player);
int avgCoolDown(const BWAPI::UnitType type, const BWAPI::Player player, bool useAirWeapon = false);

int regenerationTime(const BWAPI::UnitType type);


int pleinAttack(const BWAPI::UnitType attackerType, const BWAPI::Player attackerPlayer, bool defenderIsFlying);

int sizedAttack_times4(const BWAPI::UnitType attackerType, const BWAPI::Player attackerPlayer,
					   const BWAPI::UnitType defenderType, const BWAPI::Player defenderPlayer,
					   bool defenderIsFlying);

int avgAttack_times4(const BWAPI::UnitType attackerType, const BWAPI::Player attackerPlayer,
					const BWAPI::UnitType defenderType, const BWAPI::Player defenderPlayer,
					int defenderLife, int defenderShields, bool defenderIsFlying);


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class FaceOff
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class FaceOff
{
public:
//	typedef MyBWAPIUnit		His_t;
	typedef HisBWAPIUnit	His_t;

						FaceOff(MyUnit * mine, His_t * his);

	MyUnit *			Mine() const				{CTHIS; return m_mine; }
	His_t *				His() const					{CTHIS; return m_his; }

	// Euclidean distance
	int					AirDistanceToHitHim() const		{CTHIS; return m_airDistanceToHitHim; }
	int					AirDistanceToHitMe() const		{CTHIS; return m_airDistanceToHitMe; }
	int					GroundDistanceToHitHim() const	{CTHIS; return m_groundDistanceToHitHim; }
	int					GroundDistanceToHitMe() const	{CTHIS; return m_groundDistanceToHitMe; }

	// Damage inflicted per attack
	int					MyAttack() const			{CTHIS; return m_myAttack; }
	int					HisAttack() const			{CTHIS; return m_hisAttack; }

	int					MyRange() const				{CTHIS; return m_myRange; }
	int					HisRange() const			{CTHIS; return m_hisRange; }

	// A positive value indicates an out-of-range distance in pixels
	// A negative value indicates an in-range distance in pixels
	int					DistanceToMyRange() const	{CTHIS; return m_distanceToMyRange; }
	int					DistanceToHisRange() const	{CTHIS; return m_distanceToHisRange; }

	frame_t				FramesToKillMe() const		{CTHIS; return m_framesToKillMe; }
	frame_t				FramesToKillHim() const		{CTHIS; return m_framesToKillHim; }

private:
	int					GetAttack_times4(const BWAPIUnit * attacker, const BWAPIUnit * defender) const;
	int					GetHisRange(const BWAPIUnit * attacker, const BWAPIUnit * defender) const;
	int					GetRange(const BWAPIUnit * attacker, const BWAPIUnit * defender) const;
	frame_t				GetKillTime(const BWAPIUnit * attacker, const BWAPIUnit * defender, int damagePerAttack, int distanceToAttackerRange) const;

	MyUnit *			m_mine;
	His_t *				m_his;
	int					m_airDistanceToHitHim;
	int					m_airDistanceToHitMe;
	int					m_groundDistanceToHitHim;
	int					m_groundDistanceToHitMe;
	int					m_myAttack_times4;
	int					m_myAttack;
	int					m_hisAttack_times4;
	int					m_hisAttack;
	int					m_myRange;
	int					m_hisRange;
	int					m_distanceToMyRange;
	int					m_distanceToHisRange;
	frame_t				m_framesToKillMe;
	frame_t				m_framesToKillHim;
};




} // namespace iron


#endif

