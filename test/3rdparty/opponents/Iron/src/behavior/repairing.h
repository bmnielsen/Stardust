//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef REPAIRING_H
#define REPAIRING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyBWAPIUnit;
FORWARD_DECLARE_MY(Terran_SCV)

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Repairing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Repairing : public Behavior<MyBWAPIUnit>
{
public:
	static const vector<Repairing *> &	Instances()					{ return m_Instances; }

	static Repairing *			GetRepairer(MyBWAPIUnit * pTarget, int radius = 1000000);
	static int					CountRepairers();

	enum state_t {reachingTarget, repairing};

								Repairing(MyBWAPIUnit * pAgent, MyBWAPIUnit * pTarget);
								Repairing(MyBWAPIUnit * pAgent, Repairing * pRepairer);
								~Repairing();

	const Repairing *			IsRepairing() const override		{ return this; }
	Repairing *					IsRepairing() override				{ return this; }

	string						Name() const override				{ return "repairing"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Colors::Yellow; }
	Text::Enum					GetTextColor() const override		{ return Text::Yellow; }

	void						OnFrame_v() override;

	My<Terran_SCV> *			IsRepairer() const;

	bool						CanRepair(const MyBWAPIUnit * , int) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override			{ return false; }

	state_t						State() const						{CI(this); return m_state; }
	frame_t						InStateSince() const				{CI(this); return m_inStateSince; }

	MyBWAPIUnit *				TargetX() const						{CI(this); return m_pTarget; }


private:
	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;
	void						ChangeState(state_t st);
	void						OnFrame_reachingTarget();
	void						OnFrame_repairing();

	static vector<Repairing *>	m_Instances;

	MyBWAPIUnit *				m_pTarget;
	frame_t						m_inStateSince;
	bool						m_beingDestroyed = false;

	state_t						m_state = reachingTarget;
};




} // namespace iron


#endif

