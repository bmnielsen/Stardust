//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "fightSim.h"
#include "my.h"
#include "his.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }

namespace iron
{

const int death_delay = 3;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class FightSim
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


FightSim::FightSim(zone_t zone)
: m_zone(zone)
{

}


void FightSim::AddMyUnit(BWAPI::UnitType type, int life, int shields, Position pos, frame_t timeToReachFight, bool stimmed)
{
	int cool = avgCoolDown(type, me().Player(), Zone() == zone_t::air);
	if (stimmed) cool /= 2;
	if (type == Terran_Siege_Tank_Siege_Mode) life += 50;

	int range = groundRange(type, me().Player());

	m_MyUnits.push_back(make_unique<UnitInfo>(type, life, shields, cool, range, pos, timeToReachFight));
}


void FightSim::AddHisUnit(BWAPI::UnitType type, int life, int shields, Position pos)
{
	int cool = avgCoolDown(type, him().Player(), Zone() == zone_t::air);
	if (type == Terran_Siege_Tank_Siege_Mode) life += 50;

	int range = groundRange(type, him().Player());

	m_HisUnits.push_back(make_unique<UnitInfo>(type, life, shields, cool, range, pos, 0));
}


void FightSim::Compute_FaceOffInfoMatrix()
{
	for (const auto & myUnit : m_MyUnits)
	for (const auto & hisUnit : m_HisUnits)
	{
		// myUnit attacks hisUnit:
		auto inserted = m_FaceOffInfoMatrix.insert(make_pair(make_pair(myUnit->type, hisUnit->type), FaceOffInfo()));
		if (inserted.second)
		{
			inserted.first->second.pleinAttack = pleinAttack(myUnit->type, me().Player(), hisUnit->type.isFlyer());
			inserted.first->second.sizedAttack_times4 = sizedAttack_times4(myUnit->type, me().Player(), hisUnit->type, him().Player(), hisUnit->type.isFlyer());
		}

		// hisUnit attacks myUnit:
		inserted = m_FaceOffInfoMatrix.insert(make_pair(make_pair(hisUnit->type, myUnit->type), FaceOffInfo()));
		if (inserted.second)
		{
			inserted.first->second.pleinAttack = pleinAttack(hisUnit->type, him().Player(), myUnit->type.isFlyer());
			inserted.first->second.sizedAttack_times4 = sizedAttack_times4(hisUnit->type, him().Player(), myUnit->type, me().Player(), myUnit->type.isFlyer());
		}
	}
}


FightSim::UnitInfo::UnitInfo(BWAPI::UnitType type, int life, int shields, frame_t avgCool, int groundRange, Position pos, frame_t timeToReachFight)
: type(type), life_times4(life*4), shields(shields), avgCool(avgCool), groundRange(groundRange), pos(pos), timeToReachFight(timeToReachFight),
  groundHeight(ai()->GetMap().GetTile(TilePosition(pos)).GroundHeight())
{
}


void FightSim::UnitInfo::UpdateShields(int v, frame_t time)
{
	assert_throw(death >= time);

	shields += v;
	shields = max(0, min(type.maxShields(), shields));
}


void FightSim::UnitInfo::UpdateLife4(int v4, frame_t time)
{
	assert_throw(death >= time);

	life_times4 += v4;
	life_times4 = max(0, min(type.maxHitPoints()*4, life_times4));

	if (life_times4 == 0) death = time + death_delay;
}


class FightSim::Event
{
public:
	enum type {myUnitEnterFight, myAttack, hisAttack, protossRegen, zergRegen};

					Event(type t) : m_type(t), m_pUnitInfo(nullptr)
					{
						assert_throw(t == protossRegen || t == zergRegen);

						m_freq = (t == protossRegen) ? 37 : 64;
						m_nextFrame = m_freq;
					}
					
