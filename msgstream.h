// ////////////////////////////////////////////////////////////////////////////
// msgstream.h


#ifndef __msgstream_h__
#define __msgstream_h__


#include "misc.h"

#include "multithread.h"

#include <iostream>


// ////////////////////////////////////////////////////////////////////////////
// msgstream

/** msgstream.

    <p>Before writing to omsgstream, you must acquire lock by calling
    acquire().  Then after completion of writing, you must call
    release().</p>

    <p>Omsgbuf calls PostMessage(hwnd, messageid, 0, (LPARAM)omsgbuf)
    to notify that string is ready to get .  When the window (hwnd)
    get the message, you can get the string containd in the omsgbuf by
    calling acquireString().  After calling acquireString(), you must
    / call releaseString().</p>
*/
class omsgbuf : public std::streambuf, public SyncObject
{
  HWND hwnd;		/// window handle and messageid for notification
  UINT messageid;	///
  
  /** @name ANONYMOUS */
  enum {
    lengthof_buf = 1024,///
  };
  char *buf;		/// for streambuf
  std::string str;	/// for notification
  
  CriticalSection cs;	/// lock

  /** debug level.
      if ( msgDebugLevel <= debugLevel ), message is displayed
  */
  int debugLevel;
  int msgDebugLevel;	///
  
public:
  ///
  omsgbuf(UINT msgid_, HWND hwnd_ = NULL);
  ///
  ~omsgbuf();
  
  /// attach/detach a window
  omsgbuf* attach(HWND hwnd_);
  ///
  omsgbuf* detach();
  
  /// get window handle
  HWND getHwnd() const { return hwnd; }
  
  /// is a window attached ?
  int is_open() const { return !!hwnd; }
  
  /// acquire string and release the string
  const std::string &acquireString();
  ///
  void releaseString();

  /// set debug level
  void setDebugLevel(int debugLevel_) { debugLevel = debugLevel_; }
  ///
  int getDebugLevel() const { return debugLevel; }
  
  /// for stream
  virtual int overflow(int c = EOF);
  ///
  virtual int sync();
  
  // sync object
  /// begin writing
  virtual void acquire();
  /// begin writing
  virtual void acquire(int msgDebugLevel_);
  /// end writing
  virtual void release();
};


///
class omsgstream : public std::ostream, public SyncObject
{
public:
  ///
  omsgstream(UINT messageid, HWND hwnd = NULL);
  ///
  ~omsgstream() { }
  
  /// get the associated omsgbuf
  omsgbuf* rdbuf() const
  { return (omsgbuf *)((std::ios *)this)->rdbuf(); }

  /// attach a msg control
  void attach(HWND hwnd) { rdbuf()->attach(hwnd); }

  /// detach a msg control
  void detach() { rdbuf()->detach(); }

  /// get window handle of the msg control
  HWND getHwnd() const { return rdbuf()->getHwnd(); }

  /// is the msg control attached ?
  int is_open() const { return rdbuf()->is_open(); }

  /// set debug level
  void setDebugLevel(int debugLevel) { rdbuf()->setDebugLevel(debugLevel); }
  ///
  int getDebugLevel() const { return rdbuf()->getDebugLevel(); }

  /// acquire string and release the string
  const std::string &acquireString() { return rdbuf()->acquireString(); }
  ///
  void releaseString() { rdbuf()->releaseString(); }

  // sync object
  /// begin writing
  virtual void acquire() { rdbuf()->acquire(); }
  /// begin writing
  virtual void acquire(int msgDebugLevel) { rdbuf()->acquire(msgDebugLevel); }
  /// end writing
  virtual void release() { rdbuf()->release(); }
};


#endif // __msgstream_h__
