//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "army.h"
#include "factory.h"
#include "../strategy/strategy.h"
#include "../strategy/wraithRush.h"
#include "../strategy/terranFastExpand.h"
#include "../interactive.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Army
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Army::Army()
{
	fill(m_prevValue, m_prevValue + sizePrevValue, 0);
}


void Army::Update()
{
	UpdateStats();
	UpdateHisArmyStats();
	UpdateGroundLead();
	UpdateRatios();
}


void Army::UpdateValue()
{
	for (int i = sizePrevValue-1 ; i > 0 ; --i)
		m_prevValue[i] = m_prevValue[i-1];
	
	m_prevValue[0] = 0;
	for (const auto & u : me().Units())
		if (!u->Is(Terran_SCV))
		{
			int value = Cost(u->Type()).Minerals() + Cost(u->Type()).Gas();

			double k = u->Life() / double(u->MaxLife());
			if (!u->Completed()) k = k*k;

			value = int(value * k);

			m_prevValue[0] += value;
		}

	// 0 - 50=50*1
	// 0 - 100=50*2
	// ...
	// 0 - 1000=50*20

	int badDelta = 0;
	for (int i = 1 ; i <= 20 ; ++i)
	{
		int delta = m_prevValue[0] - m_prevValue[i];
//		bw << delta << "  ";
		if (delta < 0) badDelta = delta;
	}
//	if (badDelta) bw << "  !!!";
//	bw << endl;

	m_goodValueInTime = !badDelta;
}


void Army::UpdateStats()
{
	if (ai()->Frame() % 50 == 0)
		UpdateValue();
}


void Army::UpdateHisArmyStats()
{
	m_hisSoldiersFearingAir =
		1 * him().AllUnits(Terran_Medic).size() +
		1 * him().AllUnits(Terran_Firebat).size() +
		1 * him().AllUnits(Terran_Vulture).size() +
		1 * him().AllUnits(Terran_Siege_Tank_Tank_Mode).size() +
		1 * him().AllUnits(Terran_Siege_Tank_Siege_Mode).size() +
		3 * him().AllUnits(Terran_Dropship).size() +
		3 * him().AllUnits(Terran_Science_Vessel).size() +

		1 * him().AllUnits(Protoss_Zealot).size() +
		3 * him().AllUnits(Protoss_Dark_Templar).size() +
		3 * him().AllUnits(Protoss_Dark_Archon).size() +
		3 * him().AllUnits(Protoss_Reaver).size() +
		3 * him().AllUnits(Protoss_High_Templar).size() +
		3 * him().AllUnits(Protoss_Observer).size() +
		3 * him().AllUnits(Protoss_Shuttle).size() +

		1 * him().AllUnits(Zerg_Zergling).size() +
		3 * him().AllUnits(Zerg_Lurker).size() +
		3 * him().AllUnits(Zerg_Defiler).size() +
		3 * him().AllUnits(Zerg_Guardian).size() +
		1 * him().AllUnits(Zerg_Overlord).size() +
		3 * him().AllUnits(Zerg_Queen).size() +
		1 * him().AllUnits(Zerg_Ultralisk).size();

	m_hisInvisibleUnits =
		him().AllUnits(Terran_Ghost).size() +
		him().AllUnits(Terran_Wraith).size() +
		him().AllUnits(Protoss_Dark_Templar).size() +
		him().AllUnits(Protoss_Observer).size() +
		him().AllUnits(Protoss_Arbiter).size() +
		him().AllUnits(Zerg_Lurker).size();


	m_hisSoldiers = count_if(him().AllUnits().begin(), him().AllUnits().end(),
		[](const pair<Unit, HisKnownUnit> &	trace){ return !trace.second.Type().isWorker(); });
}