					Event(type t, UnitInfo * pUnitInfo) : m_type(t), m_pUnitInfo(pUnitInfo)
					{
						assert_throw(t == myUnitEnterFight || t == myAttack || t == hisAttack);
						assert_throw(pUnitInfo);

						if (t == myUnitEnterFight)
						{
							assert_throw(pUnitInfo->timeToReachFight > 0);
							m_freq = 0;
							m_nextFrame = pUnitInfo->timeToReachFight - 1;
						}
						else
						{// bool useAirWeapon
							m_freq = pUnitInfo->avgCool;
							assert_throw_plus(m_freq > 0, pUnitInfo->type.getName());
							m_nextFrame = 0;
							if (pUnitInfo->type.isWorker()) m_nextFrame = 40;

							if (pUnitInfo->timeToReachFight > 0) m_nextFrame = pUnitInfo->timeToReachFight;
						}
					}
					

	type			Type() const			{ return m_type; }
	UnitInfo *		GetUnitInfo() const		{ return m_pUnitInfo; }
	frame_t			NextFrame() const		{ return m_nextFrame; }
	void			UpdateNextFrame()		{ m_nextFrame += m_freq; }


	bool			operator<(const Event & Other) const { return m_nextFrame > Other.m_nextFrame; }

private:
	type			m_type;
	frame_t			m_freq;
	frame_t			m_nextFrame = 0;
	UnitInfo *		m_pUnitInfo;
};


void FightSim::SortTargets(vector<unique_ptr<UnitInfo>> & Targets)
{
	sort(Targets.begin(), Targets.end(), [](const unique_ptr<UnitInfo> & a, const unique_ptr<UnitInfo> & b)
	{
		if (a->type == Terran_Medic && b->type != Terran_Medic) return true;
		if (a->type != Terran_Medic && b->type == Terran_Medic) return false;
		return a->shields + a->life_times4/4 < b->shields + b->life_times4/4;
	});
}


// Returns true iif there remains some <Zone()> target alive.
bool FightSim::EvalAttack(bool mine, const UnitInfo * pAttacker, vector<unique_ptr<UnitInfo>> & Targets, frame_t time)
{
	bool someTargetNotYetHere = false;
	for (const auto & target : Targets)
		if (target->death > time + death_delay)
		{
			if ((Zone() == zone_t::air) != target->type.isFlyer()) continue;

			if (target->timeToReachFight > time)
			{
				someTargetNotYetHere = true;
				continue;
			}

			auto iFound = m_FaceOffInfoMatrix.find(make_pair(pAttacker->type, target->type));
			assert_throw(iFound != m_FaceOffInfoMatrix.end());
			
			int pleinAttack = iFound->second.pleinAttack;					// damage to target's shields
			int sizedAttack_times4 = iFound->second.sizedAttack_times4;		// damage to target's life*4
			if (sizedAttack_times4 == 0) continue;

			bool missedShot = false;
			if (!pAttacker->type.isFlyer() && !target->type.isFlyer())
				if (pAttacker->groundRange >= 2*32)
					if (pAttacker->groundHeight < target->groundHeight)
						if (dist(pAttacker->pos, target->pos) <= pAttacker->groundRange + (mine ? 3*32 : 5))
						{
						///	for (int i = -3 ; i <= +3 ; ++i) drawLineMap(pAttacker->pos+i, target->pos, Colors::Orange);
							if (rand() & 1) missedShot = true;
						}

			if (!missedShot)
			{
				if (target->shields > 0)
				{
					if (pleinAttack <= target->shields)
						sizedAttack_times4 = 0;
					else
						sizedAttack_times4 = sizedAttack_times4 * (pleinAttack - target->shields) / pleinAttack;

					target->UpdateShields(-pleinAttack, time);
				}

				target->UpdateLife4(-sizedAttack_times4, time);
			}

			return true;
		}

	return someTargetNotYetHere;
}


void FightSim::PrintUnitState(ostream & out, const UnitInfo * u) const
{
	if (u->type.isFlyer()) out << "^";

	if (u->type.getRace() == Races::Protoss) out << u->shields << "_";

	int n = u->life_times4 / 4;

//	out << n;
//	if (u->life_times4 % 4) out << "." << 25 * (u->life_times4 % 4);

	if ((n == 0) && (u->life_times4 % 4)) n = 1;
	out << n;

	out << "  ";
}


void FightSim::PrintState(ostream & out, char eventType, frame_t time) const
{
	out << eventType << " ";
	out << time << ") \t";
	for (const auto & u : m_MyUnits)
		PrintUnitState(out, u.get());

	out << "  vs   ";

	for (const auto & u : m_HisUnits)
		PrintUnitState(out, u.get());
}


void FightSim::CalculateEfficiency()
{
	int myCost = 0;
	for (const auto & u : m_MyUnits)
		if (u->life_times4 == 0)
			myCost += Cost(u->type).Minerals() + Cost(u->type).Gas();

	int hisCost = 0;
	for (const auto & u : m_HisUnits)
		if (u->life_times4 == 0)
			hisCost += Cost(u->type).Minerals() + Cost(u->type).Gas();

	m_efficiency = hisCost - myCost;
}


int FightSim::EvalLoop()
{
	priority_queue<Event, vector<Event>> EventQueue;

	for (const auto & u : m_MyUnits)
	{
		if (u->avgCool) EventQueue.emplace(Event::myAttack, u.get());
		if (u->timeToReachFight > 0) EventQueue.emplace(Event::myUnitEnterFight, u.get());
	}

	for (const auto & u : m_HisUnits)
	{
		if (u->avgCool) EventQueue.emplace(Event::hisAttack, u.get());
	}

	if (him().IsProtoss()) EventQueue.emplace(Event::protossRegen);
	if (him().IsZerg())    EventQueue.emplace(Event::zergRegen);

	while (!EventQueue.empty())
	{
		Event event = EventQueue.top();
		EventQueue.pop();

		frame_t time = event.NextFrame();

		if (event.Type() == Event::myUnitEnterFight)
		{
			SortTargets(m_MyUnits);
		}
		else if (event.Type() == Event::myAttack)
		{
			if (event.GetUnitInfo()->death < time) continue;
			if (!EvalAttack(bool("mine"), event.GetUnitInfo(), m_HisUnits, time)) return +1;
		}
		else if (event.Type() == Event::hisAttack)
		{
			if (event.GetUnitInfo()->death < time) continue;
			if (!EvalAttack(!bool("mine"), event.GetUnitInfo(), m_MyUnits, time)) return -1;
		}
		else if (event.Type() == Event::protossRegen)
		{
			for (const auto & u : m_HisUnits)
				if (u->death >= time)
					u->UpdateShields(+1, time);
		}
		else if (event.Type() == Event::zergRegen)
		{
			for (const auto & u : m_HisUnits)
				if (u->death >= time)
					u->UpdateLife4(+4, time);
		}

		if (EventQueue.empty() && ((event.Type() == Event::protossRegen) || (event.Type() == Event::zergRegen)))
			break;

		if (event.Type() != Event::myUnitEnterFight)
		{
			event.UpdateNextFrame();
			EventQueue.push(event);
		}
/*
		PrintState(	Log, 
					event.Type() == Event::myUnitEnterFight ? '>' : 
					event.Type() == Event::myAttack ? '+' : 
					event.Type() == Event::hisAttack ? '-' : 
					event.Type() == Event::protossRegen ? 'p' : 
					event.Type() == Event::zergRegen ? 'z' : '?',
					time);
		Log << endl;
*/
	}

	return 0;
}


void FightSim::Eval()
{
	assert_throw(!m_MyUnits.empty());
	assert_throw(!m_HisUnits.empty());

	Compute_FaceOffInfoMatrix();

	m_result = EvalLoop();
//	Log << m_result << endl;

	CalculateEfficiency();

	ostringstream oss;
	PrintState(oss, ' ', 0);
	m_finalState = oss.str();



//	int myRemainingUnits = count_if(m_MyUnits.begin(), m_MyUnits.end(), [](const UnitInfo & u){ return u.life_times4 > 0; });
//	int hisRemainingUnits = count_if(m_HisUnits.begin(), m_HisUnits.end(), [](const UnitInfo & u){ return u.life_times4 > 0; });
//	bw << "--> " << myRemainingUnits << " vs " << hisRemainingUnits << endl;
}

	
} // namespace iron



