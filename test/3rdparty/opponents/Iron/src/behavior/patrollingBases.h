//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef PATROLLING_BASES_H
#define PATROLLING_BASES_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class VBase;
FORWARD_DECLARE_MY(Terran_Vulture)


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class PatrollingBases
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class PatrollingBases : public Behavior<My<Terran_Vulture>>
{
public:
	static const vector<PatrollingBases *> &	Instances()				{ return m_Instances; }
	static int						InstancesCreated()					{ return m_instancesCreated; }

	static vector<const VBase *>	Targets();
	static PatrollingBases *		GetPatroller();

									PatrollingBases(My<Terran_Vulture> * pAgent);
									~PatrollingBases();

	const PatrollingBases *			IsPatrollingBases() const override	{ return this; }
	PatrollingBases *				IsPatrollingBases() override		{ return this; }

	string							Name() const override				{ return "patrolling bases"; }
	string							StateName() const override			{ return "-"; }


	BWAPI::Color					GetColor() const override			{ return Colors::Purple; }
	Text::Enum						GetTextColor() const override		{ return Text::Purple; }

	void							OnFrame_v() override;

	bool							CanRepair(const MyBWAPIUnit * , int) const override	{ return true; }
	bool							CanChase(const HisUnit * ) const override			{ return true; }

	const VBase *					Target() const						{CI(this); return m_pTarget; }

private:
	const VBase *					FindFirstTarget() const;
	const VBase *					FindNextTarget() const;

	vector<const VBase *>			m_Visited;
	const VBase *					m_pTarget;

	frame_t							m_lastMinePlacement = 0;

	static vector<PatrollingBases *>m_Instances;
	static int						m_instancesCreated;
};



} // namespace iron


#endif