void Army::UpdateGroundLead()
{
	m_groundLeadStatus_marines = m_groundLeadStatus_vultures = m_groundLeadStatus_mines = "-";

	if (me().SupplyUsed() >= 175)
	{
		m_groundLead = true;
		return;
	}

	double marinesNeeded = 0.0;
	double vulturesNeeded = 0.0;
	double vulturesWithMinesNeeded = 0.0;
	int hisUnitsFearingAir = 0;

	marinesNeeded += 1.2 * him().AllUnits(Protoss_Zealot).size();
	marinesNeeded += 2.1 * him().AllUnits(Protoss_Dragoon).size();
	marinesNeeded += 3.0 * him().AllUnits(Protoss_Dark_Templar).size();
	marinesNeeded += 6.0 * him().AllUnits(Protoss_Reaver).size();


	vulturesNeeded += 0.3 * him().AllUnits(Terran_Marine).size();
	vulturesNeeded += 0.6 * him().AllUnits(Terran_Medic).size();
	vulturesNeeded += 1.4 * him().AllUnits(Terran_Vulture).size();
	vulturesNeeded += 3.0 * him().AllUnits(Terran_Siege_Tank_Tank_Mode).size();
	vulturesNeeded += 3.0 * him().AllUnits(Terran_Siege_Tank_Siege_Mode).size();
	vulturesNeeded += 2.5 * him().AllUnits(Terran_Goliath).size();

	vulturesNeeded += 0.3 * him().AllUnits(Zerg_Zergling).size();
	vulturesNeeded += 1.5 * him().AllUnits(Zerg_Hydralisk).size();
	vulturesNeeded += 2.0 * him().AllUnits(Zerg_Lurker).size();

	vulturesWithMinesNeeded += 2.0 * him().AllUnits(Terran_Siege_Tank_Tank_Mode).size();
	vulturesWithMinesNeeded += 2.0 * him().AllUnits(Terran_Siege_Tank_Siege_Mode).size();
	vulturesWithMinesNeeded += 1.5 * him().AllUnits(Terran_Goliath).size();

	vulturesWithMinesNeeded += 1.0 * him().AllUnits(Zerg_Hydralisk).size();
	vulturesWithMinesNeeded += 2.0 * him().AllUnits(Zerg_Lurker).size();

	hisUnitsFearingAir += him().AllUnits(Terran_Vulture).size();
	hisUnitsFearingAir += him().AllUnits(Terran_Siege_Tank_Tank_Mode).size();
	hisUnitsFearingAir += him().AllUnits(Terran_Siege_Tank_Siege_Mode).size();

	double marinesAvailable = 0.0;
	double vulturesAvailable = 0.0;
	double vulturesWithMinesAvailable = 0.0;

	const int wraithBonus = min(hisUnitsFearingAir, me().CompletedUnits(Terran_Wraith));
	vulturesNeeded = max(0.0, vulturesNeeded - 1.5 * wraithBonus);
	vulturesWithMinesNeeded = max(0.0, vulturesWithMinesNeeded - 2.0 * wraithBonus);

	for (const auto & u : me().Units(Terran_Marine))
		if (u->Completed())
		{
			marinesAvailable += 1.0 * (0.25 + 0.75 * u->Life() / double(u->MaxLife()));
		}

	for (const auto & u : me().Units(Terran_Vulture))
		if (u->Completed())
		{
			vulturesAvailable += 1.0 * (0.25 + 0.75 * u->Life() / double(u->MaxLife()));
			if (u->IsMy<Terran_Vulture>()->RemainingMines() >= 1)
				vulturesWithMinesAvailable += 1.0;
		}

	for (const auto & u : me().Units(Terran_Siege_Tank_Tank_Mode))
		if (u->Completed())
		{
			vulturesAvailable += 2.0 * (0.25 + 0.75 * u->Life() / double(u->MaxLife()));
		}

	for (const auto & u : me().Units(Terran_Goliath))
		if (u->Completed())
		{
			vulturesAvailable += 1.5 * (0.25 + 0.75 * u->Life() / double(u->MaxLife()));
		}

	for (const auto & u : me().Units(Terran_Vulture_Spider_Mine))
		if (u->Completed())
		{
			vulturesWithMinesAvailable += 0.5 * u->Life() / double(u->MaxLife());
		}


	vulturesAvailable += min(3*me().CompletedUnits(Terran_Wraith), m_hisSoldiersFearingAir/2);


	if (him().StartingBase())
	{
		m_hisGroundUnitsAhead = count_if(him().Units().begin(), him().Units().end(),
				[](const unique_ptr<HisUnit> & u)
				{
					return u->Completed() && !u->Type().isWorker() && u->GroundAttack() && !u->Flying() &&
						(groundDist(u->Pos(), me().StartingBase()->Center()) < groundDist(u->Pos(), him().StartingBase()->Center()));
				});

		m_myGroundUnitsAhead = count_if(me().Units().begin(), me().Units().end(),
				[](const MyUnit * u)
				{
					return u->Completed() && !u->Type().isWorker() && u->GroundAttack() && !u->Flying() && !u->Loaded() &&
						(groundDist(u->Pos(), me().StartingBase()->Center()) > groundDist(u->Pos(), him().StartingBase()->Center()));
				});
	}


	m_marinesCond = marinesAvailable + 0.0001 > marinesNeeded*1.1 + 3.0;
	m_vulturesCond = vulturesAvailable + 0.0001 > vulturesNeeded*1.1 + 3.0;
	m_minesCond = vulturesWithMinesAvailable + 0.0001 > vulturesWithMinesNeeded;
	
	if (him().IsProtoss()) m_vulturesCond = m_minesCond = true;
	else m_marinesCond = true;

	if (him().IsProtoss())
	{
		m_groundLeadStatus_marines = "marines : " + to_string(int(marinesAvailable + 0.5)) + "(need " + to_string(int(marinesNeeded*1.1 + 3 + 0.5)) + ") --> " + (m_marinesCond ? "YES" : "NO");
	}
	else
	{
		m_groundLeadStatus_vultures = "vultures : " + to_string(int(vulturesAvailable + 0.5)) + "(need " + to_string(int(vulturesNeeded*1.1 + 3 + 0.5)) + ") --> " + (m_vulturesCond ? "YES" : "NO");
		m_groundLeadStatus_mines = "mines : " + to_string(int(vulturesWithMinesAvailable + 0.5)) + "(need " + to_string(int(vulturesWithMinesNeeded + 0.5)) + ") --> " + (m_minesCond ? "YES" : "NO");
	}

	m_groundLead = m_marinesCond && m_vulturesCond && m_minesCond;
}


