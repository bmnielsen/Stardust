#pragma once

#include "Common.h"
#include "MyUnit.h"

// Used to store data about buildings that are either under construction or about to be constructed
class Building
{
public:
    BWAPI::UnitType type;               // The type of the building
    BWAPI::TilePosition tile;           // The position of the building
    MyUnit unit;                        // The building itself
    MyUnit builder;                     // The unit that will build the building
    int desiredStartFrame;              // The desired start frame given by the producer
    int buildCommandSuccessFrames;      // Number of frames in which a successful build command was issued
    int buildCommandFailureFrames;      // Number of frames in which a failed build command was issued
    bool noGoAreaAdded;                 // Whether the no-go area has been added

    // TODO: State required by the builder

    // Constructor
    // We always decide on the position and builder unit before storing the building
    // If something happens to invalidate them before the building is started, we will create a new building
    Building(BWAPI::UnitType type, BWAPI::TilePosition tile, MyUnit builder, int desiredStartFrame);

    void constructionStarted(MyUnit unit);

    [[nodiscard]] BWAPI::Position getPosition() const;

    [[nodiscard]] bool isConstructionStarted() const;

    [[nodiscard]] int expectedFramesUntilStarted() const;

    [[nodiscard]] int expectedFramesUntilCompletion() const;

    [[nodiscard]] bool builderReady() const;

    void addNoGoAreaWhenNeeded();

    void removeNoGoArea();

    // TODO: Stuff like handling things blocking construction, picking a new location, cancelling, etc.

    friend std::ostream &operator<<(std::ostream &out, const Building &b)
    {
        out << b.type << " @ " << b.tile;
        return out;
    };

private:
    int startFrame;
};
