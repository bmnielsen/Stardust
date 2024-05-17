//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "his.h"
#include "../territory/vgridMap.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class HisUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


HisUnit::HisUnit(BWAPI::Unit u)
	: HisBWAPIUnit(u)
{
	ai()->GetGridMap().Add(this);

//	if (Type() == Zerg_Lurker) DO_ONCE { ai()->SetDelay(500); bw << "lurker !!!" << endl; }
//	if (Type() == Protoss_Dark_Templar) DO_ONCE { ai()->SetDelay(500); bw << "dark templar !!!" << endl; }
}


HisUnit::~HisUnit()
{
#if !DEV
	try //3
#endif
	{
		if (!InFog()) ai()->GetGridMap().Remove(this);
	}
#if !DEV
	catch(...){} //3
#endif
}


void HisUnit::UpdateMineWatchedOn()
{CI(this);
	m_watchedOn = false;
	if (Type() == Terran_Vulture_Spider_Mine)
	{
		vector<MyUnit *> MyUnitsNearby = ai()->GetGridMap().GetMyUnits(
													ai()->GetMap().Crop(TilePosition(Pos())-10),
													ai()->GetMap().Crop(TilePosition(Pos())+10));

		vector<const MyUnit *> MyVulturesNearby;
		vector<const MyUnit *> MyOthersUnitsNearby;
		bool tankOrGoliathNearby = false;

		for (const MyUnit * u : MyUnitsNearby)
			if (u->Completed() && !u->Loaded())
				if (!u->CoolDown())
					if (roundedDist(u->Pos(), Pos()) < u->GroundRange() + 2*32)
					{
						if (u->Is(Terran_Vulture))
						{
							MyVulturesNearby.push_back(u);
#if DEV
/*
							for (int i = 0 ; i < 5 ; ++i)
							{
								bw->drawCircleMap(u->Pos(), u->GroundRange() + 2*32 + i, Colors::Yellow);
								bw->drawCircleMap(Pos(), 16 + i, Colors::Yellow);
							}

							for (int x = -2 ; x <= +2 ; ++x)
							for (int y = -2 ; y <= +2 ; ++y)
							{
								drawLineMap(u->Pos() + Position(x, y), Pos() + Position(x, y), Colors::Yellow);
							}
*/
#endif
						}

						else if (u->Is(Terran_Siege_Tank_Tank_Mode) && !u->GetBehavior()->IsSieging() ||
								u->Is(Terran_Goliath) ||
								u->Is(Terran_Wraith))
						{
							if (!u->Is(Terran_Wraith)) tankOrGoliathNearby = true;
							MyOthersUnitsNearby.push_back(u);
						}
					}

		if (!MyVulturesNearby.empty()) m_watchedOn = true;
		else
		{
			bool scatteredUnits = false;
			for (const MyUnit * u1 : MyOthersUnitsNearby)
			for (const MyUnit * u2 : MyOthersUnitsNearby)
				if (u1 != u2)
					if (roundedDist(u1->Pos(), u2->Pos()) > 5*32)
						scatteredUnits = true;

			if (tankOrGoliathNearby)
			if (!scatteredUnits)
			if (MyOthersUnitsNearby.size() >= 2)
			{
				m_watchedOn = true;
#if DEV
			///	ai()->SetDelay(500);
/*
				for (const MyUnit * u : MyOthersUnitsNearby)
				{
					for (int i = 0 ; i < 5 ; ++i)
					{
						bw->drawCircleMap(u->Pos(), u->GroundRange() + 2*32 + i, Colors::Yellow);
						bw->drawCircleMap(Pos(), 16 + i, Colors::Yellow);
					}

					for (int x = -2 ; x <= +2 ; ++x)
					for (int y = -2 ; y <= +2 ; ++y)
					{
						drawLineMap(u->Pos() + Position(x, y), Pos() + Position(x, y), Colors::Yellow);
					}
				}

				for (const MyUnit * u1 : MyOthersUnitsNearby)
				for (const MyUnit * u2 : MyOthersUnitsNearby)
					if (u1 != u2)
					{
						for (int x = -2 ; x <= +2 ; ++x)
						for (int y = -2 ; y <= +2 ; ++y)
						{
							drawLineMap(u1->Pos() + Position(x, y), u2->Pos() + Position(x, y), Colors::Orange);
						}
					}
*/
#endif
			}
		}
	}
}


void HisUnit::Update()
{CI(this);
	if (InFog())
	{
		if (!bw->isVisible(TilePosition(Pos())))
			UpdatedLastFrameNoVisibleTile();

		UpdateMineWatchedOn();
	}
	else
	{
		ai()->GetGridMap().Remove(this);
		HisBWAPIUnit::Update();
		ai()->GetGridMap().Add(this);

		m_minesTargetingThis.clear();
	}
}


void HisUnit::SetPursuingTarget(MyUnit * pAgent)
{CI(this);
	m_pPursuingTarget = pAgent;
	m_pPursuingTargetLastFrame = ai()->Frame();
}


void HisUnit::AddDestroyer(Destroying * pDestroyer)
{CI(this);
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Destroyers, pDestroyer);
}


void HisUnit::RemoveDestroyer(Destroying * pDestroyer)
{CI(this);
	assert_throw(contains(m_Destroyers, pDestroyer));
	really_remove(m_Destroyers, pDestroyer);
}


