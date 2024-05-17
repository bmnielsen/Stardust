//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "refinery.h"
#include "../strategy/strategy.h"
#include "../strategy/cannonRush.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/workerRush.h"
#include "../strategy/zealotRush.h"
#include "../interactive.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Refinery>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInConstructing<Terran_Refinery> : public ConstructingExpert
{
public:
						ExpertInConstructing() : ConstructingExpert(Terran_Refinery) {}

	void				UpdateConstructingPriority() override;

private:
};


void ExpertInConstructing<Terran_Refinery>::UpdateConstructingPriority()
{
	if (ai()->GetStrategy()->Detected<WorkerRush>())
		{ m_priority = 0; return; }

	if (ai()->GetStrategy()->Detected<ZealotRush>())
		{ m_priority = 0; return; }

	if (auto s = ai()->GetStrategy()->Detected<ZerglingRush>())
	{
		if (!s->TechRestartingCondition())
			{ m_priority = 0; return; }

		if ((int)me().Units(Terran_Marine).size() < s->MaxMarines())
			if (!me().UnitsBeingTrained(Terran_Marine))
				{ m_priority = 0; return; }

		if (me().Units(Terran_SCV).size() < 20)
			if (!me().UnitsBeingTrained(Terran_SCV))
				if (me().SupplyAvailable() > 0)
					{ m_priority = 0; return; }
	}

	if (Builders() < BuildingsUncompleted()) { m_priority = 590; return; }

	if (Buildings() == 0)
	{
		if (!me().Buildings(Terran_Barracks).empty())
		{
//			if (Interactive::moreTanks || Interactive::moreGoliaths || Interactive::moreWraiths)
//				{ m_priority = 590; return; }

			{
				if (me().SupplyUsed() >= 12)	m_priority = 610;
				else							m_priority = 590;
/*
				if (!(him().IsTerran() || him().IsProtoss()))
					if (me().GetVArea()->Wall().size() >= 3)
						if (me().Buildings(Terran_Supply_Depot).size() < 2)
							m_priority = 0;
*/
			}
			return;
		}
	}
	else
	{
		if (!him().IsProtoss() || (me().Buildings(Terran_Factory).size() >= 1) || (me().Buildings(Terran_Barracks).size() >= 4))
			if (me().GasAvailable() < 200)
				for (VBase * base : me().Bases())
					if (base->Active())
						if (!base->BWEMPart()->Geysers().empty())
							if (none_of(me().Buildings(Terran_Refinery).begin(), me().Buildings(Terran_Refinery).end(),
										[base](const unique_ptr<MyBuilding> & b){ return b->GetStronghold() == base->GetStronghold(); }))
							{
								m_priority = 500;
								return;
							}
	}

	m_priority = 0;
}


ExpertInConstructing<Terran_Refinery>	My<Terran_Refinery>::m_ConstructingExpert;

ConstructingExpert * My<Terran_Refinery>::GetConstructingExpert() { return &m_ConstructingExpert; }


My<Terran_Refinery>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Refinery);

	m_ConstructingExpert.OnBuildingCreated();
}


void My<Terran_Refinery>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;
}


int My<Terran_Refinery>::MaxRepairers() const
{
	int n =	(Life()*4 > MaxLife()*3) ? 1 :
			(Life()*4 > MaxLife()*2) ? 2 : 3;

	if (ai()->GetStrategy()->Detected<CannonRush>())
		n += 1;

	return n;
}




	
} // namespace iron



