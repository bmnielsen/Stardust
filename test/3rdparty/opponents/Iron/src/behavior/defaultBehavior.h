//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef DEFAULT_BEHAVIOR_H
#define DEFAULT_BEHAVIOR_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class DefaultBehavior
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class DefaultBehavior : public Behavior<MyBWAPIUnit>
{
public:
								DefaultBehavior(MyBWAPIUnit * pAgent);
								~DefaultBehavior();

	const DefaultBehavior *		IsDefaultBehavior() const override	{ return this; }
	DefaultBehavior *			IsDefaultBehavior() override		{ return this; }

	string						Name() const override				{ return "default"; }
	string						StateName() const override			{ return "-"; }

	BWAPI::Color				GetColor() const override			{ return Colors::Grey; }
	Text::Enum					GetTextColor() const override		{ return Text::Grey; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * , int) const override			{ return false; }
	bool						CanChase(const HisUnit * ) const override			{ return false; }

private:
};


} // namespace iron


#endif

