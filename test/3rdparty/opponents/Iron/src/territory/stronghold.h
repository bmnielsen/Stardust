//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef STRONGHOLD_H
#define STRONGHOLD_H

#include <BWAPI.h>
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class VBase;
class MyUnit;
class MyBuilding;

FORWARD_DECLARE_MY(Terran_SCV)

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Stronghold
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Stronghold
{
public:
	virtual								~Stronghold(){}

	virtual VBase *						HasBase() const		{ return nullptr; }

	void								OnUnitIn(MyUnit * u);
	void								OnBuildingIn(MyBuilding * b);

	void								OnUnitOut(MyUnit * u);
	void								OnBuildingOut(MyBuilding * b);

	const vector<MyUnit *> &			Units() const		{ return m_Units; }
	const vector<MyBuilding *> &		Buildings() const	{ return m_Buildings; }

	const vector<My<Terran_SCV> *> &	SCVs() const		{ return m_SCVs; }

protected:
										Stronghold(){}
private:
	virtual void						OnUnitIn_v(MyUnit * u) = 0;
	virtual void						OnBuildingIn_v(MyBuilding * b) = 0;

	virtual void						OnUnitOut_v(MyUnit * u) = 0;
	virtual void						OnBuildingOut_v(MyBuilding * b) = 0;

	vector<MyUnit *>					m_Units;
	vector<MyBuilding *>				m_Buildings;
	vector<My<Terran_SCV> *>			m_SCVs;
};




//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class BaseStronghold
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class BaseStronghold : public Stronghold
{
public:
										BaseStronghold(VBase * pBase);

	VBase *								HasBase() const override		{ return m_pBase; }
	VBase *								GetBase() const					{ return m_pBase; }

private:
	void								OnUnitIn_v(MyUnit * u) override;
	void								OnBuildingIn_v(MyBuilding * b) override;

	void								OnUnitOut_v(MyUnit * u) override;
	void								OnBuildingOut_v(MyBuilding * b) override;

	VBase *								m_pBase;
};



} // namespace iron


#endif

