//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef SNIPING_H
#define SNIPING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
FORWARD_DECLARE_MY(Terran_Bunker)

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Sniping
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Sniping : public Behavior<MyUnit>
{
public:
	static const vector<Sniping *> &	Instances()					{ return m_Instances; }

	static My<Terran_Bunker> * FindBunker(MyBWAPIUnit * u, int maxDist);

								Sniping(MyUnit * pAgent, My<Terran_Bunker> * pWhere);
								~Sniping();

	enum state_t {entering, sniping};

	const Sniping *				IsSniping() const override			{ return this; }
	Sniping *					IsSniping() override				{ return this; }

	string						Name() const override				{ return "sniping"; }
	string						StateName() const override;


	BWAPI::Color				GetColor() const override			{CI(this); return Colors::White; }
	Text::Enum					GetTextColor() const override		{CI(this); return Text::White; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override			{ return false; }

	state_t						State() const						{CI(this); return m_state; }
	My<Terran_Bunker> *			Where() const						{CI(this); return m_pWhere; }

private:
	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;
	void						ChangeState(state_t st);
	void						OnFrame_entering();
	void						OnFrame_sniping();

	state_t						m_state = entering;
	frame_t						m_inStateSince;
	My<Terran_Bunker> *			m_pWhere;

	static vector<Sniping *>	m_Instances;
};



} // namespace iron


#endif

