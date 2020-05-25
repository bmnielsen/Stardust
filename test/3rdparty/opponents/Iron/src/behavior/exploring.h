//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef EXPLORING_H
#define EXPLORING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Exploring
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Exploring : public Behavior<MyUnit>
{
public:
	static const vector<Exploring *> &	Instances()					{ return m_Instances; }

								Exploring(MyUnit * pAgent, const Area * pWhere);
								~Exploring();

	enum state_t {reachingArea, lastVisited, random};

	const Exploring *			IsExploring() const override		{ return this; }
	Exploring *					IsExploring() override				{ return this; }

	string						Name() const override				{ return "exploring"; }
	string						StateName() const override;


	BWAPI::Color				GetColor() const override			{ return Colors::Blue; }
	Text::Enum					GetTextColor() const override		{ return Text::Blue; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int) const override	{ return true; }
	bool						CanChase(const HisUnit * ) const override			{ return true; }

	state_t						State() const						{CI(this); return m_state; }
	const Area *				Where() const						{CI(this); return m_pWhere; }
	Position					Target() const						{CI(this); return m_target; }

private:
	void						ChangeState(state_t st);
	void						OnFrame_reachingArea();
	void						OnFrame_lastVisited();
	void						OnFrame_random();

	state_t						m_state = reachingArea;
	frame_t						m_inStateSince;
	const Area *				m_pWhere;
	Position					m_target = Positions::None;

	static vector<Exploring *>	m_Instances;
};



} // namespace iron


#endif

