//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef INTERACTIVE_H
#define INTERACTIVE_H

#include <BWAPI.h>
#include "defs.h"
#include "utils.h"



namespace iron
{



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Interactive
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


class Interactive
{
public:

	static bool ProcessCommand(const std::string & command);


	static bool		moreSCVs;
	static bool		moreMarines;
	static bool		moreVultures;
	static bool		moreTanks;
	static bool		moreGoliaths;
	static bool		moreWraiths;
	static bool		expand;
	static bool		berserker;
	static Position	raidingDefaultTarget;

private:
	static bool ProcessCommandVariants(const std::string & command, const std::string & attributName, bool & attribut);
};


void drawInteractive();

} // namespace iron


#endif

