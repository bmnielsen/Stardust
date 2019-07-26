#include "The.h"

using namespace UAlbertaBot;

// NOTE This object is created before BWAPI is initialized,
//      so initialization is in initialize().
The::The()
    : vWalkRoom(GridRoom(*this))
    , tileRoom(GridTileRoom(*this))
    , inset(GridInset(*this))
    , zone(GridZone(*this))
    , micro(Micro(*this))
    , ops(OpsBoss(*this))
{
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

void The::initialize()
{
	// The order of initialization is important because of dependencies.
	partitions.initialize();
	inset.initialize();				// depends on partitions
	vWalkRoom.initialize();			// depends on edgeRange
	tileRoom.initialize();			// depends on vWalkRoom
	zone.initialize();				// depends on tileRoom
	//regions.initialize();			// depends on everything before

	ops.initialize();
}

The & The::Root()
{
	static The root;
	return root;
};
