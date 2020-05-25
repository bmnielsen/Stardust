//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef DROPPING_1T_1V_H
#define DROPPING_1T_1V_H

#include <BWAPI.h>
#include "behavior.h"
#include "../units/starport.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
FORWARD_DECLARE_MY(Terran_SCV)
FORWARD_DECLARE_MY(Terran_Siege_Tank_Tank_Mode)
FORWARD_DECLARE_MY(Terran_Dropship)

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Dropping1T1V
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Dropping1T1V : public Behavior<MyUnit>
{
public:
	static const vector<Dropping1T1V *> &	Instances()					{ return m_Instances; }

	enum state_t {landing, entering};

	static bool					EnterCondition(MyUnit * u);

								Dropping1T1V(My<Terran_Dropship> * pAgent);
								~Dropping1T1V();

	const Dropping1T1V *		IsDropping1T1V() const override		{ return this; }
	Dropping1T1V *				IsDropping1T1V() override			{ return this; }

	string						Name() const override				{ return "dropping1T1V"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Colors::White; }
	Text::Enum					GetTextColor() const override		{ return Text::White; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int ) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override				{ return false; }

	state_t						State() const						{CI(this); return m_state; }

	bool						SafePlace() const;
	My<Terran_Dropship> *		ThisDropship() const;

	My<Terran_Siege_Tank_Tank_Mode> *	GetTank() const;
	vector<My<Terran_SCV> *>	GetSCVs() const;


private:
	static int					Targets(MyUnit * u);

	bool						DropshipBeingRepaired() const;
	My<Terran_Dropship>::damage_t	GetDamage() const;

	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;
	void						ChangeState(state_t st);
	void						OnFrame_landing();
//	void						OnFrame_entering();

	state_t						m_state = landing;
	frame_t						m_inStateSince;
	frame_t						m_lastMove = 0;
	frame_t						m_lastNonNulCoolDown = 0;
	bool						m_repairOnly;
	vector<MyUnit *>			m_Units;

	static vector<Dropping1T1V *>	m_Instances;
};



} // namespace iron


#endif

