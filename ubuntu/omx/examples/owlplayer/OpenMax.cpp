extern "C"{
#include <stdlib.h>
#include <memory.h>
#include <malloc.h>
#include <dlfcn.h>
#include <errno.h>
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Index.h>
#include <OMX_Image.h>
}
#include "log.h"

#include "OpenMax.h"

#define CLASSNAME "OpenMax"


#define lib "libOMX_Core.so"


int COpenMax:: load(void)
{   
	logw("%s::%s %d\n", CLASSNAME, __func__, __LINE__);

	m_dll->libFd = dlopen(lib, RTLD_NOW);         
	if (m_dll->libFd == NULL)    
	{    
	    loge("dlopen '%s' fail: %s", lib, dlerror());        
	    return -1;    
	}    

	m_dll->OMX_Init = (OMX_Init_Func)dlsym(m_dll->libFd, "OMX_Init");
	if (m_dll->OMX_Init == NULL)    {        
		loge("Invalid libOMX_Core.so, OMX_Init not found.");        
		goto EXIT;    
	}
	m_dll->OMX_Deinit = (OMX_Deinit_Func)dlsym(m_dll->libFd, "OMX_Deinit");
	if (m_dll->OMX_Init == NULL)    {        
		loge("Invalid libOMX_Core.so, OMX_Deinit not found.");        
		goto EXIT;    
	}
	m_dll->OMX_GetHandle = (OMX_GetHandle_Func)dlsym(m_dll->libFd, "OMX_GetHandle");
	if (m_dll->OMX_Init == NULL)    {        
		loge("Invalid libOMX_Core.so, OMX_GetHandle not found.");        
		goto EXIT;    
	}
	m_dll->OMX_FreeHandle = (OMX_FreeHandle_Func)dlsym(m_dll->libFd, "OMX_FreeHandle");
	if (m_dll->OMX_Init == NULL)    {        
		loge("Invalid libOMX_Core.so, OMX_FreeHandle not found.");        
		goto EXIT;    
	}

	return 0;

EXIT:
	dlclose(m_dll->libFd);
	return -1;
}

void COpenMax:: unload(void)
{
	if (m_dll->libFd){
		dlclose(m_dll->libFd);
		m_dll->libFd = NULL;
	}

}

COpenMax::COpenMax()
{
  logw("%s::%s %d\n", CLASSNAME, __func__, __LINE__);

  m_dll = (OMX_LIB*)malloc(sizeof(OMX_LIB));
  load();  
  m_is_open = false;

  m_omx_decoder = NULL;
  m_omx_client_state = DEAD;
  m_omx_decoder_state = 0;
  sem_init(&m_omx_decoder_state_change, 0, 0);
  logw("%s::%s %d\n", CLASSNAME, __func__, __LINE__);
}

COpenMax::~COpenMax()
{
  logw("%s::%s\n", CLASSNAME, __func__);
  sem_destroy(&m_omx_decoder_state_change);
  unload();
  free(m_dll);
}




////////////////////////////////////////////////////////////////////////////////////////////
// DecoderEventHandler -- OMX event callback
OMX_ERRORTYPE COpenMax::DecoderEventHandlerCallback(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 nData1,
  OMX_U32 nData2,
  OMX_PTR pEventData)
{
  COpenMax *ctx = (COpenMax*)pAppData;
  return ctx->DecoderEventHandler(hComponent, pAppData, eEvent, nData1, nData2, pEventData);
}

// DecoderEmptyBufferDone -- OpenMax input buffer has been emptied
OMX_ERRORTYPE COpenMax::DecoderEmptyBufferDoneCallback(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer)
{
  COpenMax *ctx = (COpenMax*)pAppData;
  return ctx->DecoderEmptyBufferDone( hComponent, pAppData, pBuffer);
}

// DecoderFillBufferDone -- OpenMax output buffer has been filled
OMX_ERRORTYPE COpenMax::DecoderFillBufferDoneCallback(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer)
{
  COpenMax *ctx = (COpenMax*)pAppData;
  return ctx->DecoderFillBufferDone(hComponent, pAppData, pBuffer);
}



// Wait for a component to transition to the specified state
OMX_ERRORTYPE COpenMax::WaitForState(OMX_STATETYPE state)
{
  OMX_STATETYPE test_state;
  int tries = 0;
  struct timespec timeout;
  OMX_ERRORTYPE omx_error = OMX_GetState(m_omx_decoder, &test_state);
  
  while ((omx_error == OMX_ErrorNone) && (test_state != state)) 
  {
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 1;
    sem_timedwait(&m_omx_decoder_state_change, &timeout);
    if (errno == ETIMEDOUT)
      tries++;
    if (tries > 5)
      return OMX_ErrorUndefined;

    omx_error = OMX_GetState(m_omx_decoder, &test_state);
  }

  return omx_error;
}

// SetStateForAllComponents
// Blocks until all state changes have completed
OMX_ERRORTYPE COpenMax::SetStateForComponent(OMX_STATETYPE state)
{
  OMX_ERRORTYPE omx_err;

  #if defined(OMX_DEBUG_VERBOSE)
  logv("%s::%s - state(%d)\n", CLASSNAME, __func__, state);
  #endif
  omx_err = OMX_SendCommand(m_omx_decoder, OMX_CommandStateSet, state, 0);
  if (omx_err)
    loge("%s::%s - OMX_CommandStateSet failed with omx_err(0x%x)\n",
      CLASSNAME, __func__, omx_err);
  else
    omx_err = WaitForState(state);

  return omx_err;
}

bool COpenMax::Initialize( const std::string &decoder_name)
{
	logw("%s::%s %d\n", CLASSNAME, __func__, __LINE__);

  OMX_ERRORTYPE omx_err = m_dll->OMX_Init();
  if (omx_err)
  {
    loge("%s::%s - OpenMax failed to init, status(%d), ", 
      CLASSNAME, __func__, omx_err );
    return false;
  }
  logw("%s::%s %d\n", CLASSNAME, __func__, __LINE__);

  // Get video decoder handle setting up callbacks, component is in loaded state on return.
  static OMX_CALLBACKTYPE decoder_callbacks = {
    &DecoderEventHandlerCallback, &DecoderEmptyBufferDoneCallback, &DecoderFillBufferDoneCallback };
  omx_err = m_dll->OMX_GetHandle(&m_omx_decoder, (char*)decoder_name.c_str(), this, &decoder_callbacks);
  if (omx_err)
  {
    loge("%s::%s - could not get decoder handle\n", CLASSNAME, __func__);
    m_dll->OMX_Deinit();
    return false;
  }
  logw("%s::%s -%d\n", CLASSNAME, __func__, __LINE__);

  return true;
}

void COpenMax::Deinitialize()
{
  logw("%s::%s\n", CLASSNAME, __func__);
  m_dll->OMX_FreeHandle(m_omx_decoder);
  m_omx_decoder = NULL;
  m_dll->OMX_Deinit();
}

// OpenMax decoder callback routines.
OMX_ERRORTYPE COpenMax::DecoderEventHandler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2, OMX_PTR pEventData)
{
  return OMX_ErrorNone;
}

OMX_ERRORTYPE COpenMax::DecoderEmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer)
{
  return OMX_ErrorNone;
}

OMX_ERRORTYPE COpenMax::DecoderFillBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBufferHeader)
{
  return OMX_ErrorNone;
}

