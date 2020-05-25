//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "vmap.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }

namespace iron
{


int pathExists(const Position & a, const Position &  b)
{
	int length;
	ai()->GetMap().GetPath(a, b, &length);
	return (length >= 0);
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class VMap
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


VMap::VMap(BWEM::Map * pMap)
	: m_pMap(pMap)
{
	m_Bases.reserve(pMap->BaseCount());
	m_Areas.reserve(pMap->Areas().size());
	m_ChokePoints.reserve(pMap->ChokePointCount());

	ChokePoint::UnmarkAll();
	for (const Area & area : pMap->Areas())
	{
		m_Areas.emplace_back(&area);

		for (const Base & base : area.Bases())
			m_Bases.emplace_back(&base);

		for (const ChokePoint * cp : area.ChokePoints())
			if (!cp->Marked())
			{
				cp->SetMarked();
				m_ChokePoints.emplace_back(cp);
			}
	}

	vector<const Neutral *> Neutrals;
	for (const auto & m : pMap->Minerals()) Neutrals.push_back(m.get());
	for (const auto & g : pMap->Geysers()) Neutrals.push_back(g.get());
	for (const auto & sb : pMap->StaticBuildings()) Neutrals.push_back(sb.get());

	for (const Neutral * n : Neutrals)
	{
		vector<TilePosition> Border = outerBorder(n->TopLeft(), n->Size(), bool("noCorner"));
		for (auto t : Border)
			if (pMap->Valid(t))
				ai()->GetVMap().SetNearNeutral(pMap->GetTile(t, check_t::no_check));

		if (n->IsRessource())
			for (int dy = -3 ; dy < n->Size().y + 3 ; ++dy)
			for (int dx = -3 ; dx < n->Size().x + 3 ; ++dx)
			{
				TilePosition t = n->TopLeft() + TilePosition(dx, dy);
				if (pMap->Valid(t))
					ai()->GetVMap().SetCommandCenterImpossible(pMap->GetTile(t, check_t::no_check));
			}
	}

	for (const VBase & base : m_Bases)
		for (int dy = 0 ; dy < UnitType(Terran_Command_Center).tileSize().y ; ++dy)
		for (int dx = 0 ; dx < UnitType(Terran_Command_Center).tileSize().x + 2 ; ++dx)
		{
			TilePosition t = base.BWEMPart()->Location() + TilePosition(dx, dy);
			if (pMap->Valid(t))
				ai()->GetVMap().SetCommandCenterWithAddonRoom(pMap->GetTile(t, check_t::no_check));
		}

	Initialize();
}


void VMap::Initialize()
{
	for (VBase & base : m_Bases)
		base.Initialize();

	for (VArea & area : Areas())
		area.Initialize();

	for (VChokePoint & cp : ChokePoints())
		cp.Initialize();

	ComputeImpasses(1);
	ComputeImpasses(2);

	for (VBase & base : m_Bases)
		if (base.BWEMPart()->Starting())
			base.AssignWall();

}


void VMap::Update()
{
	for (VChokePoint & cp : ChokePoints())
		cp.Update();

	if (m_MainPath.empty())
		if (him().StartingBase())
			m_MainPath = ai()->GetMap().GetPath(me().StartingBase()->Center(), him().StartingBase()->Center(), &m_MainPathLength); 
}


VBase * VMap::GetBaseAt(TilePosition location)
{
	auto it = find_if(m_Bases.begin(), m_Bases.end(), [location](const VBase & base){ return base.BWEMPart()->Location() == location; });
	return it == m_Bases.end() ? nullptr : &*it;
}


WalkPosition VMap::GetIncreasingAltitudeDirection(WalkPosition pos) const
{
	assert_throw(GetMap()->Valid(pos));

	altitude_t maxAltitude = -1;
	WalkPosition bestDir;

	for (WalkPosition delta : { WalkPosition(-1, -1), WalkPosition(0, -1), WalkPosition(+1, -1),
								WalkPosition(-1,  0),                      WalkPosition(+1,  0),
								WalkPosition(-1, +1), WalkPosition(0, +1), WalkPosition(+1, +1)})
	{
		WalkPosition w = pos + delta;
		if (GetMap()->Valid(w))
		{
			altitude_t altitude = GetMap()->GetMiniTile(w, check_t::no_check).Altitude();
			if (altitude > maxAltitude)
			{
				maxAltitude = altitude;
				bestDir = delta;
			}
		}
	}

	return bestDir;
}


Position VMap::RandomPosition(Position center, int radius) const
{
	return GetMap()->Crop(center + Position(radius - rand()%(2*radius), radius - rand()%(2*radius)));
}


Position VMap::RandomSeaPosition() const
{
	for (int tries = 0 ; tries < 1000 ; ++tries)
	{
		Position p = GetMap()->RandomPosition();
		if (GetMap()->GetMiniTile(WalkPosition(p), check_t::no_check).Sea())
			return p;
	}

	return GetMap()->RandomPosition();
}


bool VMap::Impasse(int i, const Tile & tile) const
{
	switch(i)
	{
	case 1: return Impasse<4>(tile);
	case 2: return Impasse<8>(tile);
	}

	assert_throw(false);
}


void VMap::SetImpasse(int i, const Tile & tile)
{
	switch(i)
	{
	case 1: return SetImpasse<4>(tile);
	case 2: return SetImpasse<8>(tile);
	}

	assert_throw(false);
}


void VMap::UnsetImpasse(int i, const Tile & tile)
{
	switch(i)
	{
	case 1: return UnsetImpasse<4>(tile);
	case 2: return UnsetImpasse<8>(tile);
	}

	assert_throw(false);
}


void VMap::ComputeImpasses_MarkRessources()
{
	vector<const Ressource *> Ressources;
	for (const auto & m : ai()->GetMap().Minerals())
		Ressources.push_back(m.get());
	for (const auto & g : ai()->GetMap().Geysers())
		Ressources.push_back(g.get());

	for (const Ressource * r : Ressources)
	{
		bool edgeRessource = false;
		for (int ndy = 0 ; ndy < r->Size().y ; ++ndy) if (!edgeRessource)
		for (int ndx = 0 ; ndx < r->Size().x ; ++ndx) if (!edgeRessource)
		{
			TilePosition t = r->TopLeft() + TilePosition(ndx, ndy);
			if (GetMap()->GetTile(t).MinAltitude() <= 3*32)
				edgeRessource = true;
		}
		if (!edgeRessource) continue;

		for (int ndy = 0 ; ndy < r->Size().y ; ++ndy)
		for (int ndx = 0 ; ndx < r->Size().x ; ++ndx)
		{
			TilePosition t = r->TopLeft() + TilePosition(ndx, ndy);
			for (int dy = -4 ; dy <= +4 ; ++dy)
			for (int dx = -4 ; dx <= +4 ; ++dx)
			{
				TilePosition t2 = t + TilePosition(dx, dy);
				if (!GetMap()->Valid(t2)) continue;

				const Tile & tile2 = GetMap()->GetTile(t2);
				if (!tile2.Marked())
					if (dist(t, t2) <= 4)
					{
						tile2.SetMarked();
					}
			}
		}
	}
}


void VMap::ComputeImpasses(int kind)
{
	Tile::UnmarkAll();

	if (kind == 2) ComputeImpasses_MarkRessources();

	for (const Area & area : GetMap()->Areas())
	{
		int k = 4*32;

		altitude_t maxAltitude = area.MaxAltitude();
//		MapPrinter::Get().Circle(WalkPosition(area.Top()), maxAltitude/8, MapPrinter::Color(255, 0, 255));

		int impasseCount = 0;
		for (int tryNum = 1 ; tryNum <= 2 ; ++tryNum)
		{
			if ((maxAltitude < k+2*32) || (tryNum == 2)) k = (maxAltitude/2 + 16)/32*32;

			for (int y = area.TopLeft().y ; y <= area.BottomRight().y ; ++y)
			for (int x = area.TopLeft().x ; x <= area.BottomRight().x ; ++x)
			{
				TilePosition t(x, y);
				const Tile & tile = GetMap()->GetTile(t);
				if (tile.AreaId() == area.Id())
				{
					if (k <= tile.MinAltitude())
					{
						if (!Impasse(kind, GetMap()->GetTile(t)))
						{
							SetImpasse(kind, GetMap()->GetTile(t));
							++impasseCount;
						}

						if (tile.MinAltitude() <= k+1*32 && !tile.Marked())
							for (int dy = -k/32 ; dy <= +k/32 ; ++dy)
							for (int dx = -k/32 ; dx <= +k/32 ; ++dx)
							{
								TilePosition t2(x + dx, y + dy);
								if (!GetMap()->Valid(t2)) continue;

								const Tile & tile2 = GetMap()->GetTile(t2);
								if (tile2.AreaId() == area.Id())
								{
									if (dist(t, t2) <= (k/32))
										if (!Impasse(kind, GetMap()->GetTile(t2)))
										{
											SetImpasse(kind, GetMap()->GetTile(t2));
											++impasseCount;
										}
								}
							}
					}
				}
			}

			if (tryNum == 1)
			{
			///	Log << impasseCount << " / " << area.MiniTiles()/16 << "   " << double(impasseCount)/(area.MiniTiles()/16) << endl;
				if (double(impasseCount)/max(1, (area.MiniTiles()/16)) >= 0.65) break;
			}
		}
	}

	for (const Area & area : GetMap()->Areas())
	{
		for (const ChokePoint * cp : area.ChokePoints())
			if (!cp->Blocked())
			{
				ComputeImpasses_FillFromChokePoint(kind, area, cp);
			
				for (WalkPosition w : cp->Geometry())
					SetImpasse(kind, GetMap()->GetTile(TilePosition(w)));
			}
	}

	for (int y = 0 ; y < GetMap()->Size().y ; ++y)
	for (int x = 0 ; x < GetMap()->Size().x ; ++x)
	{
		const Tile & tile = GetMap()->GetTile(TilePosition(x, y), check_t::no_check);
		if (Impasse(kind, tile))	UnsetImpasse(kind, tile);
		else						SetImpasse(kind, tile);
	}
}


void VMap::ComputeImpasses_FillFromChokePoint(int kind, const Area & area, const ChokePoint * cp)
{
	Tile::UnmarkAll();
	TilePosition start = TilePosition(cp->PosInArea(ChokePoint::middle, &area));
	queue<TilePosition> ToVisit;
	ToVisit.push(start);
	const Tile & Start = GetMap()->GetTile(start, check_t::no_check);
	Start.SetMarked();

	int matchesCount = 0;
	altitude_t cpAltitude = ext(cp)->MaxAltitude();
	int dd = Start.MinAltitude() + 0;
	TilePosition maxAltitudeTile = TilePosition(cp->Center());
	while (!ToVisit.empty())
	{
		TilePosition current = ToVisit.front();
		ToVisit.pop();
		for (TilePosition delta : {	TilePosition(-1, -1), TilePosition(0, -1), TilePosition(+1, -1),
									TilePosition(-1,  0),                      TilePosition(+1,  0),
									TilePosition(-1, +1), TilePosition(0, +1), TilePosition(+1, +1)})
		{
			TilePosition next = current + delta;
			if (GetMap()->Valid(next))
			{
				const Tile & Next = GetMap()->GetTile(next, check_t::no_check); 
				if (!Next.Marked())
					if (Next.AreaId() == area.Id())
					{
						int d = Next.MinAltitude() + int(dist(next, start)*32/2);
						if (d >= dd) { dd = d; maxAltitudeTile = next; }

						Next.SetMarked();
						ToVisit.push(next);

						if (!Impasse(kind, Next))
						{
							if (dist(next, maxAltitudeTile)*32 <= cpAltitude+3*32)
								SetImpasse(kind, Next);
						}
						else
						{
							if (++matchesCount == (2*cpAltitude/32 + 2)*(2*cpAltitude/32 + 2)) return;
						}
					}
			}
		}
	}
}


void VMap::PrintImpasses() const
{
#if BWEM_USE_MAP_PRINTER
	for (int y = 0 ; y < GetMap()->Size().y ; ++y)
	for (int x = 0 ; x < GetMap()->Size().x ; ++x)
	{
		const Tile & tile = GetMap()->GetTile(TilePosition(x, y));
		if (tile.Walkable())
		if (tile.AreaId() > 0 || tile.AreaId() == -1)
		{
			WalkPosition origin(TilePosition(x, y));
			MapPrinter::Color col = MapPrinter::Color(255, 255, 255);
			if (Impasse(1, tile))
				MapPrinter::Get().Rectangle(origin + 0, origin + 3, col);
			if (Impasse(2, tile))
				MapPrinter::Get().Rectangle(origin + 1, origin + 2, col);
		}
	}
#endif
}


int VMap::BuildingsAndNeutralsAround(TilePosition center, int radius) const
{
	vector<void *> DistinctBuildingsAndNeutrals;

	for (int dy = -radius ; dy <= +radius ; ++dy)
	for (int dx = -radius ; dx <= +radius ; ++dx)
	{
		TilePosition t = center + TilePosition(dx, dy);
		if (GetMap()->Valid(t))
		{
			const Tile & tile = GetMap()->GetTile(t, check_t::no_check);

			if (BWAPIUnit * b = GetBuildingOn(tile))	push_back_if_not_found(DistinctBuildingsAndNeutrals, b);
			else if (Neutral * n = tile.GetNeutral())	push_back_if_not_found(DistinctBuildingsAndNeutrals, n);
		}
	}

	return DistinctBuildingsAndNeutrals.size();
}


HisBuilding * VMap::EnemyBuildingsAround(TilePosition center, int radius) const
{
	for (int dy = -radius ; dy <= +radius ; dy += 2)
	for (int dx = -radius ; dx <= +radius ; dx += 2)
	{
		TilePosition t = center + TilePosition(dx, dy);
		if (GetMap()->Valid(t))
		{
			const Tile & tile = GetMap()->GetTile(t, check_t::no_check);
			if (BWAPIUnit * b = GetBuildingOn(tile))
				if (HisBuilding * pHisBuilding = b->IsHisBuilding())
					return pHisBuilding;
		}
	}

	return nullptr;
}


void VMap::PrintAreaChains() const
{
#if BWEM_USE_MAP_PRINTER
	MapPrinter::Color col = MapPrinter::Color(255, 255, 0);

	set<const AreaChain *> AreaChains;
	for (const VArea & area : Areas())
		if (const AreaChain * pChain = area.IsInChain())
			AreaChains.insert(pChain);

	for (const AreaChain * pChain : AreaChains)
	{
		if (pChain->GetAreas().size() == 1)
			MapPrinter::Get().Circle(pChain->GetAreas().front()->BWEMPart()->Top(), 5, col);
		else
			for (int i = 1 ; i < (int)pChain->GetAreas().size() ; ++i)
				MapPrinter::Get().Line(pChain->GetAreas()[i-1]->BWEMPart()->Top(), pChain->GetAreas()[i]->BWEMPart()->Top(), col);

		if (pChain->IsLeaf())
			for (VArea * area : {pChain->GetAreas().front(), pChain->GetAreas().back()})
				if (area->BWEMPart()->AccessibleNeighbours().size() == 1)
				{
					MapPrinter::Get().Circle(area->BWEMPart()->Top(), 4, MapPrinter::Color(0, 0, 0), MapPrinter::fill);
					MapPrinter::Get().Circle(area->BWEMPart()->Top(), 2, col, MapPrinter::fill);
				}
	}
#endif
}


void VMap::PrintEnlargedAreas() const
{
#if BWEM_USE_MAP_PRINTER

	for (const VArea & area : Areas())
		for (const Area * ext : area.EnlargedArea())
			if (ext != area.BWEMPart())
			{
				MapPrinter::Color col = MapPrinter::Color(0, 0, 255);
				MapPrinter::Get().Line(area.BWEMPart()->Top(), ext->Top(), col, MapPrinter::dashed);
			}
#endif
}


} // namespace iron



