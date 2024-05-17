//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef VCP_H
#define VCP_H

#include <BWAPI.h>
#include "wall.h"
#include "../utils.h"
#include "../defs.h"


namespace iron
{

class Wall;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class VChokePoint
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class VChokePoint
{
public:
	static VChokePoint *				Get(const BWEM::ChokePoint * pBWEMPart);

										VChokePoint(const BWEM::ChokePoint * pBWEMPart);
										~VChokePoint();

	void								Initialize();
	void								Update();


	const BWEM::ChokePoint *			BWEMPart() const		{ return m_pBWEMPart; }

	int									AirDistanceFrom(Position p) const;

	altitude_t							MaxAltitude() const			{ return m_maxAltitude; }
	int									Length() const				{ return 2 * m_maxAltitude; }

	vector<VChokePoint *>				Twins() const;

	const Wall &						GetWall() const				{ return m_Wall; }
	Wall &								GetWall()					{ return m_Wall; }


	VChokePoint &						operator=(const VChokePoint &) = delete;

private:
	const BWEM::ChokePoint *			m_pBWEMPart;
	altitude_t							m_maxAltitude;
	Wall								m_Wall;
};


inline VChokePoint * ext(const BWEM::ChokePoint * cp)
{
	return VChokePoint::Get(cp);
}


} // namespace iron


#endif

