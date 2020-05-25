//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "him.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/walling.h"
#include "../behavior/checking.h"
#include "../strategy/expand.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }

namespace iron
{

class Him & him() { return ai()->Him(); }


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Him
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


const BWAPI::Player Him::Player() const
{
	return bw->enemy();
}


BWAPI::Player Him::Player()
{
	return bw->enemy();
}


const Base * Him::StartingBase() const
{
	return m_pStartingBase ? m_pStartingBase->BWEMPart() : nullptr;
}


const Base * Him::Natural() const
{
	return m_pNatural ? m_pNatural->BWEMPart() : nullptr;
}


VBase * Him::FindStartingBase() const
{
	assert_throw(!m_pStartingBase);

	if (ai()->GetStrategy()->HisPossibleLocations().size() == 1)
		return ai()->GetVMap().GetBaseAt(ai()->GetStrategy()->HisPossibleLocations().front());

	for (auto & b : Buildings())
		if (b->Type().isResourceDepot())
		{
		//	ai()->SetDelay(50);
			for (auto & base : ai()->GetVMap().Bases())
				if (base.BWEMPart()->Starting())
					if (abs(base.BWEMPart()->Location().x - b->TopLeft().x) < 4)
					if (abs(base.BWEMPart()->Location().y - b->TopLeft().y) < 3)
						return &base;
			}

	for (auto & b : Buildings())
		if (!b->Flying())
			for (auto & base : ai()->GetVMap().Bases())
				if (base.BWEMPart()->Starting())
					if (!contains(me().Bases(), &base))
						if (dist(base.BWEMPart()->Location(), b->TopLeft()) < 48)
							if (any_of(b->GetArea()->ChokePoints().begin(), b->GetArea()->ChokePoints().end(),
								[&b](const ChokePoint * p){ return dist(TilePosition(p->Center()), b->TopLeft()) < 10; }))
									return &base;

	return nullptr;


/*
	vector<VBase *> HisMainAndNaturalCandidates;
	for (auto & base : ai()->GetVMap().Bases())
		if (base.BWEMPart()->Starting())
			if (!contains(me().Bases(), &base))
			{
				HisMainAndNaturalCandidates.push_back(&base);
				if (VBase * pNatural = findNatural(&base))
					if (!contains(HisMainAndNaturalCandidates, pNatural))
						HisMainAndNaturalCandidates.push_back(pNatural);
			}

	for (auto & b : Buildings())
		for (VBase * base : HisMainAndNaturalCandidates)
//			if (dist(base.BWEMPart()->Location(), b->TopLeft()) < 48)
				if (any_of(b->GetArea()->ChokePoints().begin(), b->GetArea()->ChokePoints().end(),
					[&b](const ChokePoint * p){ return dist(center(p->Center()), b->Pos()) < 10*32; }))
					{
						SetStartingBase(base);
						return;
					}
	for (auto & u : Units())
		if (!(u->Type().isWorker() || u->Flying()))
			if (!u->Unit()->isMoving())
				for (VBase * base : HisMainAndNaturalCandidates)
		//			if (dist(base.BWEMPart()->Location(), b->TopLeft()) < 48)
						if (any_of(u->GetArea()->ChokePoints().begin(), u->GetArea()->ChokePoints().end(),
							[&u](const ChokePoint * p){ return dist(center(p->Center()), u->Pos()) < 10*32; }))
							{
								SetStartingBase(base);
								return;
							}
*/
}


void Him::UpdateStartingBase()
{
	if (m_pStartingBase) return;

	if (VBase * pStartingBase = FindStartingBase())
	{
		SetStartingBase(pStartingBase);
		m_pNatural = findNatural(StartingVBase());
		if (m_pNatural) m_pNaturalArea = m_pNatural->BWEMPart()->GetArea();
	}
}


void Him::Update()
{
/*
	for (Unit u : bw->getAllUnits())
		if (u->getType().isSpell())// == UnitTypes::Spell_Scanner_Sweep)
		{
			bw << u->getType().getName() << " " << u->getType().getID() << " " << u->getPosition() << endl;
			bw->drawCircleMap(u->getPosition(), 10*32, Colors::Cyan, true);
		}
*/

	for (auto & u : Units())
		exceptionHandler("Him::Update(" + u->NameWithId() + ")", 5000, [&u]()
		{
			u->Update();
		});

	UpdateKnwownUnits();

	for (auto & b : Buildings())
		exceptionHandler("Him::Update(" + b->NameWithId() + ")", 5000, [&b]()
		{
			b->Update();
		});

	{	// Remove any InFog-HisUnit having at least one tile visible since 10 frames. See also Him::OnShowUnit
		vector<BWAPI::Unit> ObsoleteInFogUnits;
		for (auto & u : Units())
			if (u->InFog())
				if (!(u->Type().isBurrowable() || u->Is(Terran_Vulture_Spider_Mine)))
					if (ai()->Frame() - u->LastFrameNoVisibleTile() > 10)
						ObsoleteInFogUnits.push_back(u->Unit());

		for (BWAPI::Unit u : ObsoleteInFogUnits)
			RemoveUnit(u, !bool("informOthers"));
	}

	{	// Remove any InFog-HisBuilding having at least one tile visible since 10 frames. See also Him::OnShowBuilding
		vector<BWAPI::Unit> ObsoleteInFogBuildings;
		for (auto & b : Buildings())
			if (b->InFog())
				if (ai()->Frame() - b->LastFrameNoVisibleTile() > 10)
					ObsoleteInFogBuildings.push_back(b->Unit());

		for (BWAPI::Unit b : ObsoleteInFogBuildings)
			RemoveBuilding(b, !bool("informOthers"));
	}

	UpdateStartingBase();
	CheckTanksInFog();

	m_hasBuildingsInNatural = him().NaturalArea() &&
		any_of(Buildings().begin(), Buildings().end(), [](const unique_ptr<HisBuilding> & b)
			{ return b->Completed() && contains(ext(him().NaturalArea())->EnlargedArea(), b->GetArea(check_t::no_check)); });

	if (!m_speedLings)
		if (!him().Units(Zerg_Zergling).empty())
			if (him().Units(Zerg_Zergling).front()->Completed())
				if (him().Units(Zerg_Zergling).front()->Speed() > 7.0)
					m_speedLings = true;

}


void Him::AddUnit(BWAPI::Unit u)
{
	assert_throw_plus(u->getPlayer() == Player(), u->getType().getName());
	assert_throw(!u->getType().isBuilding());

	assert_throw_plus(none_of(m_Units.begin(), m_Units.end(), [u](const unique_ptr<HisUnit> & up){ return up->Unit() == u; }), to_string(u->getID()));
	m_Units.push_back(make_unique<HisUnit>(u));
	PUSH_BACK_UNCONTAINED_ELEMENT(m_UnitsByType[u->getType()], m_Units.back().get());
}


void Him::RemoveUnit(BWAPI::Unit u, bool informOthers)
{
	auto iHisUnit = find_if(m_Units.begin(), m_Units.end(), [u](const unique_ptr<HisUnit> & up){ return up->Unit() == u; });
	if (iHisUnit != m_Units.end())
	{
		{
			vector<HisUnit *> & TypedList = m_UnitsByType[(*iHisUnit)->Type()];
			auto i = find(TypedList.begin(), TypedList.end(), iHisUnit->get());
			assert_throw(i != TypedList.end());
			fast_erase(TypedList, distance(TypedList.begin(), i));
		}

		if (informOthers && !iHisUnit->get()->InFog()) me().OnBWAPIUnitDestroyed_InformOthers(iHisUnit->get());
		fast_erase(m_Units, distance(m_Units.begin(), iHisUnit));
	}
}


void Him::AddBuilding(BWAPI::Unit u)
{
	assert_throw_plus(u->getPlayer() == Player(), u->getType().getName());
	assert_throw(u->getType().isBuilding());

	assert_throw(none_of(m_AllBuildings.begin(), m_AllBuildings.end(), [u](const unique_ptr<HisBuilding> & up){ return up->Unit() == u; }));
	m_AllBuildings.push_back(make_unique<HisBuilding>(u));
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Buildings[u->getType()], m_AllBuildings.back().get());
}


