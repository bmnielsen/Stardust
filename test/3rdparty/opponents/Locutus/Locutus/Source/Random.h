#pragma once

#include <random>

namespace Locutus
{

class Random
{
private:
	std::minstd_rand _rng;

public:
	Random();

	int index(int n);
	bool flag(double probability);

	static Random & Instance();
};

}
