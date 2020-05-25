//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef RAIDING_H
#define RAIDING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Raiding
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Raiding : public Behavior<MyUnit>
{
public:
	static const vector<Raiding *> &	Instances()					{ return m_Instances; }

	enum state_t {raiding};

								Raiding(MyUnit * pAgent, Position target);
								~Raiding();

	const Raiding *				IsRaiding() const override			{ return this; }
	Raiding *					IsRaiding() override				{ return this; }

	string						Name() const override				{ return "raiding"; }
	string						StateName() const override			{ return "-"; }

	BWAPI::Color				GetColor() const override			{ return Colors::Teal; }
	Text::Enum					GetTextColor() const override		{ return Text::Teal; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * pTarget, int distance) const override;
	bool						CanChase(const HisUnit * pTarget) const override;

	state_t						State() const						{CI(this); return m_state; }
	Position					Target() const						{CI(this); return m_target; }
	frame_t						InStateSince() const				{CI(this); return m_inStateSince;; }
	bool						Waiting() const						{CI(this); return m_waiting;; }

private:
	void						ChangeState(state_t st);
	bool						WaitGroup() const;

	state_t						m_state = raiding;
	Position					m_target;
	frame_t						m_inStateSince;
	bool						m_waiting = false;

	static vector<Raiding *>	m_Instances;
};



} // namespace iron


#endif

