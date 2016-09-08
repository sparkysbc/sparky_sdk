#pragma once
/*
* OWL Video decoder framework .
* Copyright (c) Actions(Zhuhai) Technology Co. Ltd.
* Copyright (c)2015 SY Lee
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*/

#ifndef  __OWL_CODEC_H__
#define __OWL_CODEC_H__

#include <OMX_Component.h>
#include "owl_private.h"
#include "Thread.h"
#include "OpenMax.h"
#include "OWLVdecoder.h"
#include <vector>
#include <string>
#include <queue>


class COWLVideo;


// an omx video frame
struct OWLVideoBuffer : public IResourceCounted<OWLVideoBuffer> {
  OWLVideoBuffer();
  virtual ~OWLVideoBuffer();

  OMX_BUFFERHEADERTYPE *omx_buffer;
  int width;
  int height;
  void PassBackToRenderer();
  void ReleaseTexture();
  void SetOWLVideo(COWLVideo *OWLVideo);

private:
  COWLVideo *m_OWLVideo;
};

typedef struct {
	int width;
	int height;
	int stride;
	int slice_height;
	int color_format;
	int crop_left;
	int crop_top;
	int crop_right;
	int crop_bottom;
}OMXOutputFormat;

enum {
	kPortIndexInput  = 0,
	kPortIndexOutput = 1
};

enum PortStatus {
	ENABLED,
	DISABLING,
	DISABLED,
	ENABLING,
	SHUTTING_DOWN,
};

struct OMXMessage{
	enum {
        EVENT,
        EMPTY_BUFFER_DONE,
        FILL_BUFFER_DONE,
    } type;

	union{
	    struct{
	        OMX_HANDLETYPE hComponent;
	        OMX_PTR pAppData;
			OMX_EVENTTYPE eEvent;
			OMX_U32 nData1;
			OMX_U32 nData2;
			OMX_PTR pEventData;
		}event_data;
		
        struct{
	  		OMX_HANDLETYPE hComponent;
  			OMX_PTR pAppData;
  			OMX_BUFFERHEADERTYPE* pBuffer;
        }buffer_data;
	}u;
};

class OWLVideoBufferHolder : public IResourceCounted<OWLVideoBufferHolder> {
public:
  OWLVideoBufferHolder(OWLVideoBuffer *OWLVideoBuffer);
  virtual ~OWLVideoBufferHolder();

  OWLVideoBuffer *m_OWLVideoBuffer;
};

class COWLVideo : public COpenMax, public IResourceCounted<COWLVideo>, public CThread
{
public:
  COWLVideo();
  virtual ~COWLVideo();

  // Required overrides
  bool Open(VideoStreamInfo &hints);
  void Close(void);
  int  Decode(uint8_t *pData, int iSize, double dts, double pts);
  int GetPicture(VideoPicture *pDvdVideoPicture);
  bool ClearPicture(VideoPicture *pDvdVideoPicture);
  void ReleaseBuffer(OWLVideoBuffer *buffer);
  OMXOutputFormat* getOutFormat(void){return &mOutputFormat;}

protected:

  virtual void OnStartup();
  virtual void OnExit();
  virtual void Process(); 

  int EnqueueDemuxPacket(omx_demux_packet demux_packet);

  void QueryCodec(void);
  OMX_ERRORTYPE PrimeFillBuffers(void);
  OMX_ERRORTYPE AllocOMXInputBuffers(void);
  OMX_ERRORTYPE FreeOMXInputBuffers(bool wait);
  OMX_ERRORTYPE AllocOMXOutputBuffers(void);
  OMX_ERRORTYPE FreeOMXOutputBuffers(bool wait);

  // TODO Those should move into the base class. After start actions can be executed by callbacks.
  OMX_ERRORTYPE StartDecoder(void);
  OMX_ERRORTYPE StopDecoder(void);

  void ReleaseDemuxQueue();

