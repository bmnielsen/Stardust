#pragma once

namespace BWAPI
{
  class TilePosition;

  // TODO: Add doxygen documentation
  class Position
  {
    public :
      Position();
      explicit Position(const TilePosition& position);
      Position(int x, int y);
      bool operator == (const Position& position) const;
      bool operator != (const Position& position) const;
      bool operator  < (const Position& position) const;
      bool isValid() const;
      Position operator+(const Position& position) const;
      Position operator-(const Position& position) const;
      Position& makeValid();
      Position& operator+=(const Position& position);
      Position& operator-=(const Position& position);
      double getDistance(const Position& position) const;
      double getApproxDistance(const Position& position) const;
      double getLength() const;
      int& x();
      int& y();
      int x() const;
      int y() const;
    private :
      int _x;
      int _y;
  };
  namespace Positions
  {
    extern const Position Invalid;
    extern const Position None;
    extern const Position Unknown;
  }
};

