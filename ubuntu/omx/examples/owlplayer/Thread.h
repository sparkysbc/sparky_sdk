/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

// Thread.h: interface for the CThread class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>


class IRunnable
{
public:
  virtual void Run()=0;
  virtual ~IRunnable() {}
};

// minimum as mandated by XTL
#define THREAD_MINSTACKSIZE 0x10000


struct threadOpaque
{
  pid_t LwpId;
};

typedef pthread_t ThreadIdentifier;
typedef threadOpaque ThreadOpaque;
typedef int THREADFUNC;


inline static void ThreadSleep(unsigned int millis) { usleep(millis*1000); }


class CThread
{

protected:
  CThread(const char* ThreadName);

public:
  CThread(IRunnable* pRunnable, const char* ThreadName);
  virtual ~CThread();
  void Create(bool bAutoDelete = false, unsigned stacksize = 0);
  void Sleep(unsigned int milliseconds);
  virtual void StopThread(bool bWait = true);
  bool IsRunning() const;

protected:
  virtual void OnStartup(){};
  virtual void OnExit(){};
  virtual void Process();

  volatile bool m_bStop;

private:
  static THREADFUNC staticThread(void *data);
  void Action();

  // -----------------------------------------------------------------------------------
  // These are platform specific and can be found in ./platform/[platform]/ThreadImpl.cpp
  // -----------------------------------------------------------------------------------
  ThreadIdentifier ThreadId() const;
  void SpawnThread(unsigned stacksize);
  // -----------------------------------------------------------------------------------

  ThreadIdentifier m_ThreadId;
  ThreadOpaque m_ThreadOpaque;
  IRunnable* m_pRunnable;
  uint64_t m_iLastUsage;
  uint64_t m_iLastTime;
  float m_fLastUsage;

  std::string m_ThreadName;
};