void Him::RemoveBuilding(BWAPI::Unit u, bool informOthers)
{
	auto iHisBuilding = find_if(m_AllBuildings.begin(), m_AllBuildings.end(), [u](const unique_ptr<HisBuilding> & up){ return up->Unit() == u; });
	if (iHisBuilding != m_AllBuildings.end())
	{
	//	bw << "Him::RemoveBuilding(" << iHisBuilding->get()->NameWithId() << ")" << endl;

		{
			vector<HisBuilding *> & TypedList = m_Buildings[(*iHisBuilding)->Type()];
			auto i = find(TypedList.begin(), TypedList.end(), iHisBuilding->get());
			assert_throw(i != TypedList.end());
			fast_erase(TypedList, distance(TypedList.begin(), i));
		}

		if (informOthers && !iHisBuilding->get()->InFog()) me().OnBWAPIUnitDestroyed_InformOthers(iHisBuilding->get());
		fast_erase(m_AllBuildings, distance(m_AllBuildings.begin(), iHisBuilding));
	}
}


HisUnit * Him::FindUnit(BWAPI::Unit u) const
{
	auto iHisUnit = find_if(m_Units.begin(), m_Units.end(), [u](const unique_ptr<HisUnit> & up){ return up->Unit() == u; });
	return iHisUnit != m_Units.end() ? iHisUnit->get() : nullptr;
}


