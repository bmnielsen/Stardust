//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef TURRET_H
#define TURRET_H

#include "my.h"


namespace iron
{


template<tid_t> class My;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Missile_Turret>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

template<>
class My<Terran_Missile_Turret> : public MyBuilding
{
public:
	static ConstructingExpert *	GetConstructingExpert();
	static ConstructingExpert *	GetConstructingFreeExpert();

							My(BWAPI::Unit u);

private:
	void					DefaultBehaviorOnFrame() override;

	double					MinLifePercentageToRepair() const override	{ return 0.999; }
	double					MaxLifePercentageToRepair() const override	{ return 1.0; }
	int						MaxRepairers() const override;

	int						m_nextWatchingRadius;
	frame_t					m_lastCallForRepairer = 0;

	static ExpertInConstructing<Terran_Missile_Turret>	m_ConstructingExpert;
	static ExpertInConstructingFree<Terran_Missile_Turret>	m_ConstructingFreeExpert;
};

MyBuilding * findAloneTurret(VBase * base, int radius);

} // namespace iron


#endif

