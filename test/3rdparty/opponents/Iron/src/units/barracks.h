//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef BARRACKS_H
#define BARRACKS_H

#include "my.h"


namespace iron
{



template<tid_t> class My;



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Barracks>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

template<>
class My<Terran_Barracks> : public MyBuilding
{
public:
	static ConstructingExpert *	GetConstructingExpert();

							My(BWAPI::Unit u);

private:
	void					DefaultBehaviorOnFrame() override;

	static ExpertInConstructing<Terran_Barracks>	m_ConstructingExpert;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Marine>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


template<>
class My<Terran_Marine> : public MyUnit
{
public:
							My(BWAPI::Unit u);

private:
	void					DefaultBehaviorOnFrame() override;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Medic>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


template<>
class My<Terran_Medic> : public MyUnit
{
public:
							My(BWAPI::Unit u);

	void					Heal(MyBWAPIUnit * target, check_t checkMode = check_t::check);

private:
	void					DefaultBehaviorOnFrame() override;
};



} // namespace iron


#endif

