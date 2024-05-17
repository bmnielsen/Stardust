//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef FACTORY_H
#define FACTORY_H

#include "my.h"


namespace iron
{



template<tid_t> class My;



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Factory>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

template<>
class My<Terran_Factory> : public MyBuilding
{
public:
	static ConstructingExpert *	GetConstructingExpert();

							My(BWAPI::Unit u);

	vector<ConstructingAddonExpert *>	ConstructingAddonExperts() override {CI(this); return m_ConstructingAddonExperts; }

private:
	void					DefaultBehaviorOnFrame() override;

	static ExpertInConstructing<Terran_Factory>	m_ConstructingExpert;


	static vector<ConstructingAddonExpert *>	m_ConstructingAddonExperts;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Vulture>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


template<>
class My<Terran_Vulture> : public MyUnit
{
public:
							My(BWAPI::Unit u);

	void					PlaceMine(Position pos, check_t checkMode = check_t::check);
	int						RemainingMines() const	{CI(this); return Unit()->getSpiderMineCount(); }

	bool					WorthBeingRepaired() const override;
	
	void					SetRetraitAndLayMines()	{CI(this); m_shouldRetraitAndLayMines = true; }

private:
	void					DefaultBehaviorOnFrame() override;

	bool					m_shouldRetraitAndLayMines = false;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Siege_Tank_Tank_Mode>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


template<>
class My<Terran_Siege_Tank_Tank_Mode> : public MyUnit
{
public:
							My(BWAPI::Unit u);

	void					Siege(check_t checkMode = check_t::check);
	void					Unsiege(check_t checkMode = check_t::check);

	int						MaxRepairers() const override						{ return Is(Terran_Siege_Tank_Tank_Mode) ? 2 : 5; }
	int						CanSiegeAttack(const BWAPIUnit * u) const;

private:
	void					DefaultBehaviorOnFrame() override;
};


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Vulture_Spider_Mine>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


template<>
class My<Terran_Vulture_Spider_Mine> : public MyUnit
{
public:
							My(BWAPI::Unit u);
							~My();

	bool					Ready() const			{CI(this); return m_ready; }

private:
	void					DefaultBehaviorOnFrame() override;

	bool					m_ready = false;
	bool					m_dangerous = false;
	const FaceOff *			m_pTarget = nullptr;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Goliath>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


template<>
class My<Terran_Goliath> : public MyUnit
{
public:
							My(BWAPI::Unit u);

	int						MaxRepairers() const override						{ return 2; }

private:
	void					DefaultBehaviorOnFrame() override;
};



} // namespace iron


#endif

