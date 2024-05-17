//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef SCIENCE_FACILITY_H
#define SCIENCE_FACILITY_H

#include "my.h"


namespace iron
{



template<tid_t> class My;



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Science_Facility>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

template<>
class My<Terran_Science_Facility> : public MyBuilding
{
public:
	static ConstructingExpert *	GetConstructingExpert();

							My(BWAPI::Unit u);

	vector<ConstructingAddonExpert *>	ConstructingAddonExperts() override {CI(this); return m_ConstructingAddonExperts; }

private:
	void					DefaultBehaviorOnFrame() override;

	static ExpertInConstructing<Terran_Science_Facility>	m_ConstructingExpert;


	static vector<ConstructingAddonExpert *>	m_ConstructingAddonExperts;

};



} // namespace iron


#endif

