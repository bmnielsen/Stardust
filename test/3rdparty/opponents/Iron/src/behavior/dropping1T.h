//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef DROPPING_1T_H
#define DROPPING_1T_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
FORWARD_DECLARE_MY(Terran_SCV)
FORWARD_DECLARE_MY(Terran_Siege_Tank_Tank_Mode)
FORWARD_DECLARE_MY(Terran_Dropship)

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Dropping1T
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Dropping1T : public Behavior<MyUnit>
{
public:
	static const vector<Dropping1T *> &	Instances()					{ return m_Instances; }

	enum state_t {entering, landing};

	static bool					EnterCondition(MyUnit * u);

								Dropping1T(My<Terran_Dropship> * pAgent);
								~Dropping1T();

	const Dropping1T *			IsDropping1T() const override		{ return this; }
	Dropping1T *				IsDropping1T() override				{ return this; }

	string						Name() const override				{ return "dropping1T"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Colors::White; }
	Text::Enum					GetTextColor() const override		{ return Text::White; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int ) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override				{ return false; }

	state_t						State() const						{CI(this); return m_state; }

	My<Terran_Dropship> *		ThisDropship() const;

	My<Terran_Siege_Tank_Tank_Mode> *	GetTank() const;
	vector<My<Terran_SCV> *>	GetSCVs() const;

private:
	void						ChangeState(state_t st);
	void						OnFrame_collecting();
	void						OnFrame_entering();
	void						OnFrame_landing();

	state_t						m_state = landing;
	frame_t						m_inStateSince;
	frame_t						m_lastMove = 0;

	static vector<Dropping1T *>	m_Instances;
};



} // namespace iron


#endif

