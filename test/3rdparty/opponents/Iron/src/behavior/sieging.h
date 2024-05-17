//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef SIEGING_H
#define SIEGING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

FORWARD_DECLARE_MY(Terran_Siege_Tank_Tank_Mode)
class MyUnit;
class HisBWAPIUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Sieging
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Sieging : public Behavior<My<Terran_Siege_Tank_Tank_Mode>>
{
public:
	static const vector<Sieging *> &	Instances()					{ return m_Instances; }

	static bool					EnterCondition(const MyUnit * u);

								Sieging(My<Terran_Siege_Tank_Tank_Mode> * pAgent);
								~Sieging();

	enum state_t {sieging, unsieging};

	const Sieging *				IsSieging() const override			{ return this; }
	Sieging *					IsSieging() override				{ return this; }

	string						Name() const override				{ return "sieging"; }
	string						StateName() const override;


	BWAPI::Color				GetColor() const override			{ return Colors::Orange; }
	Text::Enum					GetTextColor() const override		{ return Text::Orange; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override			{ return false; }

	state_t						State() const						{CI(this); return m_state; }
	HisBWAPIUnit *				Target() const						{CI(this); return m_pTarget; }


private:
	bool						LeaveCondition(const vector<MyBWAPIUnit *> & MyUnitsAround) const;

	pair<HisBWAPIUnit *, int>	ChooseTarget(const vector<MyBWAPIUnit *> & MyUnitsAround, const vector<HisBWAPIUnit *> & HisUnitsAround) const;
	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;
	void						ChangeState(state_t st);
	void						OnFrame_sieging();
	void						OnFrame_unsieging();

	state_t						m_state = sieging;
	frame_t						m_inStateSince;
	frame_t						m_staySiegedUntil = 0;
	frame_t						m_delayUntilUnsieging;
	HisBWAPIUnit *				m_pTarget = nullptr;
	int							m_nextTargetScore;
	int							m_killsSinceSieging;

	static vector<Sieging *>	m_Instances;
};



} // namespace iron


#endif

