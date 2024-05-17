//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef BEHAVIOR_TYPE_H
#define BEHAVIOR_TYPE_H

#include <BWAPI.h>
#include "../defs.h"
#include "../utils.h"


namespace iron
{

enum class behavior_t
{
	none,
	DefaultBehavior,
	Executing,
	Walking,
	Mining,
	Refining,
	Supplementing,
	Scouting,
	PatrollingBases,
	Exploring,
	Guarding,
	Razing,
	Harassing,
	Raiding,
	Fleeing,
	Laying,
	KillingMine,
	Chasing,
	VChasing,
	Destroying,
	Kiting,
	Fighting,
	Retraiting,
	LayingBack,
	Repairing,
	Healing,
	Constructing,
	Sieging,
	Blocking,
	Sniping,
	AirSniping,
	Checking,
	Collecting,
	Dropping,
	Dropping1T1V,
	Dropping1T
};



} // namespace iron


#endif

