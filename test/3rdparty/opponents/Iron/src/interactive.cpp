//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "interactive.h"
#include "Iron.h"
#include <sstream>

namespace { auto & bw = Broodwar; }




namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Interactive
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

bool Interactive::moreSCVs						= INTERACTIVE && false;
bool Interactive::moreMarines					= INTERACTIVE && false;
bool Interactive::moreVultures					= INTERACTIVE && false;
bool Interactive::moreTanks						= INTERACTIVE && false;
bool Interactive::moreGoliaths					= INTERACTIVE && false;
bool Interactive::moreWraiths					= INTERACTIVE && false;
bool Interactive::expand						= INTERACTIVE && false;
bool Interactive::berserker						= INTERACTIVE && false;
Position Interactive::raidingDefaultTarget		= Positions::None;



bool Interactive::ProcessCommandVariants(const string & command, const string & attributName, bool & attribut)
{
	if (command == attributName)		   { attribut = !attribut; return true; }
	return false;
}


bool Interactive::ProcessCommand(const string & command)
{
	if (command == "scvs")		{ moreSCVs = !moreSCVs; return true; }
	if (command == "marines")	{ moreMarines = !moreMarines; return true; }
	if (command == "vultures")		{ moreVultures = !moreVultures; if (moreVultures) moreTanks = moreGoliaths = false; return true; }
	if (command == "tanks")			{ moreTanks = !moreTanks;		if (moreTanks) moreVultures = moreGoliaths = false; return true; }
	if (command == "goliaths")		{ moreGoliaths = !moreGoliaths; if (moreGoliaths) moreVultures = moreTanks = false; return true; }
	if (command == "wraiths")		{ moreWraiths = !moreWraiths; return true; }
	if (command == "expand")		{ expand = !expand; return true; }
	if (command == "berserker")		{ berserker = !berserker; return true; }
	if (command.substr(0, 6) == "target")
	{
		string s = command.substr(6);
		if (s.empty()) { raidingDefaultTarget = Positions::None; return true; }

		TilePosition tile;
		std::istringstream iss(s);
		if (from_string(s, tile))
			if (ai()->GetMap().Valid(tile))
				{ raidingDefaultTarget = center(tile); return true; }

		bw << "Invalid tile position. Enter: target <integer> <integer>" << endl;
		return false;
	}

	return false;
}



void drawInteractive()
{
	int x = 3;
	int y = 15;

	if (Interactive::moreSCVs)		{ bw->drawTextScreen(x, y, "%cscvs ++", Text::White); y += 10; }
	if (Interactive::moreMarines)	{ bw->drawTextScreen(x, y, "%cmarines ++", Text::White); y += 10; }
	if (Interactive::moreVultures)	{ bw->drawTextScreen(x, y, "%cvultures ++", Text::White); y += 10; }
	if (Interactive::moreTanks)		{ bw->drawTextScreen(x, y, "%ctanks ++", Text::White); y += 10; }
	if (Interactive::moreGoliaths)	{ bw->drawTextScreen(x, y, "%cgoliaths ++", Text::White); y += 10; }
	if (Interactive::moreWraiths)	{ bw->drawTextScreen(x, y, "%cwraiths ++", Text::White); y += 10; }
	if (Interactive::expand)		{ bw->drawTextScreen(x, y, "%cexpand", Text::White); y += 10; }
	if (Interactive::berserker)		{ bw->drawTextScreen(x, y, "%cberserker", Text::White); y += 10; }
	if (Interactive::raidingDefaultTarget != Positions::None)	{ bw->drawTextScreen(x, y, "%ctarget %d %d", Text::White, TilePosition(Interactive::raidingDefaultTarget).x, TilePosition(Interactive::raidingDefaultTarget).y); y += 10; }
}



} // namespace iron



