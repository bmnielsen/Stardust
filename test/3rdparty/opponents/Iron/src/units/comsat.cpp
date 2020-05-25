//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "comsat.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Comsat_Station>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInConstructingAddon<Terran_Comsat_Station> : public ConstructingAddonExpert
{
public:
						ExpertInConstructingAddon() : ConstructingAddonExpert(Terran_Comsat_Station) {}

	void				UpdateConstructingAddonPriority() override;

private:
};


void ExpertInConstructingAddon<Terran_Comsat_Station>::UpdateConstructingAddonPriority()
{
	if (me().CompletedBuildings(Terran_Academy) == 0) { m_priority = 0; return; }

	if (me().GasAvailable() < TaskCost().Gas() - 10) { m_priority = 0; return; }

	m_priority = 610;
}


ExpertInConstructingAddon<Terran_Comsat_Station>	My<Terran_Comsat_Station>::m_ConstructingAddonExpert;


ConstructingAddonExpert * My<Terran_Comsat_Station>::GetConstructingAddonExpert() { return &m_ConstructingAddonExpert; }

My<Terran_Comsat_Station>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Comsat_Station);

	m_ConstructingAddonExpert.OnBuildingCreated();
}


ConstructingAddonExpert * My<Terran_Comsat_Station>::ConstructingThisAddonExpert()
{CI(this);
	return &m_ConstructingAddonExpert;
}


void My<Terran_Comsat_Station>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;
}


void My<Terran_Comsat_Station>::Scan(Position pos, check_t checkMode)
{CI(this);
	assert_throw(Unit()->getEnergy() >= Cost(TechTypes::Scanner_Sweep).Energy());
///	ai()->SetDelay(500);
///	bw << NameWithId() << " scan at" << pos << "!" << endl;
	bool result = Unit()->useTech(TechTypes::Scanner_Sweep, pos);
	OnCommandSent(checkMode, result, NameWithId() + " scan at " + my_to_string(pos));
}


	
} // namespace iron



