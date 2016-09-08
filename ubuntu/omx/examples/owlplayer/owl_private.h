#ifndef OWL_PRIVATE_H
#define OWL_PRIVATE_H


#include <assert.h>

#define ACTIONS_VIDEO_DECODER      "OMX.Action.Video.Decoder"


#ifndef __func__
#define __func__ __FUNCTION__
#endif


template<typename T> struct IResourceCounted
{
  IResourceCounted() : m_refs(1) {}
  virtual ~IResourceCounted() {}
  virtual T*   Acquire()
  {
    ++m_refs;
    return (T*)this;
  }

  virtual long Release()
  {
    --m_refs;
    long count = m_refs;
    assert(count >= 0);
    if (count == 0) delete (T*)this;
    return count;
  }
  long m_refs;
};



#endif
