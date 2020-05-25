//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "vcp.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }

namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class VChokePoint
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


VChokePoint * VChokePoint::Get(const BWEM::ChokePoint * pBWEMPart)
{
	return static_cast<VChokePoint *>(pBWEMPart->Ext());
}


VChokePoint::VChokePoint(const BWEM::ChokePoint * pBWEMPart)
	: m_pBWEMPart(pBWEMPart), m_Wall(pBWEMPart)
{
	assert_throw(!ext(m_pBWEMPart));
	m_pBWEMPart->SetExt(this);

	m_maxAltitude = ai()->GetMap().GetMiniTile(pBWEMPart->Center()).Altitude();

}


VChokePoint::~VChokePoint()
{
	m_pBWEMPart->SetExt(nullptr);

}


void VChokePoint::Initialize()
{
	if (!m_pBWEMPart->Blocked()) m_Wall.Compute();
}


void VChokePoint::Update()
{
	m_Wall.Update();
}


int VChokePoint::AirDistanceFrom(Position p) const
{
	int minDist = 0;
	for (auto node : {ChokePoint::end1, ChokePoint::middle, ChokePoint::end2})
		minDist = min(minDist, p.getApproxDistance(Position(BWEMPart()->Pos(node))));

	return minDist;
}


vector<VChokePoint *> VChokePoint::Twins() const
{
	vector<VChokePoint *> Res;

	auto areas = BWEMPart()->GetAreas();
	for (const ChokePoint & cp : *areas.first->ChokePointsByArea().find(areas.second)->second)
		if (&cp != BWEMPart())
			if (!cp.Blocked())
				Res.push_back(ext(&cp));

	return Res;
}

	
} // namespace iron



