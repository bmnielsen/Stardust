//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "production.h"
#include "expert.h"
#include "cc.h"
#include "depot.h"
#include "refinery.h"
#include "bunker.h"
#include "barracks.h"
#include "factory.h"
#include "shop.h"
#include "starport.h"
#include "tower.h"
#include "comsat.h"
#include "ebay.h"
#include "academy.h"
#include "armory.h"
#include "scienceFacility.h"
#include "turret.h"
#include "../behavior/mining.h"
#include "../behavior/refining.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


double gatheringRatio()
{
	return Mining::Instances().size() / (double)Refining::Instances().size();
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Production
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Production::Production()
{
	Record(My<Terran_Command_Center>::GetConstructingExpert());
	Record(My<Terran_Refinery>::GetConstructingExpert());
	Record(My<Terran_Bunker>::GetConstructingExpert());
	Record(My<Terran_Supply_Depot>::GetConstructingExpert());
	Record(My<Terran_Barracks>::GetConstructingExpert());
	Record(My<Terran_Factory>::GetConstructingExpert());
	Record(My<Terran_Starport>::GetConstructingExpert());
	Record(My<Terran_Engineering_Bay>::GetConstructingExpert());
	Record(My<Terran_Academy>::GetConstructingExpert());
	Record(My<Terran_Armory>::GetConstructingExpert());
	Record(My<Terran_Missile_Turret>::GetConstructingExpert());
	Record(My<Terran_Missile_Turret>::GetConstructingFreeExpert());
	Record(My<Terran_Science_Facility>::GetConstructingExpert());

	Record(My<Terran_Machine_Shop>::GetConstructingAddonExpert());
	Record(My<Terran_Control_Tower>::GetConstructingAddonExpert());
	Record(My<Terran_Comsat_Station>::GetConstructingAddonExpert());
}


void Production::Record(Expert * pExpert)
{
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Experts, pExpert);

}


void Production::Remove(Expert * pExpert)
{
	auto i = find(m_Experts.begin(), m_Experts.end(), pExpert);
	assert_throw(i != m_Experts.end());
	fast_erase(m_Experts, distance(m_Experts.begin(), i));
}


int Production::MineralsAvailable() const		{ return me().MineralsAvailable() - m_mineralsReserved; }
int Production::GasAvailable() const			{ return me().GasAvailable() - m_gasReserved; }
int Production::SupplyAvailable() const			{ return me().SupplyAvailable() - m_supplyReserved; }


void Production::Select(Expert * pExpert)
{
	assert_throw(pExpert->Unselected());
	
	m_mineralsReserved += pExpert->TaskCost().Minerals();
	m_gasReserved += pExpert->TaskCost().Gas();
	m_supplyReserved += pExpert->TaskCost().Supply();

	pExpert->Select();
}


void Production::Unselect(Expert * pExpert)
{
	assert_throw(!pExpert->Unselected());
	
	m_mineralsReserved -= pExpert->ReservedCost().Minerals();
	m_gasReserved -= pExpert->ReservedCost().Gas();
	m_supplyReserved -= pExpert->ReservedCost().Supply();

	assert_throw_plus(m_mineralsReserved >= 0, "m_mineralsReserved = " + to_string(m_mineralsReserved) + " " + pExpert->ToString());
	assert_throw_plus(m_gasReserved >= 0, "m_gasReserved = " + to_string(m_gasReserved) + " " + pExpert->ToString());
	assert_throw_plus(m_supplyReserved >= 0, "m_supplyReserved = " + to_string(m_supplyReserved) + " " + pExpert->ToString());

	pExpert->Unselect();
}


void Production::OnFrame()
{
	if (SupplyAvailable() >= 0)
		m_lastTimePositiveSupply = ai()->Frame();


	if (!((MineralsAvailable() >= 0) && (GasAvailable() >= 0))) return;
	
	if (!(SupplyAvailable() >= 0))
	{
		if (ai()->Frame() - m_lastTimePositiveSupply < 50)
			return;
	}

	for (Expert * e : m_Experts)
		if (e->Ready())
			if (e->Selected())
				Unselect(e);

	for (Expert * e : m_Experts)
		if (e->Ready())
			if (e->Unselected())
				e->UpdatePriority();

	sort(m_Experts.begin(), m_Experts.end(),
		[](const Expert * a, const Expert * b)
		{
//			if (a->TaskCost().Nul() && !b->TaskCost().Nul()) return true;
//			if (b->TaskCost().Nul() && !a->TaskCost().Nul()) return false;
			return a->Priority() > b->Priority();
		});

	int lastPriorityToCheck = 1;
	for (Expert * e : m_Experts)
		if (e->Ready())
			if (e->Unselected())
			{
				if (e->PriorityLowerThanOtherExperts()) continue;
//				if (!e->TaskCost().Nul() && (e->Priority() < lastPriorityToCheck)) break;
				if (e->Priority() < lastPriorityToCheck) break;

				if (e->TaskCost().Minerals() <= MineralsAvailable() &&
					e->TaskCost().Gas()		 <= GasAvailable() &&
					e->TaskCost().Supply()	 <= max(0, SupplyAvailable()))
				{
					Select(e);
				}
				else //if (!e->TaskCost().Nul())
				{
					assert_throw_plus(!e->TaskCost().Nul(), e->ToString() + " ; " + to_string(MineralsAvailable()) + ", " + to_string(GasAvailable()) + ", " + to_string(SupplyAvailable()));
					lastPriorityToCheck = e->Priority();
				}
			}
}


} // namespace iron



