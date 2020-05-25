//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef COMSAT_H
#define COMSAT_H

#include "my.h"


namespace iron
{



template<tid_t> class My;



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Comsat_Station>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

template<>
class My<Terran_Comsat_Station> : public MyBuilding
{
public:
	static ConstructingAddonExpert *	GetConstructingAddonExpert();

										My(BWAPI::Unit u);

	ConstructingAddonExpert *			ConstructingThisAddonExpert() override;

	void								Scan(Position pos, check_t checkMode = check_t::check);

private:
	void								DefaultBehaviorOnFrame() override;

	static ExpertInConstructingAddon<Terran_Comsat_Station>	m_ConstructingAddonExpert;

};



} // namespace iron


#endif

