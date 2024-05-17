//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef FREE_TURRETS_H
#define FREE_TURRETS_H

#include <BWAPI.h>
#include "strat.h"
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
FORWARD_DECLARE_MY(Terran_SCV)
	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class FreeTurrets
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class FreeTurrets : public Strat
{
public:
									FreeTurrets();

	string							Name() const override { return "FreeTurrets"; }
	string							StateDescription() const override;

	bool							Active() const		{ return m_active; }

	int								Priority() const		{ return m_priority; }
	TilePosition					NextLocation() const	{ return m_nextLocation; }
	My<Terran_SCV> *				Builder() const			{ return m_pBuilder; }

	void							SetNeedTurrets()			{ m_needTurrets = true; }
	void							SetNeedManyTurrets(bool val){ m_needManyTurrets = val; }
	void							SetNeedManyManyTurrets(bool val)	{ m_needManyManyTurrets = val; }
	bool							NeedManyTurrets() const		{ return m_needManyTurrets; }
	bool							NeedManyManyTurrets() const	{ return m_needManyManyTurrets; }

private:
	void							OnFrame_v() override;
	void							CheckNewConstruction();

	bool							m_active = false;
	frame_t							m_activeSince;
	TilePosition					m_nextLocation = TilePositions::None;
	int								m_priority;
	My<Terran_SCV> *				m_pBuilder;
	frame_t							m_lastTimeSucceed = 0;
	bool							m_needTurrets = false;
	bool							m_needManyTurrets = false;
	bool							m_needManyManyTurrets = false;
};


} // namespace iron


#endif

