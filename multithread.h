///////////////////////////////////////////////////////////////////////////////
// multithread.h


#ifndef __multithread_h__
#define __multithread_h__


#include <windows.h>


class SyncObject
{
public:
  virtual void acquire() = 0;
  virtual void acquire(int ) { acquire(); }
  virtual void release() = 0;
};


class CriticalSection : public SyncObject
{
  CRITICAL_SECTION cs;
public:
  CriticalSection() { InitializeCriticalSection(&cs); }
  ~CriticalSection() { DeleteCriticalSection(&cs); }
  void acquire() { EnterCriticalSection(&cs); }
  void release() { LeaveCriticalSection(&cs); }
};


class Acquire
{
  SyncObject *so;
public:
  Acquire(SyncObject *so_) : so(so_) { so->acquire(); }
  Acquire(SyncObject *so_, int n) : so(so_) { so->acquire(n); }
  ~Acquire() { so->release(); }
};


#endif // __multithread_h__
