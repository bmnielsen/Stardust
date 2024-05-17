#pragma once

#include "GridWalk.h"

// The accessible room, measured in walk tiles, around each walk tile.

namespace UAlbertaBot
{
class The;

class GridRoom : public GridWalk
{
private:
	The & the;

public:
	GridRoom(The & the);

	void initialize();
	
	void draw() const;
};
}