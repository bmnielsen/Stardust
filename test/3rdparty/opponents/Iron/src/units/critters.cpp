//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "critters.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }

namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Critters
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


void Critters::Update()
{
	for (auto & u : Get())
		u->Update();
}


void Critters::AddUnit(BWAPI::Unit u)
{
	assert_throw(u->getInitialType().isCritter());

	assert_throw_plus(none_of(m_Units.begin(), m_Units.end(), [u](const unique_ptr<HisUnit> & up){ return up->Unit() == u; }), to_string(u->getID()));

	m_Units.push_back(make_unique<HisUnit>(u));
}


void Critters::RemoveUnit(BWAPI::Unit u)
{
	assert_throw(u->getInitialType().isCritter());
	auto iHisUnit = find_if(m_Units.begin(), m_Units.end(), [u](const unique_ptr<HisUnit> & up){ return up->Unit() == u; });
	if (iHisUnit != m_Units.end())
		fast_erase(m_Units, distance(m_Units.begin(), iHisUnit));
}



	
} // namespace iron



