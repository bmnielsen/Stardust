//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef KITING_H
#define KITING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class HisBWAPIUnit;
class HisUnit;
class MyUnit;
class FaceOff;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Kiting
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Kiting : public Behavior<MyUnit>
{
public:
	static const vector<Kiting *> &	Instances()					{ return m_Instances; }

	static HisUnit *			AttackCondition(const MyUnit * u, bool evenWeak = false);
	static bool					KiteCondition(const MyUnit * u, vector<const FaceOff *> * pThreats = nullptr, int * pDistanceToHisFire = nullptr);

	enum state_t {kiting, fleeing, patroling, attacking, cornered, mining};

								Kiting(MyUnit * pAgent);
								~Kiting();

	const Kiting *				IsKiting() const override			{ return this; }
	Kiting *					IsKiting() override					{ return this; }

	string						Name() const override				{ return "kiting"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return m_state == fleeing ? Colors::Green : Colors::Teal; }
	Text::Enum					GetTextColor() const override		{ return m_state == fleeing ? Text::Green : Text::Teal; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override	{ return false; }

	state_t						State() const						{CI(this); return m_state; }

	frame_t						LastAttack() const					{CI(this); return max(m_lastAttack, m_lastPatrol); }
	frame_t						FleeUntil() const					{CI(this); return m_fleeUntil; }
	bool						VultureFeast() const;

private:
	void						ChangeState(state_t st);
	void						OnFrame_kiting();

	Vect						CalcultateThreatVector(const vector<const FaceOff *> & Threats) const;
	Vect						CalcultateDivergingVector() const;
	Vect						CalcultateHelpVector() const;
	Vect						CalcultateIncreasingAltitudeVector() const;
	void						AdjustTarget(Position & target, const int walkRadius) const;
	int							SafeDist(const HisBWAPIUnit * pTarget) const;
	frame_t						PatrolTime() const;
	frame_t						AttackTime() const;
	frame_t						MinePlacementTime() const;
	int							ShouldGroundRetrait(const vector<const FaceOff *> & Threats) const;
	void						CheckBlockingMarinesAround(int distanceToHisFire) const;
	void						FleeFor(frame_t time);
	bool						ShouldFlee() const;
	bool						TestVultureFeast(const vector<const FaceOff *> & Threats5) const;

	state_t						m_state = kiting;
	frame_t						m_inStateSince;
	frame_t						m_lastPatrol;
	frame_t						m_lastAttack;
	frame_t						m_lastMinePlacement;
	frame_t						m_lastVultureFeast = 0;
	frame_t						m_lastTimeBlockedByMines;
	frame_t						m_fleeUntil = 0;
	Position					m_lastPos;
	Position					m_lastTarget;

	static vector<Kiting *>	m_Instances;
};



} // namespace iron


#endif

