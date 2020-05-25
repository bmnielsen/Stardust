//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "scienceFacility.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Science_Facility>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInConstructing<Terran_Science_Facility> : public ConstructingExpert
{
public:
						ExpertInConstructing() : ConstructingExpert(Terran_Science_Facility) {}

	void				UpdateConstructingPriority() override;

private:
};


void ExpertInConstructing<Terran_Science_Facility>::UpdateConstructingPriority()
{
	if (me().CompletedBuildings(Terran_Factory) == 0) { m_priority = 0; return; }
	if (me().CompletedBuildings(Terran_Starport) == 0) { m_priority = 0; return; }

	if (Builders() < BuildingsUncompleted()) { m_priority = 590; return; }

	if (Buildings() < 1)
	{
		if (me().Buildings(Terran_Control_Tower).size() >= 1)
		{
			if (him().IsProtoss())
			{
				if (him().CreationCount(Protoss_Arbiter) > 0 ||
					him().Buildings(Protoss_Arbiter_Tribunal).size() > 0 ||
					me().CompletedBuildings(Terran_Command_Center) >= 2 && me().CompletedBuildings(Terran_Factory) >= 3)
				{
					{ m_priority = 500; return; }
				}
			}
		}

		if (me().Buildings(Terran_Command_Center).size() >= 3)
			if (me().CompletedBuildings(Terran_Armory) >= (him().IsProtoss() ? 1 : 2))
			{
				static frame_t startingFrame = ai()->Frame();
				m_priority = min(500, (him().IsProtoss() ? 450 : 350) + (ai()->Frame() - startingFrame)/10); return;
			}
	}

	m_priority = 0;
}


ExpertInConstructing<Terran_Science_Facility>	My<Terran_Science_Facility>::m_ConstructingExpert;

ConstructingExpert * My<Terran_Science_Facility>::GetConstructingExpert() { return &m_ConstructingExpert; }

vector<ConstructingAddonExpert *> My<Terran_Science_Facility>::m_ConstructingAddonExperts {};


My<Terran_Science_Facility>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Science_Facility);

	m_ConstructingExpert.OnBuildingCreated();
}


void My<Terran_Science_Facility>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;
}

	
} // namespace iron



