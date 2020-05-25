//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "depot.h"
#include "../strategy/strategy.h"
#include "../strategy/walling.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/zealotRush.h"
#include "../strategy/marineRush.h"
#include "../strategy/workerRush.h"
#include "../strategy/wraithRush.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Supply_Depot>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInConstructing<Terran_Supply_Depot> : public ConstructingExpert
{
public:
						ExpertInConstructing() : ConstructingExpert(Terran_Supply_Depot) {}

	void				UpdateConstructingPriority() override;

private:
};


void ExpertInConstructing<Terran_Supply_Depot>::UpdateConstructingPriority()
{
	if (Builders() < BuildingsUncompleted()) { m_priority = 590; return; }

	if (me().SupplyMax() == 200) { m_priority = 0; return; }
/*	
	if (him().IsProtoss())
		if (Buildings() == 3)
		{
			if (me().Buildings(Terran_Command_Center).size() < 2)
			{
				m_priority = 0;
				return;
			}
		}
*/		
	/*
	if (me().Buildings(Terran_Supply_Depot).size() < 5)
		if (me().BuildingsBeingTrained(Terran_Supply_Depot))
			{ m_priority = 0; return; }
*/

	const int supplyAvailable = me().SupplyAvailable();

	if (ai()->GetStrategy()->Detected<ZerglingRush>() && ai()->GetStrategy()->Detected<ZerglingRush>()->FastPool() ||
		ai()->GetStrategy()->Detected<MarineRush>())
			if (supplyAvailable >= 2)
				for (const auto & b : me().Buildings(Terran_Factory))
					if (b->Completed() && b->CanAcceptCommand() && !b->Unit()->isTraining())
						{ m_priority = 0; return; }

	if (auto s = ai()->GetStrategy()->Detected<ZerglingRush>())
		if (s->FastPool())
			if (me().SupplyMax() == 18)
				if ((me().SupplyAvailable() >= 3) || me().Buildings(Terran_Bunker).empty() || me().Units(Terran_Marine).empty())
					{ m_priority = 0; return; }

	if (ai()->GetStrategy()->Detected<WorkerRush>())
		if (me().SupplyMax() == 18)
			if (me().SupplyAvailable() >= 3)
				{ m_priority = 0; return; }

	if (him().IsProtoss())
		if (ai()->GetStrategy()->Active<Walling>())
			if (me().SupplyMax() == 26)
				if (me().SupplyAvailable() >= 2)
					if (me().Units(Terran_Siege_Tank_Tank_Mode).empty() && me().Units(Terran_Vulture).empty() ||
						me().BuildingsBeingTrained(Terran_Machine_Shop))
						{ m_priority = 0; return; }

	if (him().IsZerg())
		if (ai()->GetStrategy()->Active<Walling>())
			if (me().SupplyAvailable() >= 2)
				if (me().CompletedBuildings(Terran_Factory))
					if (me().Buildings(Terran_Machine_Shop).empty())
						if (!me().UnitsBeingTrained(Terran_Vulture))
							{ m_priority = 0; return; }


	if (auto s = ai()->GetStrategy()->Detected<WraithRush>())
		if (me().CompletedBuildings(Terran_Command_Center) < 2)
			if (Buildings() >= 4)
				if (me().SupplyAvailable() >= 2)
					{ m_priority = 0; return; }


/**
	if (ai()->GetStrategy()->Detected<ZerglingRush>())
		if (me().Units(Terran_Vulture).size() == 0)
			if (me().SupplyAvailable() >= 1)
				if (me().MineralsAvailable() + 3*(int)Mining::Instances().size() < 150)
					if (!me().UnitsBeingTrained(Terran_Marine))
						{ m_priority = 0; return; }
*/
//	if (!ai()->GetStrategy()->Detected<ZerglingRush>())
//		if (me().Buildings(Terran_Supply_Depot).size() == 2)
//			if (me().Buildings(Terran_Factory).size() == 0)
//				{ m_priority = 0; return; }

	if (ai()->GetStrategy()->Active<Walling>())
		if (Buildings() == 0)
			if (me().SupplyUsed() <= 8)
				{ m_priority = 0; return; }

	if (Buildings() == 1)
	{
/*
		if (me().GetWall() && me().GetWall()->Active())
			if (me().GetWall()->Size() >= 3)
				if (me().SupplyUsed() >= 13)
				{
					m_priority = 1001;
					return;
				}
*/

		const bool mayBeZerg = !(him().IsProtoss() || him().IsTerran());
		int startSupply = mayBeZerg ? 14 : 15;

		if (mayBeZerg)
			if (auto s = ai()->GetStrategy()->Active<Walling>())
				if (s->GetWall()->Size() == 3)
					startSupply = 13;

		if (me().SupplyUsed() < startSupply)
			{ m_priority = 0; return; }

		if (me().SupplyUsed() == startSupply)
			if (me().MineralsAvailable() >= 50)
				if (auto s = ai()->GetStrategy()->Active<Walling>())
					if (s->GetWall()->Size() == 3)
					{
						if (!mayBeZerg)
							for (const auto & b : me().Buildings(Terran_Barracks))
								if (!b->Completed() && (b->RemainingBuildTime() < 300))
								{
								//	DO_ONCE bw << "Terran_Barracks RemainingBuildTime = " << b->RemainingBuildTime() << " - Waiting builder" << endl;
									m_priority = 0;
									return;
								}

						m_priority = 1001;
						return;
					}
	}

	if (BuildingsUncompleted() >= (Buildings() < 8 ? 1 : 2))
		{ m_priority = 0; return; }

/*
	if (him().IsProtoss())
		if (me().Buildings(Terran_Factory).size() == 2)
			if (Buildings() == 2)
				if (me().SupplyUsed() < 22)
					{ m_priority = 0; return; }
*/

	double trainingCapacity = 0;
	for (auto type : {Terran_Command_Center, Terran_Barracks, Terran_Factory, Terran_Starport})
	{
		double value = (type == Terran_Starport) ? 0.5 : 1.0;
		for (const auto & b : me().Buildings(type))
			if (b->Completed())
				if (b->Unit()->isTraining() && !b->Unit()->getTrainingQueue().empty())
				{
					frame_t buildTime = b->Unit()->getTrainingQueue().front().buildTime();
					trainingCapacity += value * (buildTime - b->TimeToTrain()) / buildTime;
				}
				else
					trainingCapacity += value;
			else
				trainingCapacity += value * (300 - min(300, b->RemainingBuildTime())) / 300;
	}

	bool someSupplyProviderHasJustStartedBuilding = false;
	double supplyBeeingCreated = 0;
	for (auto type : {Terran_Supply_Depot, Terran_Command_Center})
		for (const auto & b : me().Buildings(type))
			if (!b->Completed())
			{
				supplyBeeingCreated += b->Type().supplyProvided()/2 * (b->Type().buildTime() - b->RemainingBuildTime()) / b->Type().buildTime();
				if (b->RemainingBuildTime() > b->Type().buildTime()/2)
					someSupplyProviderHasJustStartedBuilding = true;
			}

//	bw << ai()->Frame() << ") ExpertInConstructing : " << me().Buildings(Terran_Supply_Depot).size() << " depots : " << supplyBeeingCreated << " "  << trainingCapacity << " " << endl;

	int n = supplyAvailable + lround(supplyBeeingCreated - trainingCapacity - 2 - (int)me().Buildings(Terran_Supply_Depot).size());
#if DEV
	bw->drawTextScreen(615, 25, "%c(%d)", Text::White, n);
#endif
/*
	// prevents from building several supply providers at the same time
	if (someSupplyProviderHasJustStartedBuilding && (n >= -2))
		{ m_priority = 0; return; }
*/
	if (n >= (int)me().Buildings(Terran_Supply_Depot).size()/2)
		{ m_priority = 0; return; }


	if (n < 0)	m_priority = -n * 1000;
	else		m_priority = 500 / (n+1);
}


ExpertInConstructing<Terran_Supply_Depot>	My<Terran_Supply_Depot>::m_ConstructingExpert;

ConstructingExpert * My<Terran_Supply_Depot>::GetConstructingExpert() { return &m_ConstructingExpert; }


My<Terran_Supply_Depot>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Supply_Depot);

	m_ConstructingExpert.OnBuildingCreated();
}


void My<Terran_Supply_Depot>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;
}




	
} // namespace iron



