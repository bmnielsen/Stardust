//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef AIR_SNIPING_H
#define AIR_SNIPING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
FORWARD_DECLARE_MY(Terran_Dropship)

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class AirSniping
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class AirSniping : public Behavior<MyUnit>
{
public:
	static const vector<AirSniping *> &	Instances()					{ return m_Instances; }

	static My<Terran_Dropship> * FindDropship(MyBWAPIUnit * u, int maxDist);

								AirSniping(MyUnit * pAgent, My<Terran_Dropship> * pWhere);
								~AirSniping();

	enum state_t {entering, landing};

	const AirSniping *			IsAirSniping() const override		{ return this; }
	AirSniping *				IsAirSniping() override				{ return this; }

	string						Name() const override				{ return "airSniping"; }
	string						StateName() const override;


	BWAPI::Color				GetColor() const override			{CI(this); return Colors::White; }
	Text::Enum					GetTextColor() const override		{CI(this); return Text::White; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int) const override	{ return false; }
	bool						CanChase(const HisUnit * ) const override			{ return false; }

	state_t						State() const						{CI(this); return m_state; }
	My<Terran_Dropship> *		Dropship() const					{CI(this); return m_pDropship; }

private:
	void						OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other) override;
	void						ChangeState(state_t st);
	void						OnFrame_entering();
	void						OnFrame_landing();

	state_t						m_state = entering;
	frame_t						m_inStateSince;
	My<Terran_Dropship> *		m_pDropship;

	static vector<AirSniping *>	m_Instances;
};



} // namespace iron


#endif

