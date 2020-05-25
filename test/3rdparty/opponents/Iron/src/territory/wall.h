//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef WALL_H
#define WALL_H

#include <BWAPI.h>
#include "../utils.h"
#include "../defs.h"


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Wall
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Wall : public ExampleWall
{
public:
								Wall(const ChokePoint * cp);


	void						Update();
	void						Compute();
	void						DrawLocations() const;

	bool						Completed() const;

	int							DistanceTo(Position pos) const;
	int							DistToClosestGroundEnemy() const{ return m_distToClosestGroundEnemy;; }
	bool						UnderWall(Position pos) const;

	const Area *				InnerArea() const				{ return m_innerArea; }
	const Area *				OuterArea() const				{ return m_outerArea; }

	bool						Open() const;
	bool						OpenRequest() const				{ return m_openRequest; }
	bool						CloseRequest() const			{ return m_closeRequest; }

private:
	void						MapSpecific();
	void						MapSpecificApply(int xCP, int yCP, int xBarrack, int yBarrack, int xDepot1 = -1, int yDepot1 = -1, int xDepot2 = -1, int yDepot2 = -1);
	void						MapSpecificRemove(int xCP, int yCP);

	const Area *				m_innerArea;
	const Area *				m_outerArea;
	int							m_distToClosestGroundEnemy;
	bool						m_openRequest;
	bool						m_closeRequest;
	frame_t						m_lastTimeEnemyNearby = 0;
};



} // namespace iron


#endif

