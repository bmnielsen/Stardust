//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef HEALING_H
#define HEALING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

FORWARD_DECLARE_MY(Terran_Medic)
class MyUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Healing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Healing : public Behavior<My<Terran_Medic>>
{
public:
	static const vector<Healing *> &	Instances()					{ return m_Instances; }

	static MyUnit *				FindTarget(const My<Terran_Medic> * pMedic);

								Healing(My<Terran_Medic> * pAgent);
								~Healing();

	enum state_t {healing};

	const Healing *				IsHealing() const override			{ return this; }
	Healing *					IsHealing() override				{ return this; }

	string						Name() const override				{ return "healing"; }
	string						StateName() const override;


	BWAPI::Color				GetColor() const override			{ return Colors::Yellow; }
	Text::Enum					GetTextColor() const override		{ return Text::Yellow; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override			{ return false; }

	state_t						State() const						{CI(this); return m_state; }
	MyUnit *					Target() const						{CI(this); return m_pTarget; }


private:
	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;
	void						ChangeState(state_t st);
	void						OnFrame_healing();

	state_t						m_state = healing;
	frame_t						m_inStateSince;
	MyUnit *					m_pTarget = nullptr;

	static vector<Healing *>	m_Instances;
};



} // namespace iron


#endif