void HisUnit::OnWorthScaning()
{CI(this);
	m_lastWorthScaning = ai()->Frame();
}


bool HisUnit::WorthScaning() const
{CI(this);
	return ai()->Frame() - m_lastWorthScaning < 10;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class HisBuilding
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


HisBuilding::HisBuilding(BWAPI::Unit u)
	: HisBWAPIUnit(u), m_size(u->getType().tileSize())
{
	if (!him().FoundBuildingsInMain())
		if (!Flying() && (GetArea() == him().GetArea()))
			him().SetFoundBuildingsInMain();

	if (!Flying()) PutBuildingOnTiles();
	ai()->GetGridMap().Add(this);
}


HisBuilding::~HisBuilding()
{
#if !DEV
	try //3
#endif
	{
		if (!InFog() && !Flying()) RemoveBuildingFromTiles();
		if (!InFog()) ai()->GetGridMap().Remove(this);
	}
#if !DEV
	catch(...){} //3
#endif
}


bool HisBuilding::AtLeastOneTileIsVisible() const
{CI(this);
	for (auto t : {TopLeft(), TopLeft() + TilePosition(Size().x-1, 0), TopLeft() + TilePosition(0, Size().y-1), TopLeft() + Size()-1})
		if (bw->isVisible(t))
			return true;

	return false;
}


void HisBuilding::Update()
{CI(this);
	if (InFog())
	{
		if (!AtLeastOneTileIsVisible())
			UpdatedLastFrameNoVisibleTile();
	}
	else
	{
		ai()->GetGridMap().Remove(this);
		HisBWAPIUnit::Update();
		ai()->GetGridMap().Add(this);

		if (JustLifted()) RemoveBuildingFromTiles();
		if (JustLanded()) PutBuildingOnTiles();
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class HisKnownUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


HisKnownUnit::HisKnownUnit(HisUnit * u)
//	: m_bwapiUnitType(Unknown)
{
	assert_throw(u);
	assert_throw_plus(!u->InFog(), u->NameWithId());
	Update(u);
	ResetCheckedInfo();
}


void HisKnownUnit::Update(HisUnit * u)
{CTHIS;
	m_pHisUnit = u;
	if (u)
	{
		assert_throw_plus(!u->InFog(), u->NameWithId());

		if ((m_bwapiUnitType != u->Type()) ||
			!u->Is(Terran_Goliath) && !u->Is(Protoss_Reaver) && (m_lastPosition != u->Pos()) ||
			u->Is(Terran_Goliath) && (dist(m_lastPosition, u->Pos()) < 5*32) ||
			u->Is(Protoss_Reaver) && (dist(m_lastPosition, u->Pos()) < 5*32))
			ResetCheckedInfo();

		/*if (u->Type() != Unknown)*/ m_bwapiUnitType = u->Type();
		m_lastPosition = u->Pos();
		m_noMoreHere = m_noMoreHere3 = false;
		m_lastLife = u->Life();
		m_lastShields = u->Shields();
		m_lastTimeVisible = ai()->Frame();
	}
	else
	{
		TilePosition t(m_lastPosition);
		if (!m_noMoreHere)
			if (bw->isVisible(t))
				m_noMoreHere = true;

		if (m_noMoreHere)
			if (!m_noMoreHere3)
			{
				if (!ai()->GetMap().Valid(t + TilePosition(-3, 0)) || bw->isVisible(t + TilePosition(-3, 0)))
				if (!ai()->GetMap().Valid(t + TilePosition(+3, 0)) || bw->isVisible(t + TilePosition(+3, 0)))
				if (!ai()->GetMap().Valid(t + TilePosition(0, -3)) || bw->isVisible(t + TilePosition(0, -3)))
				if (!ai()->GetMap().Valid(t + TilePosition(0, +3)) || bw->isVisible(t + TilePosition(0, +3)))
				{
					m_noMoreHere3 = true;
#if DEV
					for (int i = 0 ; i < 5 ; ++i) bw->drawCircleMap(m_lastPosition, 25 + i, Colors::Grey);
#endif
				}
			}

	}
}


void HisKnownUnit::ResetCheckedInfo()
{CTHIS;
	m_checkedCount = 0;
	m_lastTimeChecked = ai()->Frame();
	
	UpdateNextTimeToCheck();
}


void HisKnownUnit::OnChecked()
{CTHIS;
	m_lastTimeChecked = ai()->Frame();
	++m_checkedCount;

	UpdateNextTimeToCheck();
}


void HisKnownUnit::UpdateNextTimeToCheck()
{CTHIS;

//	int n = (Type() == Terran_Siege_Tank_Siege_Mode ? 2*m_checkedCount : m_checkedCount);
	int n = m_checkedCount;
	int q = Type() == Terran_Siege_Tank_Siege_Mode ? 300 :
			Type() == Protoss_Reaver ? 300 :
			Type() == Terran_Goliath ? 150 : 200;

	int kMax = (Type() == Terran_Goliath) ? 1000 : (Type() == Protoss_Reaver) ? 1000 : 2000;

	int k = min(kMax, q * (n + 1));
	m_nextTimeToCheck = m_lastTimeChecked + k + rand()%k;

}


bool HisKnownUnit::TimeToCheck() const
{CTHIS;
	return ai()->Frame() >= m_nextTimeToCheck;
}



	
} // namespace iron



