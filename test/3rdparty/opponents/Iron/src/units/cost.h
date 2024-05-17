//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef COST_H
#define COST_H

#include <BWAPI.h>
#include "../defs.h"


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Cost
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

class Cost
{
public:
						Cost() : m_minerals(0), m_gas(0), m_supply(0) {}

						Cost(UnitType type)
							: m_minerals(type.mineralPrice()),
							  m_gas(type.gasPrice()),
							  m_supply(type.supplyRequired() / 2) {}

						Cost(UnitTypes::Enum::Enum type)
							: Cost(UnitType(type)) {}

						Cost(TechType techType)
							: m_minerals(techType.mineralPrice()),
							  m_gas(techType.gasPrice()),
							  m_energy(techType.energyCost()) {}

						Cost(UpgradeType upgradeType)
							: m_minerals(upgradeType.mineralPrice()),
							  m_gas(upgradeType.gasPrice()) {}

	int					Minerals() const	{ return m_minerals; }
	int					Gas() const			{ return m_gas; }
	int					Supply() const		{ return m_supply; }
	int					Energy() const		{ return m_energy; }

	bool				Nul() const			{ return (m_minerals | m_gas | m_supply) == 0; }

private:
	int					m_minerals;
	int					m_gas;
	int					m_supply = 0;
	int					m_energy = 0;
};





} // namespace iron


#endif

