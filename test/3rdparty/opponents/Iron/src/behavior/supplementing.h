//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef SUPPLEMENTING_H
#define SUPPLEMENTING_H

#include <BWAPI.h>
#include "behavior.h"
#include "../utils.h"


namespace iron
{

class VBase;
FORWARD_DECLARE_MY(Terran_SCV)

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Supplementing
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//
//	Mineral::Data() is used to store the number of miners assigned to this Mineral.
//

class Supplementing : public Behavior<My<Terran_SCV>>
{
public:
	static const vector<Supplementing *> &	Instances()					{ return m_Instances; }

	enum state_t {supplementing};

								Supplementing(My<Terran_SCV> * pSCV);
								~Supplementing();

	const Supplementing *		IsSupplementing() const override	{ return this; }
	Supplementing *				IsSupplementing() override			{ return this; }

	string						Name() const override				{ return "Supplementing"; }
	string						StateName() const override			{ return "-"; }

	BWAPI::Color				GetColor() const override			{ return Colors::Grey; }
	Text::Enum					GetTextColor() const override		{ return Text::Grey; }

	void						OnFrame_v() override;

	bool						CanRepair(const MyBWAPIUnit * pTarget, int distance) const override;
	bool						CanChase(const HisUnit * pTarget) const override;

	VBase *						GetBase() const						{CI(this); return m_pBase; }

private:
	VBase *						m_pBase;

	static vector<Supplementing *>	m_Instances;
};



} // namespace iron


#endif

