/*
 *      Copyright (c) 2002 Frodo
 *      Portions Copyright (c) by the authors of ffmpeg and xvid
 *      Copyright (C) 2002-2013 Team XBMC
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

#include "Thread.h"
#include <stdlib.h>
#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <stdint.h>
#include <time.h>

#define LOG printf

unsigned int SystemClockMillis()
{
    uint64_t now_time;
    static uint64_t start_time = 0;
    static bool start_time_set = false;
    struct timespec ts = {};

    clock_gettime(CLOCK_MONOTONIC, &ts);

    now_time = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    if (!start_time_set)
    {
      start_time = now_time;
      start_time_set = true;
    }
    return (unsigned int)(now_time - start_time);
 }


CThread::CThread(const char* ThreadName)
{
  m_bStop = false;

  m_ThreadId = 0;
  m_iLastTime = 0;
  m_iLastUsage = 0;
  m_fLastUsage = 0.0f;

  m_pRunnable=NULL;

  if (ThreadName)
    m_ThreadName = ThreadName;
}

CThread::CThread(IRunnable* pRunnable, const char* ThreadName)
{
  m_bStop = false;


  m_ThreadId = 0;
  m_iLastTime = 0;
  m_iLastUsage = 0;
  m_fLastUsage = 0.0f;

  m_pRunnable=pRunnable;

  if (ThreadName)
    m_ThreadName = ThreadName;
}

CThread::~CThread()
{
  StopThread();
}

void CThread::Create(bool bAutoDelete, unsigned stacksize)
{
  if (m_ThreadId != 0)
  {
    LOG("%s - fatal error creating thread- old thread id not null", __FUNCTION__);
    exit(1);
  }
  m_iLastTime = SystemClockMillis() * 10000;
  m_iLastUsage = 0;
  m_fLastUsage = 0.0f;
  m_bStop = false;

  SpawnThread(stacksize);
}

bool CThread::IsRunning() const
{
  return m_ThreadId ? true : false;
}

THREADFUNC CThread::staticThread(void* data)
{
  CThread* pThread = (CThread*)(data);
  std::string name;
  ThreadIdentifier id;
  if (!pThread) {
    LOG("%s, sanity failed. thread is NULL.",__FUNCTION__);
    return 1;
  }

  name = pThread->m_ThreadName;
  id = pThread->m_ThreadId;

  LOG("Thread %s start", name.c_str());

  pThread->Action();

  pThread->m_ThreadId = 0;

  LOG("Thread %s %llu terminating", name.c_str(), (uint64_t)id);

  return 0;
}


void CThread::StopThread(bool bWait /*= true*/)
{
  m_bStop = true;
}

ThreadIdentifier CThread::ThreadId() const
{
  return m_ThreadId;
}

void CThread::Process()
{
  if(m_pRunnable)
    m_pRunnable->Run();
}


void CThread::Sleep(unsigned int milliseconds)
{
   ThreadSleep(milliseconds);
}

void CThread::Action()
{
  OnStartup();

  Process();

  OnExit();
}


void CThread::SpawnThread(unsigned stacksize)
{
  pthread_attr_t attr;
  pthread_attr_init(&attr);

  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  if (pthread_create(&m_ThreadId, &attr, (void*(*)(void*))staticThread, this) != 0)
  {
    LOG("%s - fatal error creating thread",__FUNCTION__);
  }
  pthread_attr_destroy(&attr);
}


