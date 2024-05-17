//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef WALLING_H
#define WALLING_H

#include <BWAPI.h>
#include "strat.h"
#include "../territory/wall.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
FORWARD_DECLARE_MY(Terran_SCV)

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Walling
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Walling : public Strat
{
public:
									Walling();
									~Walling();

	string							Name() const override { return "Walling"; }
	string							StateDescription() const override;

	bool							Active() const		{ return m_active; }

	Wall *							GetWall() const;

private:
	void							OnFrame_v() override;

	bool							m_active = false;
};



} // namespace iron


#endif

