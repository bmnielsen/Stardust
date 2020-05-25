//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef GUARDING_H
#define GUARDING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Guarding
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Guarding : public Behavior<MyUnit>
{
public:
	static const vector<Guarding *> &	Instances()					{ return m_Instances; }

								Guarding(MyUnit * pAgent, const Base * pWhere);
								~Guarding();

	enum state_t {reachingArea, guarding};

	const Guarding *			IsGuarding() const override			{ return this; }
	Guarding *					IsGuarding() override				{ return this; }

	string						Name() const override				{ return "guarding"; }
	string						StateName() const override;


	BWAPI::Color				GetColor() const override			{ return Colors::Yellow; }
	Text::Enum					GetTextColor() const override		{ return Text::Yellow; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override			{ return false; }

	state_t						State() const						{CI(this); return m_state; }
	const Base *				Where() const						{CI(this); return m_pWhere; }
	Position					Target() const						{CI(this); return m_target; }

private:
	void						ChangeState(state_t st);
	void						OnFrame_reachingArea();
	void						OnFrame_guarding();

	state_t						m_state = reachingArea;
	frame_t						m_inStateSince;
	const Base *				m_pWhere;
	Position					m_target = Positions::None;

	static vector<Guarding *>	m_Instances;
};



} // namespace iron


#endif