HisBuilding * Him::FindBuilding(BWAPI::Unit u) const
{
	auto iHisBuilding = find_if(m_AllBuildings.begin(), m_AllBuildings.end(), [u](const unique_ptr<HisBuilding> & up){ return up->Unit() == u; });
	return iHisBuilding != m_AllBuildings.end() ? iHisBuilding->get() : nullptr;
}


void Him::UpdateKnwownUnits()
{
	for (auto & b : Buildings())
		if (!b->InFog())
			if (b->Type().isBuilding())
				m_KnownUnitMap.erase(b->Unit());

	for (auto & u : Units())
	{
		if (!u->InFog())
			if (u->Type() != Terran_Vulture_Spider_Mine)
			{
				if (u->Type().isBuilding())
					m_KnownUnitMap.erase(u->Unit());
				else
				{
					auto iInserted = m_KnownUnitMap.emplace(u->Unit(), u.get());
					auto & trace = iInserted.first->second;
					if (!iInserted.second) trace.Update(u.get());
					assert_throw(trace.LastTimeVisible() == ai()->Frame());
				}
			}
	}

	for (auto & e : m_KnownUnitMap)
		if (e.second.LastTimeVisible() != ai()->Frame())
			e.second.Update(nullptr);


	for (auto & e : m_KnownUnitMap)
	{
		assert_throw(e.first->getType() != Terran_Vulture_Spider_Mine);
		assert_throw(!e.second.GetHisUnit() || !e.second.GetHisUnit()->InFog());
	}


	{	// repopulate m_KnownUnits
		for (auto & List : m_KnownUnits)
			List.clear();

		for (auto & e : m_KnownUnitMap)
			AllUnits(e.second.Type()).push_back(&e.second);
	}
}


