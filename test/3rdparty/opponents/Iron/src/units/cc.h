//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef CC_H
#define CC_H

#include "my.h"


namespace iron
{



template<tid_t> class My;



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Command_Center>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

template<>
class My<Terran_Command_Center> : public MyBuilding
{
public:
	static ConstructingExpert *	GetConstructingExpert();

							My(BWAPI::Unit u);

private:
	void					DefaultBehaviorOnFrame() override;

	double					MinLifePercentageToRepair() const override	{ return 0.75; }
	double					MaxLifePercentageToRepair() const override	{ return 0.80; }

	static ExpertInConstructing<Terran_Command_Center>	m_ConstructingExpert;
};





//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_SCV>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


template<>
class My<Terran_SCV> : public MyUnit
{
public:
							My(BWAPI::Unit u);

	void					Gather(Mineral * m, check_t checkMode = check_t::check);
	void					Gather(Geyser * g, check_t checkMode = check_t::check);
	void					ReturnCargo(check_t checkMode = check_t::check);
	void					Repair(MyBWAPIUnit * target, check_t checkMode = check_t::check);
	bool					Build(BWAPI::UnitType type, TilePosition location, check_t checkMode = check_t::check);

	void					SetSoldierForever()		{CI(this); m_soldierForever = true; }
	bool					SoldierForever() const	{CI(this); return m_soldierForever; }

private:
	void					DefaultBehaviorOnFrame() override;
	bool					m_soldierForever = false;
};




} // namespace iron


#endif

