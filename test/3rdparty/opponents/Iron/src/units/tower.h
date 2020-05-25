//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef TOWER_H
#define TOWER_H

#include "my.h"


namespace iron
{



template<tid_t> class My;



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Control_Tower>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

template<>
class My<Terran_Control_Tower> : public MyBuilding
{
public:
	static ConstructingAddonExpert *	GetConstructingAddonExpert();

										My(BWAPI::Unit u);

	ConstructingAddonExpert *			ConstructingThisAddonExpert() override;

private:
	void								DefaultBehaviorOnFrame() override;

	static ExpertInConstructingAddon<Terran_Control_Tower>	m_ConstructingAddonExpert;

};



} // namespace iron


#endif