void Army::UpdateRatios_vsTerran()
{
	const int hisGoliaths = him().AllUnits(Terran_Goliath).size();
	const int hisAirUnits = count_if(him().AllUnits().begin(), him().AllUnits().end(), [](const pair<Unit, HisKnownUnit> & p)
									{ return p.second.Type().isFlyer() && !p.second.Type().isBuilding(); });

	m_tankRatioWanted = (me().SupplyUsed() < 100) ? 2 : 3;
	if		(hisGoliaths >= 10) m_tankRatioWanted += 3;
	else if (hisGoliaths >= 5) m_tankRatioWanted += 2;
	else if (hisGoliaths >= 2) m_tankRatioWanted += 1;


	m_goliathRatioWanted = 0;
	if (me().CompletedBuildings(Terran_Armory) >= 1)
		if		(hisAirUnits >= 10) m_goliathRatioWanted = 3;
		else if (hisAirUnits >= 3) m_goliathRatioWanted = 2;
		else if (hisAirUnits >= 1) m_goliathRatioWanted = 1;

	if (ai()->GetStrategy()->Detected<WraithRush>())
	{
		m_tankRatioWanted = 0;
		m_goliathRatioWanted = 5;
	}

	if (ai()->GetStrategy()->Detected<TerranFastExpand>())
	{
		m_tankRatioWanted += 1;
	}

//	m_goliathRatioWanted = 2;
//	m_tankRatioWanted = 4;

}


