//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "stronghold.h"
#include "vbase.h"
#include "../units/cc.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }

namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Stronghold
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

void Stronghold::OnUnitIn(MyUnit * u)
{
	assert_throw(u->GetStronghold() == this);

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Units, u);
	
	if (auto * pSCV = u->IsMy<Terran_SCV>()) PUSH_BACK_UNCONTAINED_ELEMENT(m_SCVs, pSCV);

	OnUnitIn_v(u);
}


void Stronghold::OnBuildingIn(MyBuilding * b)
{
	assert_throw(b->GetStronghold() == this);

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Buildings, b);
	OnBuildingIn_v(b);
}


void Stronghold::OnUnitOut(MyUnit * u)
{
	assert_throw(u->GetStronghold() == this);

	OnUnitOut_v(u);

	if (auto * pSCV = u->IsMy<Terran_SCV>())
	{
		auto iSCV = find(m_SCVs.begin(), m_SCVs.end(), pSCV);
		assert_throw(iSCV != m_SCVs.end());
		fast_erase(m_SCVs, distance(m_SCVs.begin(), iSCV));
	}

	auto i = find(m_Units.begin(), m_Units.end(), u);
	assert_throw(i != m_Units.end());
	fast_erase(m_Units, distance(m_Units.begin(), i));
}


void Stronghold::OnBuildingOut(MyBuilding * b)
{
	assert_throw(b->GetStronghold() == this);

	OnBuildingOut_v(b);

	auto i = find(m_Buildings.begin(), m_Buildings.end(), b);
	assert_throw(i != m_Buildings.end());
	fast_erase(m_Buildings, distance(m_Buildings.begin(), i));
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class BaseStronghold
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

BaseStronghold::BaseStronghold(VBase * pBase)
	: m_pBase(pBase)
{
	assert_throw(pBase);
	assert_throw(!m_pBase->GetStronghold());
	m_pBase->SetStronghold(this);

}


void BaseStronghold::OnUnitIn_v(MyUnit * u)
{
	assert_throw(contains(Units(), u));

	GetBase()->OnUnitIn(u);
}


void BaseStronghold::OnBuildingIn_v(MyBuilding * b)
{
	assert_throw(contains(Buildings(), b));

	GetBase()->OnBuildingIn(b);
}


void BaseStronghold::OnUnitOut_v(MyUnit * u)
{
	assert_throw(contains(Units(), u));

	GetBase()->OnUnitOut(u);
}


void BaseStronghold::OnBuildingOut_v(MyBuilding * b)
{
	assert_throw(contains(Buildings(), b));

	GetBase()->OnBuildingOut(b);
}




} // namespace iron