void Him::CheckTanksInFog()
{
	vector<BWAPI::Unit> ToCheck;
	for (const auto & u : m_Units)
		if (u->InFog())
			if (u->Is(Terran_Siege_Tank_Siege_Mode) || u->Is(Zerg_Lurker) || u->Is(Terran_Goliath) || u->Is(Protoss_Reaver))
			{
				auto iTrace = m_KnownUnitMap.find(u->Unit());
				assert_throw(iTrace != m_KnownUnitMap.end());

			///	if (u->Is(Terran_Goliath))
			///		bw << "check " << u->NameWithId() << " in " << iTrace->second.NextTimeToCheck() - ai()->Frame() << endl;

				if (iTrace->second.TimeToCheck())
				{
				///	bw << "check " << u->NameWithId() << " : " << endl;

					// If possible, we simply assign a unit to check if u is still there:
					// if (u->getType() == Terran_Siege_Tank_Siege_Mode)
//					if (ai()->Frame() - iTrace->second.NextTimeToCheck() < 200)
					const int limitToCheck = (200 + ai()->Frame()/200) * (u->Is(Terran_Siege_Tank_Siege_Mode ? 5 : 1));
					if (ai()->Frame() - iTrace->second.NextTimeToCheck() < limitToCheck)
					{
						if (u->Is(Terran_Goliath) || u->Is(Protoss_Reaver))
						{
						///	ai()->SetDelay(1000);
						///	bw << ai()->Frame() << ") check " << u->NameWithId() << " after " << ai()->Frame() - iTrace->second.NextTimeToCheck() << endl;
							iTrace->second.OnChecked();
							return;
						}

						if (MyUnit * pCandidate = Checking::FindCandidate(u.get()))
						{
						///	ai()->SetDelay(1000);
						///	bw << ai()->Frame() << ") check " << u->NameWithId() << " after " << ai()->Frame() - iTrace->second.NextTimeToCheck() << endl;
							pCandidate->ChangeBehavior<Checking>(pCandidate, u.get());
							iTrace->second.OnChecked();
							return;
						}
					}
					else // Otherwise, after a delay, we remove u so that u won't be considered as a threat anymore (until u shows again):
					{
						bool severalTanks = false;
						if (u->Is(Terran_Siege_Tank_Siege_Mode))
							for (const auto & closeTank : AllUnits(Terran_Siege_Tank_Siege_Mode))
								if (closeTank->LastPosition() != u->Pos())
									if (ai()->Frame() - closeTank->LastTimeVisible() < limitToCheck)
										if (roundedDist(closeTank->LastPosition(), u->Pos()) < 8*32)
											severalTanks = true;

						if (!severalTanks)
						{
						///	ai()->SetDelay(1000);
						///	bw << ai()->Frame() << ") forget inFog Unit " << u->NameWithId() << " after " << ai()->Frame() - iTrace->second.NextTimeToCheck() << endl;
							RemoveUnit(u->Unit(), !bool("informOthers"));
							iTrace->second.OnChecked();
							return;
						}
					}
				}
			}
}


void Him::CheckObsoleteInFogUnit(BWAPI::Unit u)
{
	vector<BWAPI::Unit> ObsoleteInFogUnits;
	for (const auto & v : m_Units)
		if (v->Unit() == u)
		{
			assert_throw(v->InFog());
			ObsoleteInFogUnits.push_back(v->Unit());
		}

	for (BWAPI::Unit v : ObsoleteInFogUnits)
		RemoveUnit(v, !bool("informOthers"));
}


void Him::CheckObsoleteInFogBuilding(BWAPI::Unit u)
{
	vector<BWAPI::Unit> ObsoleteInFogBuildings;
	for (const auto & b : m_AllBuildings)
		if (b->Unit() == u)
		{
			assert_throw(b->InFog());
			ObsoleteInFogBuildings.push_back(b->Unit());
		}

	for (BWAPI::Unit b : ObsoleteInFogBuildings)
		RemoveBuilding(b, !bool("informOthers"));
}


void Him::OnShowUnit(BWAPI::Unit u)
{
	// First, remove any corresponding InFog-HisUnit. See also Him::Update
	CheckObsoleteInFogUnit(u);

	assert_throw(none_of(m_AllBuildings.begin(), m_AllBuildings.end(), [u](const unique_ptr<HisBuilding> & up){ return up->Unit() == u; }));

	AddUnit(u);
}


