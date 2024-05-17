//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef REFINING_H
#define REFINING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class VBase;
FORWARD_DECLARE_MY(Terran_SCV)

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Refining
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//
//	Mineral::Data() is used to store the number of miners assigned to this Mineral.
//

class Refining : public Behavior<My<Terran_SCV>>
{
public:
	static const vector<Refining *> &	Instances()					{ return m_Instances; }

	enum state_t {gathering, returning};

								Refining(My<Terran_SCV> * pSCV);
								~Refining();

	const Refining *			IsRefining() const override			{ return this; }
	Refining *					IsRefining() override				{ return this; }

	string						Name() const override				{ return "refining"; }
	string						StateName() const override			{ return "-"; }

	BWAPI::Color				GetColor() const override			{ return Colors::Blue; }
	Text::Enum					GetTextColor() const override		{ return Text::Blue; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * pTarget, int distance) const override;
	bool						CanChase(const HisUnit * pTarget) const override;

	state_t						State() const						{CI(this); return m_state; }

	VBase *						GetBase() const						{CI(this); return m_pBase; }
	Geyser *					GetGeyser() const					{CI(this); return m_pGeyser; }

	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;

	bool						MovingTowardsRefinery() const;

private:

	void						Assign(Geyser * g);
	void						Release();

	Geyser *					m_pGeyser = nullptr;
	VBase *						m_pBase;
	state_t						m_state;

	static vector<Refining *>	m_Instances;
};



} // namespace iron


#endif

