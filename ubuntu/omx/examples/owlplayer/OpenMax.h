#pragma once
/*
 *      Copyright (C) 2010-2013 Team XBMC
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
#ifndef _OPEN_MAX_H__
#define _OPEN_MAX_H__

#include <semaphore.h>
#include <OMX_Core.h>
#include <string>


// check for potentially undefined OpenMAX version numbers
#ifndef OMX_VERSION_MAJOR
#define OMX_VERSION_MAJOR 1
#endif
#ifndef OMX_VERSION_MINOR
#define OMX_VERSION_MINOR 1
#endif
#ifndef OMX_VERSION_REVISION
#define OMX_VERSION_REVISION 2
#endif
#ifndef OMX_VERSION_STEP
#define OMX_VERSION_STEP 0
#endif

// debug spew defines
#if 0
#define OMX_DEBUG_VERBOSE
#define OMX_DEBUG_EVENTHANDLER
#define OMX_DEBUG_FILLBUFFERDONE
#define OMX_DEBUG_EMPTYBUFFERDONE
#endif

#define OMX_INIT_STRUCTURE(a) \
  memset(&(a), 0, sizeof(a)); \
  (a).nSize = sizeof(a); \
  (a).nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
  (a).nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
  (a).nVersion.s.nRevision = OMX_VERSION_REVISION; \
  (a).nVersion.s.nStep = OMX_VERSION_STEP


typedef struct omx_demux_packet {
  OMX_U8 *buff;
  int size;
  double dts;
  double pts;
} omx_demux_packet;


enum OMX_CLIENT_STATE {
	DEAD,
	LOADED,
	LOADED_TO_IDLE,
	IDLE_TO_EXECUTING,
	EXECUTING,
	EXECUTING_TO_IDLE,
	IDLE_TO_LOADED,
	RECONFIGURING,
	ERROR
};

typedef OMX_ERRORTYPE  (*OMX_Init_Func)(void);	 
typedef OMX_ERRORTYPE (*OMX_Deinit_Func)(void);	  
typedef OMX_ERRORTYPE  (*OMX_GetHandle_Func)(OMX_HANDLETYPE *pHandle, OMX_STRING cComponentName, OMX_PTR pAppData, OMX_CALLBACKTYPE *pCallBacks);   
typedef OMX_ERRORTYPE (*OMX_FreeHandle_Func)(OMX_HANDLETYPE hComponent); 

struct OMXLIB{
	void *libFd;
	OMX_Init_Func OMX_Init;    
	OMX_Deinit_Func OMX_Deinit;    
	OMX_GetHandle_Func OMX_GetHandle;   
	OMX_FreeHandle_Func OMX_FreeHandle;    
};

typedef struct OMXLIB OMX_LIB;

class COpenMax
{
public:
  COpenMax();
  virtual ~COpenMax();

protected:

  int load(void);
  void unload(void);

  // initialize OpenMax and get decoder component
  bool Initialize( const std::string &decoder_name);
  void Deinitialize();

  // OpenMax Decoder delegate callback routines.
  static OMX_ERRORTYPE DecoderEventHandlerCallback(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
    OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2, OMX_PTR pEventData);
  static OMX_ERRORTYPE DecoderEmptyBufferDoneCallback(
    OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer);
  static OMX_ERRORTYPE DecoderFillBufferDoneCallback(
    OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBufferHeader);

  // OpenMax decoder callback routines.
  virtual OMX_ERRORTYPE DecoderEventHandler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
    OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2, OMX_PTR pEventData);
  virtual OMX_ERRORTYPE DecoderEmptyBufferDone(
    OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer);
  virtual OMX_ERRORTYPE DecoderFillBufferDone(
    OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBufferHeader);

  // OpenMax helper routines
  OMX_ERRORTYPE WaitForState(OMX_STATETYPE state);
  OMX_ERRORTYPE SetStateForComponent(OMX_STATETYPE state);

  OMX_LIB* m_dll;
  bool              m_is_open;
  OMX_HANDLETYPE    m_omx_decoder;   // openmax decoder component reference

  // OpenMax state tracking
  OMX_CLIENT_STATE  m_omx_client_state;
  volatile int      m_omx_decoder_state;
  sem_t             m_omx_decoder_state_change;

private:
  COpenMax(const COpenMax& other);
  COpenMax& operator=(const COpenMax&);
};

#endif
