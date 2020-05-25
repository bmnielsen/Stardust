//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef LAYING_H
#define LAYING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Laying
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Laying : public Behavior<MyUnit>
{
public:
	static const vector<Laying *> &	Instances()					{ return m_Instances; }

	static Position				FindMinePlacement(const MyUnit * pSCV, Position near, int radius, int minSpacing);

	enum state_t {laying};

								Laying(MyUnit * pAgent);
								~Laying();

	const Laying *				IsLaying() const override			{ return this; }
	Laying *					IsLaying() override					{ return this; }

	string						Name() const override				{ return "laying"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Colors::White; }
	Text::Enum					GetTextColor() const override		{ return Text::White; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int ) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override				{ return false; }

	state_t						State() const						{CI(this); return m_state; }

private:
	void						ChangeState(state_t st);
	frame_t						TimeElapsedSinceLastMineCreated() const;

	state_t						m_state = laying;
	frame_t						m_inStateSince;
	int							m_oldRemainingMines;
	int							m_minesLayed = 0;
	frame_t						m_lastPlacementOrder = 0;
	frame_t						m_lastMineCreated = 0;
	Position					m_target = Positions::None;
	int							m_tries;

	static vector<Laying *>	m_Instances;
};



} // namespace iron


#endif