  OMX_ERRORTYPE SetComponentRole(OMX_VIDEO_CODINGTYPE codecType);
  OMX_ERRORTYPE setVideoPortFormatType(
		  OMX_U32 portIndex,
		  OMX_VIDEO_CODINGTYPE compressionFormat,
		  OMX_COLOR_FORMATTYPE colorFormat); 
  OMX_ERRORTYPE setVideoOutputFormat(OMX_VIDEO_CODINGTYPE codecType);
  OMX_ERRORTYPE initOutputFormat(void);
  OMX_ERRORTYPE configureCodec(OMX_VIDEO_CODINGTYPE codecType);
  void setState(OMX_CLIENT_STATE newState);
  void onStateChange(OMX_STATETYPE newState);
  void onCmdComplete(OMX_PTR pAppData,OMX_COMMANDTYPE cmd, OMX_U32 data);
  void onPortSettingsChanged(OMX_U32 portIndex);
  bool flushPortAsync(OMX_U32 portIndex);
  OMX_ERRORTYPE disablePortAsync(OMX_U32 portIndex);

  OMX_ERRORTYPE enablePortAsync(OMX_U32 portIndex);
  bool isIntermediateState(OMX_CLIENT_STATE state);

  OMX_ERRORTYPE stopOmxComponent_l(void);
  int fillinPrivateData(void);
  OMX_ERRORTYPE OnEventHandler(
	OMX_HANDLETYPE hComponent,
	OMX_PTR pAppData,
	OMX_EVENTTYPE eEvent,
	OMX_U32 nData1,
	OMX_U32 nData2,
	OMX_PTR pEventData);

  
  OMX_ERRORTYPE OnEmptyBufferDone(
    OMX_HANDLETYPE hComponent, 
    OMX_PTR pAppData, 
    OMX_BUFFERHEADERTYPE* pBuffer);

  OMX_ERRORTYPE OnFillBufferDone(
    OMX_HANDLETYPE hComponent, 
    OMX_PTR pAppData, 
    OMX_BUFFERHEADERTYPE* pBufferHeader);


  // OpenMax decoder callback routines.
  virtual OMX_ERRORTYPE DecoderEventHandler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
    OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2, OMX_PTR pEventData);
  virtual OMX_ERRORTYPE DecoderEmptyBufferDone(
    OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer);
  virtual OMX_ERRORTYPE DecoderFillBufferDone(
    OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBufferHeader);


  OMX_CLIENT_STATE mState;
  OMXOutputFormat mOutputFormat;
  PortStatus mPortStatus[2];
  VideoStreamInfo  m_hints;
  bool mInitialBufferSubmit;

  OMX_VIDEO_CODINGTYPE mCodecType;

  bool              m_drop_state;
  int               m_decoded_width;
  int               m_decoded_height;

  std::queue<double> m_dts_queue;
  std::queue<omx_demux_packet> m_demux_queue;

  // Synchronization
  pthread_mutex_t   m_omx_queue_mutex;
  pthread_cond_t    m_omx_queue_available;

  pthread_mutex_t	m_event_queue_mutex;
  pthread_cond_t    m_event_queue_available;

  std::queue<OMXMessage*> m_omx_message;
  
  // OpenMax input buffers (demuxer packets)
  std::queue<OMX_BUFFERHEADERTYPE*> m_omx_input_avaliable;
  std::vector<OMX_BUFFERHEADERTYPE*> m_omx_input_buffers;
  bool              m_omx_input_eos;
  int               m_omx_input_port;

  // OpenMax output buffers (video frames)
  std::queue<OWLVideoBuffer*> m_omx_output_busy;
  std::queue<OWLVideoBuffer*> m_omx_output_ready;
  std::vector<OWLVideoBuffer*> m_omx_output_buffers;
  bool              m_omx_output_eos;
  int               m_omx_output_port;

  bool              m_portChanging;
};

#endif