void Him::OnShowBuilding(BWAPI::Unit u)
{
	// Check destroyed Neutrals not triggered by onUnitDestroy because the were not visible.
	// TODO: refactor and do the check globally as soon as one tile of the destroyed Neutrals is visible.
	for (int dy = 0 ; dy < u->getType().tileSize().y ; ++dy)
	for (int dx = 0 ; dx < u->getType().tileSize().x ; ++dx)
	{
		auto & tile = ai()->GetMap().GetTile(u->getTilePosition() + TilePosition(dx, dy));
		if (Neutral * n = tile.GetNeutral())
		{
			if (n->IsMineral())
			{
			///	bw << "Check destroyed Mineral " << n->TopLeft() << endl;
				ai()->OnMineralDestroyed(n->Unit());
			}
			else if (n->IsStaticBuilding())
			{
			///	bw << "Check destroyed Static Building " << n->TopLeft() << endl;
				ai()->OnStaticBuildingDestroyed(n->Unit());
			}
		}
	}

	// First, remove any corresponding InFog-HisBuilding. See also Him::Update
	CheckObsoleteInFogUnit(u);		// in case of zerg's morphing
	CheckObsoleteInFogBuilding(u);

	AddBuilding(u);
}


void Him::OnHideUnit(BWAPI::Unit u)
{
	auto iHisUnit = find_if(m_Units.begin(), m_Units.end(), [u](const unique_ptr<HisUnit> & up){ return up->Unit() == u; });
	if (iHisUnit != m_Units.end())
	{
		if (
			iHisUnit->get()->Is(Zerg_Lurker)/* && iHisUnit->get()->Burrowed()*/ ||
			iHisUnit->get()->Is(Terran_Siege_Tank_Siege_Mode) ||
		//	iHisUnit->get()->Type().isWorker() ||
			iHisUnit->get()->Is(Terran_Vulture_Spider_Mine) ||
			iHisUnit->get()->Is(Terran_Goliath) ||
			iHisUnit->get()->Is(Protoss_Reaver)
			)
		{
		//	if (iHisUnit->get()->Is(Terran_Vulture_Spider_Mine))
		//	{ bw << " SetInFog : " << (*iHisUnit)->Type().getName() << endl; ai()->SetDelay(500); }

			me().OnBWAPIUnitDestroyed_InformOthers(iHisUnit->get());
			iHisUnit->get()->SetInFog();
		}
		else
		{
		//	if (iHisUnit->get()->Is(Zerg_Lurker))
		//	{ bw << " RemoveUnit : " << (*iHisUnit)->Type().getName() << endl; ai()->SetDelay(5000); }

			RemoveUnit(u, bool("informOthers"));
		}
	}
}


void Him::OnHideBuilding(BWAPI::Unit u)
{
	auto iHisBuilding = find_if(m_AllBuildings.begin(), m_AllBuildings.end(), [u](const unique_ptr<HisBuilding> & up){ return up->Unit() == u; });
	if (iHisBuilding != m_AllBuildings.end())
	{
	//	if (iHisBuilding->get()->Flying()) { RemoveBuilding(u, bool("informOthers")); return; }

		me().OnBWAPIUnitDestroyed_InformOthers(iHisBuilding->get());
		iHisBuilding->get()->SetInFog();
	}
}


void Him::OnDestroyed(BWAPI::Unit u)
{
	RemoveUnit(u, !bool("informOthers"));
	RemoveBuilding(u, !bool("informOthers"));
	
	auto it = m_KnownUnitMap.find(u);
	if (it != m_KnownUnitMap.end())
	{
		really_remove(AllUnits(it->second.Type()), &it->second);
		m_KnownUnitMap.erase(it);
		++m_lostUnits[u->getType()];

		if (u->getType() == Protoss_Dragoon)
		{
			m_lastDragoonKilledPos = u->getPosition();
			m_lastDragoonKilledFrame = ai()->Frame();
		}
	}

}


const Area * Him::GetArea() const
{
	return m_pStartingBase ? m_pStartingBase->BWEMPart()->GetArea() : nullptr;
}


bool Him::CanDetect(Position pos) const
{
	for (UnitType t : {Terran_Missile_Turret, Protoss_Photon_Cannon, Zerg_Spore_Colony})
		for (const auto & b : Buildings(t))
			if (roundedDist(b->Pos(), pos) < 7*32 + 32)
				return true;
		
	for (UnitType t : {Terran_Science_Vessel, Protoss_Observer, Zerg_Overlord})
		for (const auto & u : AllUnits(t))
			if (roundedDist(u->LastPosition(), pos) < 11*32 + 32)
				return true;
		
	return false;
	
}


