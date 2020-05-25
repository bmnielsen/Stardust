//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef EBAY_H
#define EBAY_H

#include "my.h"


namespace iron
{



template<tid_t> class My;



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Engineering_Bay>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

template<>
class My<Terran_Engineering_Bay> : public MyBuilding
{
public:
	static ConstructingExpert *	GetConstructingExpert();

							My(BWAPI::Unit u);

private:
	void					DefaultBehaviorOnFrame() override;

	static ExpertInConstructing<Terran_Engineering_Bay>	m_ConstructingExpert;

};



} // namespace iron


#endif

