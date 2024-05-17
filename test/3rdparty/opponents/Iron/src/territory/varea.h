//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef VAREA_H
#define VAREA_H

#include <BWAPI.h>
#include "../utils.h"
#include "../defs.h"


namespace iron
{


class VArea;


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class AreaChain
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class AreaChain
{
public:

	const vector<VArea *> &		GetAreas() const		{ return m_Areas; }

	bool						IsLeaf() const			{ return m_leaf; }

private:
	vector<VArea *>				m_Areas;
	bool						m_leaf = false;
	friend class VArea;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class VArea
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class VArea
{
public:
	static VArea *					Get(const BWEM::Area * pBWEMPart);

									VArea(const BWEM::Area * pBWEMPart);
									~VArea();

	void							Initialize();

	const Area		*				BWEMPart() const		{ return m_pBWEMPart; }

	const vector<const Area *> &	EnlargedArea() const	{ return m_EnlargedArea; }
	const vector<const ChokePoint *> &	EnlargedAreaChokePoints() const	{ return m_EnlargedAreaChokePoints; }

	VArea &							operator=(const VArea &) = delete;

	// if this contains a starting Base, returns the CP of EnlargedArea() that should be defended first, if ever.
	const ChokePoint *				HotCP() const			{ return m_pHotCP; }

	const ChokePoint *				DefenseCP() const		{ return m_pDefenseCP; }
	bool							UnderDefenseCP() const	{ return m_underDefenseCP; }

	Position						SentryPos() const		{ return m_sentryPos; }

	const AreaChain *				IsInChain() const		{ return m_pChain.get(); }

private:
	void							ComputeProtossWalling(const Area * area, bool zerglingTight);
	vector<TilePosition>			ComputeProtossWalling(int wallSize, const vector<TilePosition> & BuildableBorderTiles1, const vector<TilePosition> & BuildableBorderTiles2, bool zerglingTight);
	void							ComputeChain(const VArea * pFrom = nullptr);
	void							ComputeEnlargedArea();
	void							ComputeEnlargedAreaChokePoints();
	void							ComputeHotCP();
	void							ComputeDefenseCP();
	void							ComputeSentryPosition();

	const Area *					m_pBWEMPart;
	vector<const Area *>			m_EnlargedArea;
	vector<const ChokePoint *>		m_EnlargedAreaChokePoints;
	const ChokePoint *				m_pHotCP = nullptr;
	const ChokePoint *				m_pDefenseCP = nullptr;
	bool							m_underDefenseCP = false;
	Position						m_sentryPos = Positions::None;
	shared_ptr<AreaChain>			m_pChain;
};


inline VArea * ext(const BWEM::Area * area)
{
	return VArea::Get(area);
}

bool tileInEnlargedArea(const TilePosition & t, const VArea * varea, const Area * area);

bool isImpasse(const Area * area);

const Area * homeAreaToHold();

} // namespace iron


#endif

