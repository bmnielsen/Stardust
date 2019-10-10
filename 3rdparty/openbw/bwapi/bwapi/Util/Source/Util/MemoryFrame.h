#pragma once
////////////////////////////////////////////////////////////////////////
//  Boundaries of a non typed memory block                            //
//                                                                    //
////////////////////////////////////////////////////////////////////////

namespace Util
{
  class MemoryFrame;
}

#include <Util/Exceptions.h>

#ifndef __FUNCTION__
#define __FUNCTION__ "UNKNOWN FUNCTION"
#endif

namespace Util
{
  class MemoryFrame
  {
  public:
    MemoryFrame(void *base, unsigned int size);
    MemoryFrame();
    ~MemoryFrame();

    void fill(unsigned char value);

    MemoryFrame getSubFrame(int from, int size);
    MemoryFrame getSubFrameByLimits(int from, int to);
    MemoryFrame getFrameUpto(const MemoryFrame &upto);
    void *begin() const;
    void *end() const;
    unsigned int size() const;
    bool isInside(void *ptr) const;
    bool isEmpty() const;
    bool isMultipleOf(unsigned int bytes) const;
    template<typename T>
      bool isFitFor() const
      {
        return this->frameSize >= sizeof(T);
      }
    template<typename T>
      bool isMultipleOf() const
      {
        return this->isMultipleOf(sizeof(T));
      }
    bool operator == (const MemoryFrame &) const;           // compares the frame addresses
    bool compareBytes(const MemoryFrame &) const;

    void skip(unsigned int bytes);
    template<typename T>
      void skip()
      {
        this->skip(sizeof(T));
      }
    MemoryFrame read(int bytes);
    void write(const MemoryFrame &source);
    template<typename T>
      T& readAs()
      {
        if(!this->isFitFor<T>())
          throw GeneralException(__FUNCTION__ ": memory too small");
        T& retval = getAs<T>();
        skip<T>();
        return retval;
      }
    template<typename T>
      void readTo(T &destination)
      {
        if(!this->tryReadTo(destination))
        {
          throw GeneralException(__FUNCTION__ ": memory too small");
        }
      }
    template<typename T>
      bool tryReadTo(T &destination) throw()
      {
        if(!this->isFitFor<T>())
          return false;
        destination = getAs<T>();
        skip<T>();
        return true;
      }
    template<typename T>
      bool writeAs(const T& storee)
      {
        if(!this->isFitFor<T>())
          return false;
        this->getAs<T>() = storee;
        skip<T>();
        return true;
      }

    // interpreter functions
    template<typename T>
      T &getAs()
      {
        return *((T*)frameBase);
      }
    template<typename T>
      T &offsetAs(int offset)
      {
        return *((T*)((int)frameBase+offset));
      }
    template<typename T>
      T *offset(int offset = 0)
      {
        return ((T*)((int)frameBase+offset));
      }
    template<typename T>
      T *beginAs()
      {
        return (T*)frameBase;
      }
    template<typename T>
      T *endAs()  // the array filling as much as possible
      {
        return (T*)((int)frameBase+(frameSize/sizeof(T))*sizeof(T));
      }
    template<typename T>
      unsigned int sizeAs() const
      {
        return frameSize/sizeof(T);
      }
    template<typename T>
      static MemoryFrame from(T &structure)
      {
        return MemoryFrame((void*)&structure, sizeof(T));
      }

  private:
    void *frameBase;
    unsigned int frameSize;

    static int _limit(int a, int low, int hi);
  };
}