void Army::UpdateRatios_vsProtoss()
{
	int prosVultures = 0;
	int prosTanks = 0;
	int prosGoliaths = 0;
	int zealots = him().AllUnits(Protoss_Zealot).size();

	for (const auto & info : him().AllUnits())
		switch (info.second.Type())
		{
		case Protoss_Dragoon:							prosTanks += 1;						break;
		case Protoss_Reaver:							prosTanks += 3;						break;
		case Protoss_Arbiter:											prosGoliaths += 1;	break;
		case Protoss_Scout:												prosGoliaths += 3;	break;
		case Protoss_Carrier:											prosGoliaths += 6;	break;
		case Protoss_Archon:		prosVultures += 3;										break;
		case Protoss_Dark_Archon:	prosVultures += 3;										break;
		case Protoss_Dark_Templar:	prosVultures += 3;										break;
//		case Protoss_Zealot:		prosVultures += 2;										break;
		}

	prosVultures += max(zealots/2, zealots*2 - (int)me().Units(Terran_Marine).size());

	if (him().MayCarrier())
		if (me().CompletedUnits(Terran_Goliath) < me().CompletedUnits(Terran_Siege_Tank_Tank_Mode))
			if ((him().CreationCount(Protoss_Carrier) == 0) && (him().CreationCount(Protoss_Arbiter) > 0))
				prosGoliaths += 2;
			else
				prosGoliaths += 10;

	int cannons = him().Buildings(Protoss_Photon_Cannon).size();

	prosTanks += cannons * 2;

	m_tankRatioWanted = int(0.5 + 10 * prosTanks / double(prosVultures + prosTanks + prosGoliaths));
	m_tankRatioWanted = max(0, min(7, m_tankRatioWanted));

	m_goliathRatioWanted = int(0.5 + 10 * prosGoliaths / double(prosVultures + prosTanks + prosGoliaths));
	m_goliathRatioWanted = max(0, min(8, m_goliathRatioWanted));
}


void Army::UpdateRatios_vsZerg()
{
	int prosVultures = 0;
	int prosTanks = 0;
	int prosGoliaths = 0;

	for (const auto & info : him().AllUnits())
		switch (info.second.Type())
		{
		case Zerg_Zergling:		prosVultures += 1;								break;
		case Zerg_Hydralisk:	prosVultures += 2; ++prosTanks;					break;
		case Zerg_Mutalisk:										++prosGoliaths;	break;
		case Zerg_Lurker:		prosVultures += 2; ++prosTanks;					break;
		case Zerg_Defiler:		prosVultures += 2; ++prosTanks;					break;
		case Zerg_Guardian:										++prosGoliaths;	break;
		case Zerg_Overlord:														break;
		case Zerg_Queen:														break;
		case Zerg_Ultralisk:	prosVultures += 6;								break;
		case Zerg_Devourer:										++prosGoliaths;	break;
		case Zerg_Scourge:										++prosGoliaths;	break;
		}

	prosTanks += him().Buildings(Zerg_Sunken_Colony).size();
	if (prosTanks > 0) prosTanks = max(1, prosTanks - (int)him().AllUnits(Zerg_Mutalisk).size()/4);

	prosVultures /= 2;

	m_tankRatioWanted = int(0.5 + 10 * prosTanks / double(prosVultures + prosTanks + prosGoliaths));
	m_tankRatioWanted = max(1, min(max(5, 2 + (int)me().Bases().size()), m_tankRatioWanted));
	if (me().Army().KeepGoliathsAtHome() && (me().CompletedBuildings(Terran_Command_Center) <= 2)) m_tankRatioWanted = 0;

	if (him().AllUnits(Zerg_Mutalisk).size() > him().AllUnits(Zerg_Hydralisk).size() + him().AllUnits(Zerg_Lurker).size())
		if (me().Units(Terran_Goliath).size() <= me().Units(Terran_Siege_Tank_Tank_Mode).size())
		{
			m_tankRatioWanted = 0;
		}

//	if (him().HydraPressure_needVultures())
//		m_tankRatioWanted = 0;

	m_goliathRatioWanted = int(0.5 + 10 * prosGoliaths / double(prosVultures + prosTanks + prosGoliaths));
	m_goliathRatioWanted = max(0, min(6, m_goliathRatioWanted));
	if (me().Army().KeepGoliathsAtHome()) m_goliathRatioWanted = max(m_goliathRatioWanted, 7);
}


