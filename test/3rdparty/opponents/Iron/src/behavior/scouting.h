//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef SCOUTING_H
#define SCOUTING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class MyUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Scouting
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Scouting : public Behavior<MyUnit>
{
public:
	static const vector<Scouting *> &	Instances()					{ return m_Instances; }
	static int							InstancesCreated()			{ return m_instancesCreated; }

								Scouting(MyUnit * pAgent);
								~Scouting();

	const Scouting *			IsScouting() const override			{ return this; }
	Scouting *					IsScouting() override				{ return this; }

	string						Name() const override				{ return "scouting"; }
	string						StateName() const override			{ return "-"; }


	BWAPI::Color				GetColor() const override			{ return Colors::Green; }
	Text::Enum					GetTextColor() const override		{ return Text::Green; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int) const override;
	bool						CanChase(const HisUnit * ) const override			{ return true; }

	TilePosition				Target() const						{CI(this); return m_Target; }

private:
	TilePosition				m_Target;

	static vector<Scouting *>	m_Instances;
	static int					m_instancesCreated;
};



} // namespace iron


#endif

