//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "utils.h"
#include <fstream>


void fileCopy(const string & dest, const string & src)
{
	ifstream  in(src, std::ios::binary);
    ofstream  out(dest, std::ios::binary);

    out << in.rdbuf();
}