void Army::UpdateRatios()
{
	static bool started = false;
	
	if (!me().Buildings(Terran_Armory).empty() ||
		(me().Units(Terran_Siege_Tank_Tank_Mode).size() >= 1) ||
		him().IsProtoss())
		started = true;


	if (started)
	{
		if		(him().IsTerran())  UpdateRatios_vsTerran();
		else if (him().IsProtoss()) UpdateRatios_vsProtoss();
		else if (him().IsZerg())    UpdateRatios_vsZerg();
	}

	if (Interactive::moreGoliaths)
	{
		m_tankRatioWanted = 0;
		m_goliathRatioWanted = 10;
	}

	if (Interactive::moreTanks)
	{
		m_tankRatioWanted = 10;
		m_goliathRatioWanted = 0;
	}

	if (Interactive::moreVultures)
	{
		m_tankRatioWanted = 0;
		m_goliathRatioWanted = 0;
	}

	double tankRatio = (me().Units(Terran_Siege_Tank_Tank_Mode).size()) * 10 / (double)(1 +	me().Units(Terran_Vulture).size() +
																		me().Units(Terran_Siege_Tank_Tank_Mode).size() +
																		me().Units(Terran_Goliath).size());

	double goliathRatio = (me().Units(Terran_Goliath).size()) * 10 / (double)(1 +	me().Units(Terran_Vulture).size() +
																		me().Units(Terran_Siege_Tank_Tank_Mode).size() +
																		me().Units(Terran_Goliath).size());

	m_favorTanksOverGoliaths = m_tankRatioWanted - tankRatio > m_goliathRatioWanted - goliathRatio;
}


bool Army::KeepGoliathsAtHome() const
{
	if (!him().IsZerg()) return false;

	if (him().AllUnits(Zerg_Mutalisk).size() >= 4)
		if (him().AllUnits(Zerg_Mutalisk).size() >= 1.5*me().Units(Terran_Goliath).size())
			return true;

	return false;
}


bool Army::KeepTanksAtHome() const
{
	if (him().HydraPressure() || him().HydraPressure_needVultures())
		if (me().Bases().size() >= 2)
		{
			if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) < 4)
				return true;
		}

	if (!him().IsProtoss()) return false;

	if (him().Buildings(Protoss_Photon_Cannon).size() >= 2)
	{
		if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) < 5)
			if (!(me().CompletedUnits(Terran_Siege_Tank_Tank_Mode)*4 < (int)him().AllUnits(Protoss_Dragoon).size()*3))
				 return false;
	}

	if (him().Buildings(Protoss_Photon_Cannon).size() >= 1)
		if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) + me().CompletedUnits(Terran_Vulture)  >
			2*int(him().AllUnits(Protoss_Dragoon).size() + him().AllUnits(Protoss_Zealot).size()))
				 return false;


	if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) >= 15) return false;

	if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode)*4 < (int)him().AllUnits(Protoss_Dragoon).size()*3) return true;
		
	if (me().CompletedUnits(Terran_Vulture)*2 < (int)him().AllUnits(Protoss_Zealot).size()*1) return true;

	if (me().CompletedBuildings(Terran_Command_Center) <= 2)
		if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) + me().CompletedUnits(Terran_Vulture) < 20) return true;

	return false;
}



} // namespace iron



