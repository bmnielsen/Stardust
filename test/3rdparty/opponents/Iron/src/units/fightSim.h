//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef FIGHT_SIM_H
#define FIGHT_SIM_H

#include "../debug.h"
#include "../defs.h"

namespace iron
{

class MyUnit;
class HisUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class FightSim
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class FightSim
{
public:

						FightSim(zone_t zone);

	zone_t				Zone() const			{ return m_zone; }

	void				AddMyUnit(BWAPI::UnitType type, int life, int shields, Position pos, frame_t timeToReachFight, bool stimmed = false);
	void				AddHisUnit(BWAPI::UnitType type, int life, int shields, Position pos);

	void				Eval();

	int					Efficiency() const		{ return m_efficiency; }

// +1 : all his ground units die
// -1 : all my ground units die
//  0 : draw
	int					Result() const			{ return m_result; }

	const string &		FinalState() const		{ return m_finalState; }

private:
	struct UnitInfo
	{
							UnitInfo(BWAPI::UnitType type, int life, int shields, frame_t avgCool, int groundRange, Position pos, frame_t timeToReachFight);
		BWAPI::UnitType		type;
		int					life_times4;
		int					shields;
		frame_t				avgCool;
		int					groundRange;
		Position			pos;
		int					groundHeight;
		frame_t				timeToReachFight;
		frame_t				death = 1000000;

		void				UpdateShields(int v, frame_t time);
		void				UpdateLife4(int v4, frame_t time);
	};

	struct FaceOffInfo
	{
		int					pleinAttack;
		int					sizedAttack_times4;
	};

	int					EvalLoop();
	void				Compute_FaceOffInfoMatrix();
	void				SortTargets(vector<unique_ptr<UnitInfo>> & Targets);
	bool				EvalAttack(bool mine, const UnitInfo * pAttacker, vector<unique_ptr<UnitInfo>> & Targets, frame_t time);
	void				PrintState(ostream & out, char eventType, frame_t time) const;
	void				PrintUnitState(ostream & out, const UnitInfo * u) const;
	void				CalculateEfficiency();

	vector<unique_ptr<UnitInfo>>	m_MyUnits;
	vector<unique_ptr<UnitInfo>>	m_HisUnits;

	map<pair<BWAPI::UnitType, BWAPI::UnitType>, FaceOffInfo>	m_FaceOffInfoMatrix;		// (attacker, defender)

	zone_t				m_zone;
	string				m_finalState;
	int					m_efficiency;
	int					m_result;

	class Event;
};




} // namespace iron


#endif

