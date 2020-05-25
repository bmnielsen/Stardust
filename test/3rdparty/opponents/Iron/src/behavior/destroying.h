//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef DESTROYING_H
#define DESTROYING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class HisUnit;
FORWARD_DECLARE_MY(Terran_Vulture)


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Destroying
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Destroying : public Behavior<My<Terran_Vulture>>
{
public:
	static const vector<Destroying *> &	Instances()					{ return m_Instances; }


	enum state_t {destroying};

								Destroying(My<Terran_Vulture> * pAgent, const vector<HisUnit *> & Targets);
								~Destroying();

	const Destroying *			IsDestroying() const override		{ return this; }
	Destroying *				IsDestroying() override				{ return this; }

	string						Name() const override				{ return "destroying"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Colors::Yellow; }
	Text::Enum					GetTextColor() const override		{ return Text::Yellow; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override	{ return false; }

	state_t						State() const						{CI(this); return m_state; }

	const vector<HisUnit *> &	Targets() const						{CI(this); return m_Targets; }


private:
	Vect						CalcultateDivergingVector() const;
	frame_t						MinePlacementTime() const;
	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;
	void						ChangeState(state_t st);
	void						OnFrame_destroying();

	state_t						m_state = destroying;
	vector<HisUnit *>			m_Targets;

	frame_t						m_DestroyingSince;
	frame_t						m_lastMinePlacement = 0;

	static vector<Destroying *>	m_Instances;
};



} // namespace iron


#endif

