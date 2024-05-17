//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef LAYING_BACK_H
#define LAYING_BACK_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

FORWARD_DECLARE_MY(Terran_Vulture)
class FaceOff;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class LayingBack
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class LayingBack : public Behavior<My<Terran_Vulture>>
{
public:
	static const vector<LayingBack *> &	Instances()					{ return m_Instances; }

	static bool					Allowed(My<Terran_Vulture> * pVulture);
//	static int					Condition(const MyUnit * u, const vector<const FaceOff *> & Threats, int & coverMines);

	enum state_t {layingBack};

								LayingBack(My<Terran_Vulture> * pAgent, Position target);
								~LayingBack();

	const LayingBack *			IsLayingBack() const override		{ return this; }
	LayingBack *				IsLayingBack() override				{ return this; }

	string						Name() const override				{ return "LayingBack"; }
	string						StateName() const override;

	BWAPI::Color				GetColor() const override			{ return Colors::Purple; }
	Text::Enum					GetTextColor() const override		{ return Text::Purple; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int ) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override	{ return false; }

	state_t						State() const						{CI(this); return m_state; }
	Position					Target() const						{CI(this); return m_target; }
	const Area *				TargetArea() const					{CI(this); return m_pTargetArea; }
	frame_t						InStateSince() const				{CI(this); return m_inStateSince;; }

private:
	void						ChangeState(state_t st);
//	bool						TryCounterNow();

	state_t						m_state = layingBack;
	Position					m_target;
	const Area *				m_pTargetArea;
	frame_t						m_inStateSince;
	frame_t						m_layingCoverMinesTime = 0;

	static vector<LayingBack *>	m_Instances;
};



} // namespace iron


#endif

