//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef PRODUCTION_H
#define PRODUCTION_H

#include <BWAPI.h>
#include "../defs.h"


namespace iron
{

class Expert;

double gatheringRatio();

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Production
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Production
{
public:
									Production();

	void							OnFrame();
	void							Record(Expert * pExpert);
	void							Remove(Expert * pExpert);

	int								MineralsReserved() const		{ return m_mineralsReserved; }
	int								GasReserved() const				{ return m_gasReserved; }
	int								SupplyReserved() const			{ return m_supplyReserved; }

	int								MineralsAvailable() const;
	int								GasAvailable() const;
	int								SupplyAvailable() const;

	double							AvailableRatio()				{ return MineralsAvailable() / (double)GasAvailable(); }

	void							Unselect(Expert * e);

	const vector<Expert *>			Experts() const					{ return m_Experts; }


private:
	vector<Expert *>				m_Experts;

	void							Select(Expert * e);

	int								m_mineralsReserved = 0;
	int								m_gasReserved = 0;
	int								m_supplyReserved = 0;

	frame_t							m_lastTimePositiveSupply = 0;
};



} // namespace iron


#endif

