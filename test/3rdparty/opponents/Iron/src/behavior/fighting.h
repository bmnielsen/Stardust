//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef FIGHTING_H
#define FIGHTING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Fighting
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Fighting : public Behavior<MyUnit>
{
public:
	static const vector<Fighting *> &	Instances()					{ return m_Instances; }

	enum state_t {reachingArea, attacking, retreating};

								Fighting(MyUnit * pAgent, Position where, zone_t zone, int fightSimResult, bool protectTank = false);
								~Fighting();

	const Fighting *			IsFighting() const override			{ return this; }
	Fighting *					IsFighting() override				{ return this; }

	string						Name() const override				{ return "fighting"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Zone() == zone_t::ground ? Colors::White : Colors::Cyan; }
	Text::Enum					GetTextColor() const override		{ return Zone() == zone_t::ground ? Text::White : Text::Cyan; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int ) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override				{ return false; }

	state_t						State() const						{CI(this); return m_state; }

	Position					Where() const						{CI(this); return m_where; }
	zone_t						Zone() const						{CI(this); return m_zone; }
	bool						Shooted() const						{CI(this); return m_lastShot > 0; }
	frame_t						LastShot() const					{CI(this); return m_lastShot; }
	
	bool						ProtectTank() const					{CI(this); return m_protectTank; }

private:
	void						OnFrame_reachingArea();
	void						OnFrame_attacking();
	void						OnFrame_retreating();
	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;
	void						ChangeState(state_t st);
	HisUnit *					ChooseTarget() const;
	bool						CheckRetreating();
	bool						EnemyInRange() const;

	state_t						m_state = reachingArea;
	frame_t						m_inStateSince;
	frame_t						m_lastMinePlacement;
	HisUnit *					m_pTarget = nullptr;;
	HisUnit *					m_pLastAttackedTarget = nullptr;
	Position					m_where;
	Position					m_retreatingPos;
	zone_t						m_zone;
	int							m_fightSimResult;
	frame_t						m_lastShot = 0;
	bool						m_protectTank = false;

	static vector<Fighting *>	m_Instances;
};



} // namespace iron


#endif