bool Him::ZerglingPressure() const
{
	if (m_lastZerglingPressureFrame && (ai()->Frame() - m_lastZerglingPressureFrame < 100))
		return true;

	if (IsZerg())
		if (!ai()->GetStrategy()->Detected<ZerglingRush>())
		if (!ai()->GetStrategy()->Active<Walling>())
			if (me().Units(Terran_Vulture).size() >= 1)
			if (him().AllUnits(Zerg_Zergling).size() >= 6)
			{
				//if (me().Units(Terran_Vulture).size() < 15) return true;

				if (  me().Units(Terran_Vulture).size() * (me().Buildings(Terran_Command_Center).size() < 2 ? 2 : 3)
					+ me().Units(Terran_Marine).size() * me().Buildings(Terran_Bunker).size()
					< AllUnits(Zerg_Zergling).size())
				{
					// cancel second Machine Shop
					DO_ONCE
						if (me().Buildings(Terran_Machine_Shop).size() == 2)
						{
							MyBuilding * pSecondShop = nullptr;
							int maxTimeToComplete = numeric_limits<int>::min();
							for (const auto & b : me().Buildings(Terran_Machine_Shop))
								if (!b->Completed())
									if (b->RemainingBuildTime() > maxTimeToComplete)
									{
										pSecondShop = b.get();
										maxTimeToComplete = b->RemainingBuildTime();
									}

							if (pSecondShop)
								if (pSecondShop->CanAcceptCommand())
									pSecondShop->CancelConstruction();
						}

				//	bw << "ZerglingPressure ! ZerglingPressure ! ZerglingPressure ! ZerglingPressure ! " << endl;
					m_lastZerglingPressureFrame = ai()->Frame();
					return true;
				}
			}

	return false;
}


bool Him::HydraPressure_needVultures() const
{
	m_maxHydraAllTimes = max(m_maxHydraAllTimes, (int)AllUnits(Zerg_Hydralisk).size());
	double score = me().Units(Terran_Vulture).size()*1.3 + 2*me().Units(Terran_Siege_Tank_Tank_Mode).size()
					- m_maxHydraAllTimes*2;
	if (me().Buildings(Terran_Command_Center).size() >= 2)
		if (score < 0)
		{
		///	for (int i = 0 ; i < 3 ; ++i) bw->drawBoxScreen(Position(i, i)+5, Position(640-1-i, 374-1-i)-5, Colors::Orange);
		///	bw->drawTextScreen(320, 30, "%c%f", Text::Orange, score);
			
			m_lastHydraPressure_needVulturesFrame = ai()->Frame();
			return true;
		}

	if (m_lastHydraPressure_needVulturesFrame && (ai()->Frame() - m_lastHydraPressure_needVulturesFrame < 500))
	{
	///	for (int i = 0 ; i < 3 ; ++i) bw->drawBoxScreen(Position(i, i)+5, Position(640-1-i, 374-1-i)-5, Colors::White);

		return true;
	}

	return false;
}


bool Him::HydraPressure() const
{
	if (m_lastHydraPressureFrame && (ai()->Frame() - m_lastHydraPressureFrame < 100))
		return true;

	if (IsZerg())
		if (!ai()->GetStrategy()->Detected<ZerglingRush>())
			if (me().Buildings(Terran_Command_Center).size() < 2)
				if (AllUnits(Zerg_Hydralisk).size() >= 2)
					if (AllUnits(Zerg_Hydralisk).size()*1.5 >= me().Units(Terran_Vulture).size() + 2*me().Units(Terran_Siege_Tank_Tank_Mode).size())
					{
						//if (me().Units(Terran_Vulture).size() < 15) return true;

						if (!MayMuta())
						{
							DO_ONCE
								for (const auto & b : me().Buildings(Terran_Engineering_Bay))
									if (!b->Completed())
										if (b->CanAcceptCommand())
											b->CancelConstruction();
						}

						m_lastHydraPressureFrame = ai()->Frame();
						return true;
					}

	return false;
}

	
} // namespace iron



