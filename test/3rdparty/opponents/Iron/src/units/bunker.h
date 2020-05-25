//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef BUNKER_H
#define BUNKER_H

#include "my.h"


namespace iron
{


template<tid_t> class My;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Bunker>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

template<>
class My<Terran_Bunker> : public MyBuilding
{
public:
	static ConstructingExpert *	GetConstructingExpert();

							My(BWAPI::Unit u);

	void					Load(BWAPIUnit * u, check_t checkMode = check_t::check);
	void					Unload(BWAPIUnit * u, check_t checkMode = check_t::check);
	int						LoadedUnits() const;
	int						Snipers() const;


private:
	void					DefaultBehaviorOnFrame() override;

	double					MinLifePercentageToRepair() const override	{ return 0.999; }
	double					MaxLifePercentageToRepair() const override	{ return 1.0; }
	int						MaxRepairers() const override;

	int						m_nextWatchingRadius;
	frame_t					m_lastCallForRepairer = 0;


	static ExpertInConstructing<Terran_Bunker>	m_ConstructingExpert;
};


} // namespace iron


#endif

