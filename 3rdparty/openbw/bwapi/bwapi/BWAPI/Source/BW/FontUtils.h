#include "font8x8.h"
typedef unsigned char uint8_t;

/*
 * Font utils for OpenBW BWAPI
 */
namespace BW {
  namespace FontUtils {
    inline int getCharWidth(uint8_t c, uint8_t bSize) {
      if(c=='\t') return 12*bSize;
      if(c==' ') return 6*bSize;
      return 6*bSize;
    }

    inline int maxHeight(uint8_t bSize) {
      return 10*bSize;
    }

    inline int maxWidth(uint8_t bSize) {
      return 6*bSize;
    }

    inline int getTextWidth(const char *pszStr, uint8_t bSize) {
      // Retrieve size
      int size = 0;
      for ( ; *pszStr; ++pszStr )
        size += getCharWidth(*pszStr, bSize);

      return size;
    }

    inline int getTextHeight(const char *pszStr, uint8_t bSize) {
      uint8_t Ymax = maxHeight(bSize);
      int size = Ymax;
      for ( ; *pszStr; ++pszStr )
      {
        if ( *pszStr == '\n' )
          size += Ymax;
      }
      return size;
    }

    namespace Char {

      int isValid(uint8_t ch, uint8_t bSize) {
        return true;
      }

      inline int height(uint8_t ch, uint8_t bSize) {
        return 8*bSize;
      }

      inline int width(uint8_t ch, uint8_t bSize) {
        return 6*bSize;
      }

      int x(uint8_t ch, uint8_t bSize) {
        return 0;
      }

      int y(uint8_t ch, uint8_t bSize) {
        return 0;
      }

      inline int pixelOffset(uint8_t ch, uint8_t bSize, int i) {
        return 0;
      }

      int getColor(uint8_t ch, uint8_t bSize, int i) {
        int y = i/(6*bSize)/bSize;
        int x = (i%(6*bSize))/bSize;
        return Font8x8GetColor(ch, x, y);
      }
    }
  }
}
