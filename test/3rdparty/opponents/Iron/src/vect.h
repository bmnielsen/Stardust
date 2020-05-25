//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef VECT_H
#define VECT_H


#include "myassert.h"




using namespace std;



struct Vect
{
						Vect() : x(0.0), y(0.0) {}
						Vect(double x, double y) : x(x), y(y) {}

	double				Norm() const					{ return sqrt(x*x + y*y); }
	void				Normalize()						{ auto d = Norm(); x /= d; y /= d; }
	Vect &				operator+=(const Vect & a)		{ x += a.x; y += a.y; return *this; }
	Vect &				operator-=(const Vect & a)		{ return *this += -a; }
	Vect				operator-() const				{ return Vect(-x, -y); }
	Vect &				operator*=(double k)			{ x *= k; y *= k; return *this; }
	Vect &				operator/=(double k)			{ return *this *= 1/k; }

	double	x;
	double	y;
};


inline Vect operator+(const Vect & a, const Vect & b)		{ Vect r(a); r += b; return r; }
inline Vect operator-(const Vect & a, const Vect & b)		{ Vect r(a); r -= b; return r; }
inline Vect operator*(const Vect & a, double k)				{ Vect r(a); r *= k; return r; }
inline Vect operator*(double k, const Vect & a)				{ Vect r(a); r *= k; return r; }
inline Vect operator/(const Vect & a, double k)				{ Vect r(a); r /= k; return r; }

inline double operator*(const Vect & a, const Vect & b)		{ return a.x*b.x + a.y*b.y; }

Vect rot(const Vect & a, double angle);

double angle(const Vect & from, const Vect & to);


#endif

