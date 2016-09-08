
#include "log.h"
#include "OWLCodec.h"

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Index.h>
#include <OMX_RoleNames.h>
#include "OWLCodec.h"


#if 1
#define OMX_DEBUG_VERBOSE
//#define OMX_DEBUG_EMPTYBUFFERDONE
//#define OMX_DEBUG_FILLBUFFERDONE
#endif

#define CLASSNAME "OWLVideo"

COWLVideo::COWLVideo():
	CThread("COWLVideo")
{
  m_portChanging = false;
  pthread_mutex_init(&m_omx_queue_mutex, NULL);
  pthread_cond_init(&m_omx_queue_available, NULL);
  pthread_mutex_init(&m_event_queue_mutex, NULL);
  pthread_cond_init(&m_event_queue_available, NULL);
  memset(&mOutputFormat, 0, sizeof(OMXOutputFormat));
  m_drop_state = false;
  m_decoded_width = 0;
  m_decoded_height = 0;
  m_omx_input_eos = false;
  m_omx_input_port = 0;
  m_omx_output_eos = false;
  m_omx_output_port = 0;
  mState = LOADED;
  mPortStatus[kPortIndexInput] = ENABLED;
  mPortStatus[kPortIndexOutput] = ENABLED;
  mInitialBufferSubmit = true;
  memset(&m_hints,0,sizeof(VideoStreamInfo));

}

COWLVideo::~COWLVideo()
{
  #if defined(OMX_DEBUG_VERBOSE)
  logv("%s::%s\n", CLASSNAME, __func__);
  #endif
  if (m_is_open)
    Close();
  
  StopThread();
}

void COWLVideo::OnStartup(){
  logw("%s::%s \n",CLASSNAME, __func__);
}

void COWLVideo::OnExit(){
	logw("%s::%s \n",CLASSNAME, __func__);
	m_bStop = true;
}
void COWLVideo::Process(){
	OMXMessage* message;
    while (!m_bStop)
    {
        pthread_mutex_lock(&m_event_queue_mutex);
		if (m_omx_message.empty()) {
            struct timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_nsec += 50000000;
            if (timeout.tv_nsec >= 1000000000) {
                timeout.tv_sec += 1;
                timeout.tv_nsec -=  1000000000;
            }
          pthread_cond_timedwait(&m_event_queue_available, &m_event_queue_mutex, &timeout);
        }
		
        if (!m_omx_message.empty())
        {
            message = m_omx_message.front();
			logv("%s::%s new message \n",CLASSNAME, __func__);

			switch(message->type){
				case OMXMessage::EVENT:
					pthread_mutex_lock(&m_omx_queue_mutex);
					OnEventHandler(message->u.event_data.hComponent, message->u.event_data.pAppData, 
				    	message->u.event_data.eEvent, message->u.event_data.nData1, 
				    	message->u.event_data.nData2, message->u.event_data.pEventData);
					pthread_mutex_unlock(&m_omx_queue_mutex);
					break;
				case OMXMessage::EMPTY_BUFFER_DONE:
					OnEmptyBufferDone(message->u.buffer_data.hComponent, message->u.buffer_data.pAppData, 
				    	message->u.buffer_data.pBuffer);
					break;
				case OMXMessage::FILL_BUFFER_DONE:
					OnFillBufferDone(message->u.buffer_data.hComponent, message->u.buffer_data.pAppData, 
				    	message->u.buffer_data.pBuffer);
					break;
			}
			
			m_omx_message.pop();
			delete message;
			
        }
		pthread_mutex_unlock(&m_event_queue_mutex);
    }
    logw("%s::%s out \n",CLASSNAME, __func__);
	
}

OMX_ERRORTYPE COWLVideo:: SetComponentRole(OMX_VIDEO_CODINGTYPE codecType) {
    struct MimeToRole {
        OMX_VIDEO_CODINGTYPE codecType;
        const char *decoderRole;
    };

    static const MimeToRole kMimeToRole[] = {
        
        { OMX_VIDEO_CodingAVC,  VIDDEC_COMPONENTROLES_AVC},
        { OMX_VIDEO_CodingMPEG4,VIDDEC_COMPONENTROLES_MPEG4},
        { OMX_VIDEO_CodingH263, VIDDEC_COMPONENTROLES_H263},
        { OMX_VIDEO_CodingVP8,  VIDDEC_COMPONENTROLES_VP8 },
        { OMX_VIDEO_CodingMPEG2,VIDDEC_COMPONENTROLES_MPEG2},

		{ OMX_VIDEO_CodingMJPEG,VIDDEC_COMPONENTROLES_MJPEG },
        { OMX_VIDEO_CodingVP6,  VIDDEC_COMPONENTROLES_VP6},
        { OMX_VIDEO_CodingFLV1, VIDDEC_COMPONENTROLES_FLV1},
        { OMX_VIDEO_CodingRV, VIDDEC_COMPONENTROLES_RV},
        { OMX_VIDEO_CodingRV30, VIDDEC_COMPONENTROLES_RV30},
        { OMX_VIDEO_CodingRV40, VIDDEC_COMPONENTROLES_RV40},
        { OMX_VIDEO_CodingVC1,  VIDDEC_COMPONENTROLES_VC1},
        { OMX_VIDEO_CodingWMV3, VIDDEC_COMPONENTROLES_WMV3 },
    };

    static const size_t kNumMimeToRole =
        sizeof(kMimeToRole) / sizeof(kMimeToRole[0]);

    size_t i;
    for (i = 0; i < kNumMimeToRole; ++i) {
        if (codecType ==  kMimeToRole[i].codecType) {
            break;
        }
    }

    if (i == kNumMimeToRole) {
        return OMX_ErrorUndefined;
    }

    const char *role = kMimeToRole[i].decoderRole;

    if (role != NULL) {
		OMX_ERRORTYPE omx_err = OMX_ErrorNone;
        OMX_PARAM_COMPONENTROLETYPE roleParams;
        OMX_INIT_STRUCTURE(roleParams);

        strncpy((char *)roleParams.cRole,
                role, OMX_MAX_STRINGNAME_SIZE - 1);
        roleParams.cRole[OMX_MAX_STRINGNAME_SIZE - 1] = '\0';

		logd("%s::%s set standard component role %s\n",
        CLASSNAME, __func__,role);
		
        omx_err = OMX_SetParameter(m_omx_decoder, OMX_IndexParamStandardComponentRole, &roleParams);
        return omx_err;
    }
	return OMX_ErrorUndefined;
}

OMX_ERRORTYPE COWLVideo::setVideoPortFormatType(
        OMX_U32 portIndex,
        OMX_VIDEO_CODINGTYPE compressionFormat,
        OMX_COLOR_FORMATTYPE colorFormat) {

	OMX_ERRORTYPE omx_err = OMX_ErrorNone;
    OMX_VIDEO_PARAM_PORTFORMATTYPE format;
    OMX_INIT_STRUCTURE(format);
    format.nPortIndex = portIndex;
    format.nIndex = 0;
    bool found = false;
    OMX_U32 index = 0;
	
    for (;;) {
        format.nIndex = index;

		omx_err = OMX_GetParameter(m_omx_decoder, OMX_IndexParamVideoPortFormat, &format);

        if (omx_err) {
			loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,omx_err);
            return omx_err;
        }

        if (format.eCompressionFormat == compressionFormat
                && format.eColorFormat == colorFormat) {
            found = true;
            break;
        }

        ++index;
    }

    if (!found) {
        return OMX_ErrorUndefined;
    }

    logd("found a match.");
	 
    omx_err = OMX_SetParameter(m_omx_decoder, OMX_IndexParamVideoPortFormat, &format);
	 
    return omx_err;
}


OMX_ERRORTYPE COWLVideo::setVideoOutputFormat(OMX_VIDEO_CODINGTYPE codecType) 
{
    OMX_ERRORTYPE omx_err = OMX_ErrorNone;
    OMX_VIDEO_CODINGTYPE compressionFormat = codecType;

    omx_err = setVideoPortFormatType(
            kPortIndexInput, compressionFormat, OMX_COLOR_FormatUnused);

    if (omx_err != OMX_ErrorNone) {
		loge("%s::%s -%d, err(0x%x)", CLASSNAME, __func__,__LINE__,omx_err);
        return omx_err;
    }

    {

        OMX_VIDEO_PARAM_PORTFORMATTYPE format;
        OMX_INIT_STRUCTURE(format);
        format.nPortIndex = kPortIndexOutput;
        format.nIndex = 0;

		omx_err = OMX_GetParameter(m_omx_decoder, OMX_IndexParamVideoPortFormat, &format);

        if (omx_err) {
			loge("%s::%s -%d, err(0x%x)", CLASSNAME, __func__,__LINE__,omx_err);
            return omx_err;
        }
		
		int colorFormat = OMX_COLOR_FormatYUV420SemiPlanar;

		if (colorFormat != format.eColorFormat) {

            while (OMX_ErrorNoMore != omx_err) {
                format.nIndex++;

                omx_err = OMX_GetParameter(m_omx_decoder, OMX_IndexParamVideoPortFormat,
                            &format);
				if (omx_err) {
					loge("%s::%s -%d, err(0x%x)", CLASSNAME, __func__,__LINE__,omx_err);
                    return omx_err;
                }

				logw("Color format %d is supported", format.eColorFormat);

                if (format.eColorFormat == colorFormat) {
                    break;
                }
            }
			
            if (format.eColorFormat != colorFormat) {
                logw("Color format %d is not supported", colorFormat);
                return OMX_ErrorUndefined;
            }
        }

		omx_err = OMX_SetParameter(m_omx_decoder, OMX_IndexParamVideoPortFormat,
                &format);

        if (omx_err) {
			loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,omx_err);
            return omx_err;
        }

    }

    OMX_PARAM_PORTDEFINITIONTYPE def;
    OMX_INIT_STRUCTURE(def);
    def.nPortIndex = kPortIndexInput;

    OMX_VIDEO_PORTDEFINITIONTYPE *video_def = &def.format.video;

	omx_err = OMX_GetParameter(m_omx_decoder, OMX_IndexParamPortDefinition, &def);

    if (omx_err) {
		loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,omx_err);
        return omx_err;
    }


    video_def->nFrameWidth = m_decoded_width;
    video_def->nFrameHeight = m_decoded_height;


    video_def->eCompressionFormat = compressionFormat;
    video_def->eColorFormat = OMX_COLOR_FormatUnused;

	omx_err = OMX_SetParameter(m_omx_decoder, OMX_IndexParamPortDefinition, &def);

    if (omx_err) {
		loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,omx_err);
        return omx_err;
    }

    OMX_INIT_STRUCTURE(def);
    def.nPortIndex = kPortIndexOutput;

    omx_err = OMX_GetParameter(
            m_omx_decoder, OMX_IndexParamPortDefinition, &def);
	if (omx_err) {
		loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,omx_err);
        return omx_err;
    }

    if(def.eDomain == OMX_PortDomainVideo){
		logw("def.eDomain == OMX_PortDomainVideo");
	}

    video_def->nFrameWidth = m_decoded_width;
    video_def->nFrameHeight = m_decoded_height;

    omx_err = OMX_SetParameter(
            m_omx_decoder, OMX_IndexParamPortDefinition, &def);

    return omx_err;
	
}


OMX_ERRORTYPE COWLVideo::initOutputFormat(void) {

	OMX_ERRORTYPE err = OMX_ErrorNone;

    OMX_PARAM_PORTDEFINITIONTYPE def;
    OMX_INIT_STRUCTURE(def);
    def.nPortIndex = kPortIndexOutput;

	err = OMX_GetParameter(m_omx_decoder, OMX_IndexParamPortDefinition, &def);

    if (err) {
		loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,err);
        return err;
    }
	
	switch (def.eDomain) {
		case OMX_PortDomainVideo:
		{
	       OMX_VIDEO_PORTDEFINITIONTYPE *video_def = &def.format.video;

		   logd("video_def nFrameWidth =%d nFrameHeight =%d eColorFormat=%d",
		   	    video_def->nFrameWidth,
		   	    video_def->nFrameHeight,
		   	    video_def->eColorFormat);
		   
		   mOutputFormat.width = video_def->nFrameWidth;
		   mOutputFormat.height = video_def->nFrameHeight;
		   mOutputFormat.color_format =  video_def->eColorFormat;

	       mOutputFormat.stride = video_def->nStride;
	       mOutputFormat.slice_height = video_def->nSliceHeight;

           OMX_CONFIG_RECTTYPE rect;
           OMX_INIT_STRUCTURE(rect);
           rect.nPortIndex = kPortIndexOutput;
           err = OMX_GetConfig(m_omx_decoder, OMX_IndexConfigCommonOutputCrop,
                            &rect);
           if (err == OMX_ErrorNone) {
			   logd("rect.nLeft =%d rect.nTop =%d rect.nright=%d rect.nBottom=%d",
                    rect.nLeft, rect.nTop,
                    rect.nLeft + rect.nWidth - 1,
                    rect.nTop + rect.nHeight - 1);
			   mOutputFormat.crop_left = rect.nLeft;
			   mOutputFormat.crop_top = rect.nTop;
			   mOutputFormat.crop_right = rect.nLeft + rect.nWidth - 1;
			   mOutputFormat.crop_bottom = rect.nTop + rect.nHeight - 1;
           }else{
           	   mOutputFormat.crop_left = 0;
			   mOutputFormat.crop_top = 0;
			   mOutputFormat.crop_right = video_def->nFrameWidth - 1;
			   mOutputFormat.crop_bottom = video_def->nFrameHeight - 1;
           } 
		}
		default:
			break;
	}
	return err;
}


OMX_ERRORTYPE COWLVideo::configureCodec(OMX_VIDEO_CODINGTYPE codecType) 
{
    OMX_ERRORTYPE err = setVideoOutputFormat(codecType);
    if (err) {
		loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,err);
        return err;
    }
    return initOutputFormat();
}


void COWLVideo::setState(OMX_CLIENT_STATE newState) {
    mState = newState;
}

void COWLVideo::onStateChange(OMX_STATETYPE newState) {
    logw("onStateChange %d", newState);
	OMX_ERRORTYPE err;

    switch (newState) {
        case OMX_StateIdle:
        {
            logw("Now Idle.");
            if (mState == LOADED_TO_IDLE) {
				err = OMX_SendCommand(m_omx_decoder, OMX_CommandStateSet, OMX_StateExecuting, 0);
				if (err) {
				   loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,err);
				   break;
				}

                setState(IDLE_TO_EXECUTING);
            } else {
                if(mState != EXECUTING_TO_IDLE) {
					loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,err);
					break;
                }

				err = OMX_SendCommand(m_omx_decoder, OMX_CommandStateSet, OMX_StateLoaded, 0);
				if (err) {
					loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,err);
				   break;
				}

                err = FreeOMXInputBuffers(false);
                if(err) {
					loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,err);
					break;
                }

                err = FreeOMXOutputBuffers(false);
				if(err) {
					loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,err);
					break;
				}

			    mPortStatus[kPortIndexInput] = ENABLED;
                mPortStatus[kPortIndexOutput] = ENABLED;

                setState(IDLE_TO_LOADED);
            }
            break;
        }

        case OMX_StateExecuting:
        {
            logw("Now Executing.");

            setState(EXECUTING);

            break;
        }

        case OMX_StateLoaded:
        {
            logw("Now Loaded.");

            setState(LOADED);
            break;
        }

        case OMX_StateInvalid:
        {
            setState(ERROR);
            break;
        }

        default:
        {
            loge("should not be here.");
            break;
        }
    }
}


void COWLVideo::onCmdComplete(OMX_PTR pAppData,OMX_COMMANDTYPE cmd, OMX_U32 data) {
	COWLVideo *ctx = static_cast<COWLVideo*>(pAppData);
    switch (cmd) {
        case OMX_CommandStateSet:
        {
            ctx->m_omx_decoder_state = (int)data;
            logw("StateChange = > %d", data);
            onStateChange((OMX_STATETYPE)data);
            sem_post(&ctx->m_omx_decoder_state_change);
            break;
        }

        case OMX_CommandPortDisable:
        {
          logw("%s::%s - OMX_CommandPortDisable, nData1(0x%lx), nData2(0x%lx)\n",
            CLASSNAME, __func__, cmd, data);
		  
		  OMX_U32 portIndex = data;
		  mPortStatus[portIndex] = DISABLED;

		  if (mState == RECONFIGURING) {
              if (ctx->m_omx_output_port == (int)data)
              {
                  initOutputFormat();

				  OMX_ERRORTYPE err = enablePortAsync(portIndex);
                if (err) {
                    loge("enablePortAsync(%u) failed (err = %d)", portIndex, err);
                    setState(ERROR);
                } else {
                    err = ctx->AllocOMXOutputBuffers();;
                    if (err) {
                        loge("allocateBuffersOnPort (output) failed (err = %x)",err);
                        setState(ERROR);
                    }
                }
			  }
          }
   
          break;
        }

        case OMX_CommandPortEnable:
        {
          logw("%s::%s - OMX_CommandPortEnable, nData1(0x%lx), nData2(0x%lx)\n",
            CLASSNAME, __func__, cmd, data);
		  
		  OMX_U32 portIndex = data;
		  mPortStatus[portIndex] = ENABLED;
		  logw("%s::%s --%d\n",CLASSNAME, __func__, __LINE__);
		  if (mState == RECONFIGURING) {
              if (ctx->m_omx_output_port == (int)data)
              {
				  setState(EXECUTING);
                  ctx->PrimeFillBuffers();
              }
		  }
		  logw("%s::%s --%d\n",CLASSNAME, __func__, __LINE__);
          ctx->m_portChanging = false;

          break;
        }

        case OMX_CommandFlush:
        {
			OMX_U32 portIndex = data;
			logw("%s::%s - OMX_CommandFlush, FLUSH_DONE(%u)\n",
              CLASSNAME, __func__, portIndex);
			
			mPortStatus[portIndex] = ENABLED;

			if (mState == RECONFIGURING) {		
				disablePortAsync(portIndex);
			} 

            break;
        }

        default:
        {
            loge("CMD_COMPLETE(%d, %ld)", cmd, data);
            break;
        }
    }
}



void COWLVideo::onPortSettingsChanged(OMX_U32 portIndex) {
   logd("PORT_SETTINGS_CHANGED(%ld)", portIndex);

    if (mState != EXECUTING && mState != EXECUTING_TO_IDLE){
        return;
	}
	logd("PORT_SETTINGS_CHANGED --2");
    if (portIndex != kPortIndexOutput) return;
	logd("PORT_SETTINGS_CHANGED --3");

	if (mPortStatus[kPortIndexOutput] != ENABLED) {
        loge("Deferring output port settings change.");
        return;
    }

    setState(RECONFIGURING);

    if (!flushPortAsync(portIndex)) {
        onCmdComplete(NULL, OMX_CommandFlush, portIndex);
    }
}

bool COWLVideo::flushPortAsync(OMX_U32 portIndex) {
	 logd("flushPortAsync(%ld)",portIndex);
    if (mState != EXECUTING && mState != RECONFIGURING
        && mState != EXECUTING_TO_IDLE)
    {
        return false;
    }
	logd("flushPortAsync2(%ld)",portIndex);

	mPortStatus[portIndex] = SHUTTING_DOWN;

	OMX_ERRORTYPE omx_err = OMX_SendCommand(m_omx_decoder, OMX_CommandFlush, portIndex, NULL);
	if (omx_err ) {
	   loge("flushPortAsync(%u) failed (err = %x)", portIndex, omx_err);
	   return false;
	}
    return true;
}

OMX_ERRORTYPE COWLVideo::disablePortAsync(OMX_U32 portIndex) {
    if (mState != EXECUTING && mState != RECONFIGURING) 
		return OMX_ErrorUndefined;

    logd("sending OMX_CommandPortDisable(%ld)", portIndex);

	mPortStatus[portIndex] = DISABLING;
    OMX_ERRORTYPE omx_err =
        OMX_SendCommand(m_omx_decoder, OMX_CommandPortDisable, portIndex, NULL);
	if (omx_err ) {
	   loge("disablePortAsync(%u) failed (err = %x)", portIndex, omx_err);
	   return omx_err;
	}

	if (portIndex == 0)
	{
	    omx_err = FreeOMXInputBuffers(false);
	}else{
        omx_err = FreeOMXOutputBuffers(false);
	}
	return omx_err;
}

OMX_ERRORTYPE COWLVideo::enablePortAsync(OMX_U32 portIndex) {
    if (mState != EXECUTING && mState != RECONFIGURING) 
		return OMX_ErrorUndefined;
	mPortStatus[portIndex] = ENABLING;

    logd("sending OMX_CommandPortEnable(%ld)", portIndex);
    return OMX_SendCommand(m_omx_decoder, OMX_CommandPortEnable, portIndex, NULL);
}

bool COWLVideo::isIntermediateState(OMX_CLIENT_STATE state) {
    return state == LOADED_TO_IDLE
        || state == IDLE_TO_EXECUTING
        || state == EXECUTING_TO_IDLE
        || state == IDLE_TO_LOADED
        || state == RECONFIGURING;
}

OMX_ERRORTYPE COWLVideo::stopOmxComponent_l(void) {
    logw("stopOmxComponent_l mState=%d", mState);

	OMX_ERRORTYPE err = OMX_ErrorNone;
	struct timespec timeout;

    int all_timeout = 0; 
    while (isIntermediateState(mState)) {
		if(all_timeout > 5) break;
		logd("stopOmxComponent_l --1 mState=%d", mState);
        
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1;
        sem_timedwait(&m_omx_decoder_state_change, &timeout);
        
		logd("stopOmxComponent_l --2   mState=%d", mState);
    }

    bool isError = false;
    switch (mState) {
        case LOADED:
            break;

        case ERROR:
        {
            OMX_ERRORTYPE err;
            if (mPortStatus[kPortIndexOutput] == ENABLING) {
				err = FreeOMXInputBuffers(false);         
				err = FreeOMXOutputBuffers(false);
				setState(LOADED);
                break;
            } else {
                OMX_STATETYPE state = OMX_StateInvalid;
                err = OMX_GetState(m_omx_decoder, &state);
				if (err)
				{
				    break;
				}

                if (state != OMX_StateExecuting) {
                    break;
                }
                // else fall through to the idling code
            }

            isError = true;
        }

        case EXECUTING:
        {
            setState(EXECUTING_TO_IDLE);

            mPortStatus[kPortIndexInput] = SHUTTING_DOWN;
            mPortStatus[kPortIndexOutput] = SHUTTING_DOWN;

            OMX_ERRORTYPE err =
                   OMX_SendCommand(m_omx_decoder, OMX_CommandStateSet, OMX_StateIdle,NULL);
			if (err){
				loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,err);
				setState(ERROR);
                break;
			}
            logw("===wait loaded start ,now mState is :%d===\n",mState);
            all_timeout = 0;
			while (mState != LOADED && mState != ERROR) {
				if(all_timeout > 10) break;
				clock_gettime(CLOCK_REALTIME, &timeout);
				timeout.tv_sec += 1;
				sem_timedwait(&m_omx_decoder_state_change, &timeout);  
            }
            logw("===wait loaded end ,now mState is :%d===\n",mState);
            if (isError) {
                // We were in the ERROR state coming in, so restore that now
                // that we've idled the OMX component.
                setState(ERROR);
            }

            break;
        }

        default:
        {
            loge("should not be here.");
            break;
        }
    }

    return err;
}


bool COWLVideo::Open(VideoStreamInfo &hints)
{
 
   OMX_ERRORTYPE omx_err = OMX_ErrorNone;
 
   m_decoded_width	= hints.width;
   m_decoded_height = hints.height;
 
   m_hints = hints;
 
   switch (hints.codec)
   {
	 case AV_CODEC_ID_AVS:
	 case AV_CODEC_ID_CAVS:
	 case AV_CODEC_ID_H264:
	 {
		 mCodecType = OMX_VIDEO_CodingAVC;
	 }
	 break;
	 case AV_CODEC_ID_MPEG4:
	   mCodecType = OMX_VIDEO_CodingMPEG4;
	 break;
	 case AV_CODEC_ID_MPEG2VIDEO:
	   mCodecType = OMX_VIDEO_CodingMPEG2;
	 break;
	 
	 case AV_CODEC_ID_VC1:
		 mCodecType = OMX_VIDEO_CodingVC1;
	 break;
	 case AV_CODEC_ID_WMV3:
		 mCodecType = OMX_VIDEO_CodingWMV3;
	 break;
	 
	 case AV_CODEC_ID_H263:
	 case AV_CODEC_ID_H263P:
	 case AV_CODEC_ID_H263I:
		 mCodecType = OMX_VIDEO_CodingH263;
	 break;
	 
	 case AV_CODEC_ID_MJPEG:
		 mCodecType = OMX_VIDEO_CodingMJPEG;
	 break;
	 
	 case AV_CODEC_ID_FLV1:
		 mCodecType = OMX_VIDEO_CodingFLV1;
	 break;
	 
	 case AV_CODEC_ID_RV30:
		 mCodecType = OMX_VIDEO_CodingRV30;
	 case AV_CODEC_ID_RV40:
		 mCodecType = OMX_VIDEO_CodingRV40;
	 break; 
	 
	 case CODEC_ID_VP3:
	 case AV_CODEC_ID_VP6:
	 case AV_CODEC_ID_VP6A:
	 case AV_CODEC_ID_VP6F:
		 mCodecType = OMX_VIDEO_CodingVP6;
	 break;
 
	 case AV_CODEC_ID_VP8:
		 mCodecType = OMX_VIDEO_CodingVP8;
	 break; 
	 default:
	   return false;
	 break;
   }

   m_hints.extradata = NULL;
   if (mCodecType == OMX_VIDEO_CodingRV30 || mCodecType == OMX_VIDEO_CodingRV40 ||
	 	mCodecType == OMX_VIDEO_CodingMPEG2 ||mCodecType == OMX_VIDEO_CodingVC1 ||
	 	mCodecType == OMX_VIDEO_CodingWMV3)
  {
	 if (hints.extradata && hints.extrasize > 0){
		 logv("%s::%s HAVE extradata\n",CLASSNAME, __func__);
		 logv("%s::%s size ==%d\n",CLASSNAME, __func__,hints.extrasize);
		 m_hints.extradata =(char *)malloc(hints.extrasize);
		 
		 m_hints.extrasize = hints.extrasize;
		 memcpy(m_hints.extradata, hints.extradata, hints.extrasize);
	 }
   }
   
   Create();
   logw("%s::%s width =%d,height =%d\n",
	 CLASSNAME, __func__,m_decoded_width,m_decoded_height);
 
   logw("%s::%s CodecType =0x%x\n",CLASSNAME, __func__,mCodecType);
   // initialize OpenMAX.
   if (!Initialize(ACTIONS_VIDEO_DECODER))
   {
	 return false;
   }
   
   omx_err = SetComponentRole(mCodecType);
   if (omx_err)
   {
	 loge("%s::%sFailed to set standard component role, omx_err(0x%x)\n",
	   CLASSNAME, __func__, omx_err);
	 Deinitialize();
	 return false;
   }
 
   omx_err = configureCodec(mCodecType);
   if (omx_err)
   {
	 loge("%s::%sFailed to configureCodec, omx_err(0x%x)\n",
	   CLASSNAME, __func__, omx_err);
	 Deinitialize();
	 return false;
   }
 
   setState(LOADED_TO_IDLE);
 
   // transition decoder component to IDLE state
   omx_err = OMX_SendCommand(m_omx_decoder, OMX_CommandStateSet, OMX_StateIdle, 0);
   if (omx_err)
   {
	 loge("%s::%s - setting OMX_StateIdle failed with omx_err(0x%x)\n",
	   CLASSNAME, __func__, omx_err);
	 Deinitialize();
	 return false;
   }
 
   OMX_PORT_PARAM_TYPE port_param;
   OMX_INIT_STRUCTURE(port_param);
   omx_err = OMX_GetParameter(m_omx_decoder, OMX_IndexParamVideoInit, &port_param);
   if (omx_err)
   {
	 loge("%s::%s - setting OMX_IndexParamVideoInit failed with omx_err(0x%x)\n",
	   CLASSNAME, __func__, omx_err);
	 Deinitialize();
	 return false;
   }
 
   m_omx_input_port = port_param.nStartPortNumber;
   m_omx_output_port = m_omx_input_port + 1;
 
   omx_err = AllocOMXInputBuffers();
   if (omx_err)
   {
	 Deinitialize();
	 loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,omx_err);
	 return false;
   }
 
   omx_err = AllocOMXOutputBuffers();
   if (omx_err)
   {
	 FreeOMXInputBuffers(false);
	 Deinitialize();
	 loge("%s::%s -%d,  err(0x%x)", CLASSNAME, __func__,__LINE__,omx_err);
	 return false;
   }

   m_is_open = true;
   m_drop_state = false;
   // crank it up.
   StartDecoder();
 
  return true;
}

void COWLVideo::Close()
{
  logw("%s::%s\n", CLASSNAME, __func__);
  if (m_omx_decoder)
  {
    if (mState != LOADED)
	{
      StopDecoder();
    }
    Deinitialize();
  }

  pthread_mutex_lock(&m_event_queue_mutex);

  while (!m_omx_message.empty())
  {
     OMXMessage* message = m_omx_message.front();
     m_omx_message.pop();
	 delete message;
  }
  
  if (m_hints.extradata){
	 free(m_hints.extradata);
  }
  pthread_mutex_unlock(&m_event_queue_mutex);
  m_is_open = false;
}

int COWLVideo::EnqueueDemuxPacket(omx_demux_packet demux_packet)
{
  if (mState != EXECUTING && mState != RECONFIGURING)
  {
    loge("%s::%s ccccccc %d\n",CLASSNAME, __func__, __LINE__);
    delete[] demux_packet.buff;
    return VC_ERROR;
  }
  OMX_ERRORTYPE omx_err;
  OMX_BUFFERHEADERTYPE* omx_buffer;

  // need to lock here to retreve an input buffer and pop the queue
  omx_buffer = m_omx_input_avaliable.front();
  m_omx_input_avaliable.pop();

  // setup a new omx_buffer.
  omx_buffer->nFlags  = m_omx_input_eos ? OMX_BUFFERFLAG_EOS : 0;

  omx_buffer->nOffset = 0;

  omx_buffer->nFilledLen = (demux_packet.size > omx_buffer->nAllocLen) ? omx_buffer->nAllocLen : demux_packet.size;

  memcpy(omx_buffer->pBuffer, demux_packet.buff, omx_buffer->nFilledLen);

  delete[] demux_packet.buff;


  omx_buffer->nTimeStamp = (demux_packet.pts == DVD_NOPTS_VALUE) ? 0 : demux_packet.pts * 1000.0; // in microseconds;

  omx_buffer->nInputPortIndex = m_omx_input_port;
#if defined(OMX_DEBUG_EMPTYBUFFERDONE)
  loge("%s::%s - feeding decoder, omx_buffer->pBuffer(0x%p), demuxer_bytes(%d)\n",
	    CLASSNAME, __func__, omx_buffer->pBuffer, omx_buffer->nFilledLen);
#endif
  // Give this omx_buffer to OpenMax to be decoded.
  omx_err = OMX_EmptyThisBuffer(m_omx_decoder, omx_buffer);
  if (omx_err)  {
    loge("%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n",
	      CLASSNAME, __func__, omx_err);
    return VC_ERROR;
  }
  
  m_dts_queue.push(demux_packet.dts);

  return 0;
}



int COWLVideo::fillinPrivateData(void)
{
	OMX_ERRORTYPE omx_err;
	OMX_BUFFERHEADERTYPE* omx_buffer;

	if(m_hints.extradata != NULL &&!m_omx_input_avaliable.empty()) 
	{
        size_t size = m_hints.extrasize;
		
	    omx_buffer = m_omx_input_avaliable.front();
	    m_omx_input_avaliable.pop();
		

		omx_buffer->nFlags = OMX_BUFFERFLAG_ENDOFFRAME | OMX_BUFFERFLAG_CODECCONFIG;
        omx_buffer->nOffset = 0;
	
        omx_buffer->nFilledLen = (size > omx_buffer->nAllocLen) ? omx_buffer->nAllocLen : size;
        memcpy(omx_buffer->pBuffer, m_hints.extradata, omx_buffer->nFilledLen);
	
	
	    omx_buffer->nTimeStamp = 0;
	    omx_buffer->nInputPortIndex = m_omx_input_port;

		logd("%s::%s - feeding decoder, omx_buffer->pBuffer(0x%p), demuxer_bytes(%d)\n",
		  CLASSNAME, __func__, omx_buffer->pBuffer, omx_buffer->nFilledLen);
	
	    omx_err = OMX_EmptyThisBuffer(m_omx_decoder, omx_buffer);
	    if (omx_err)  {
	        loge("%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n",
			CLASSNAME, __func__, omx_err);
	        return VC_ERROR;
	    }
		m_dts_queue.push(DVD_NOPTS_VALUE);
	}
    return 0;
}

int COWLVideo::Decode(uint8_t* pData, int iSize, double dts, double pts)
{
  pthread_mutex_lock(&m_omx_queue_mutex);
    // handle codec extradata
  if (mInitialBufferSubmit)
  {
      mInitialBufferSubmit = false;
      if (fillinPrivateData() == VC_ERROR){
	  	 pthread_mutex_unlock(&m_omx_queue_mutex);
         return VC_ERROR;
	  }
  }

  if (pData)
  {
    int demuxer_bytes = iSize;
    uint8_t *demuxer_content = pData;

    if (m_demux_queue.empty() &&!m_omx_input_avaliable.empty()){

		if (mState != EXECUTING && mState != RECONFIGURING)
		{
		  loge("%s::%s ccccccc %d\n",CLASSNAME, __func__, __LINE__);
		  pthread_mutex_unlock(&m_omx_queue_mutex);
		  return VC_ERROR;
		}
		OMX_ERRORTYPE omx_err;
		OMX_BUFFERHEADERTYPE* omx_buffer;
		
		// need to lock here to retreve an input buffer and pop the queue
		omx_buffer = m_omx_input_avaliable.front();
		m_omx_input_avaliable.pop();
		
		// setup a new omx_buffer.
		omx_buffer->nFlags	= m_omx_input_eos ? OMX_BUFFERFLAG_EOS : 0;
	   
		omx_buffer->nOffset = 0;
		
		omx_buffer->nFilledLen = (demuxer_bytes > omx_buffer->nAllocLen) ? omx_buffer->nAllocLen : demuxer_bytes;
		
		memcpy(omx_buffer->pBuffer, demuxer_content, omx_buffer->nFilledLen);


		omx_buffer->nTimeStamp = (pts == DVD_NOPTS_VALUE) ? 0 : pts * 1000.0; // in microseconds;
		omx_buffer->nInputPortIndex = m_omx_input_port;
		
		// Give this omx_buffer to OpenMax to be decoded.
		omx_err = OMX_EmptyThisBuffer(m_omx_decoder, omx_buffer);
		if (omx_err)  {
		  loge("%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n",
				CLASSNAME, __func__, omx_err);
		  pthread_mutex_unlock(&m_omx_queue_mutex);
		  return VC_ERROR;
		}
		m_dts_queue.push(dts);

	}else{

    // we need to queue then de-queue the demux packet, seems silly but
    // omx might not have a omx input buffer avaliable when we are called
    // and we must store the demuxer packet and try again later.
	omx_demux_packet demux_packet;

    demux_packet.dts = dts;
    demux_packet.pts = pts;

    demux_packet.size = demuxer_bytes;
    demux_packet.buff = new OMX_U8[demuxer_bytes];

    memcpy(demux_packet.buff, demuxer_content, demuxer_bytes);

    m_demux_queue.push(demux_packet);

    while(!m_omx_input_avaliable.empty() && !m_demux_queue.empty())
    {
      demux_packet = m_demux_queue.front();
      m_demux_queue.pop();
      if (EnqueueDemuxPacket(demux_packet) == VC_ERROR){
		  pthread_mutex_unlock(&m_omx_queue_mutex);
		  return VC_ERROR;
      }
    }
  }

    #if 0//defined(OMX_DEBUG_VERBOSE)
    if (m_omx_input_avaliable.empty())
      logd("%s::%s - buffering demux, m_demux_queue_size(%d), demuxer_bytes(%d)\n",
        CLASSNAME, __func__, m_demux_queue.size(), demuxer_bytes);
    #endif
  }

  int returnCode = VC_BUFFER;

  if (m_omx_input_avaliable.empty() && m_omx_output_ready.empty()) {

    // Sleep for some time until either an image has been decoded or there's space in the input buffer again
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_nsec += 50000000;//100000000; // 100ms, 1ms still shows the stuttering
    if (timeout.tv_nsec >= 1000000000) {
      timeout.tv_sec += 1;
      timeout.tv_nsec -=  1000000000;
    }
    pthread_cond_timedwait(&m_omx_queue_available, &m_omx_queue_mutex, &timeout);
  }

  if (!m_omx_output_ready.empty()) {
    returnCode |= VC_PICTURE;
  }
  if (!m_omx_input_avaliable.empty()) {
    returnCode |= VC_BUFFER;
  }

  pthread_mutex_unlock(&m_omx_queue_mutex);

  return returnCode;
}


bool COWLVideo::ClearPicture(VideoPicture* pVideoPicture)
{
  if (pVideoPicture->pSelf) {
  	OWLVideoBufferHolder* bufferHolder;
	bufferHolder = (OWLVideoBufferHolder*) pVideoPicture->pSelf;
    bufferHolder->Release();
    pVideoPicture->pSelf = 0;
  }
  return true;
}


void COWLVideo::ReleaseBuffer(OWLVideoBuffer* releaseBuffer)
{
  if (!releaseBuffer)
    return;

  pthread_mutex_lock(&m_omx_queue_mutex);
  
  OWLVideoBuffer *buffer = releaseBuffer;

  bool done = !!(buffer->omx_buffer->nFlags & OMX_BUFFERFLAG_EOS);
  if (!done)
  {
    // return the omx buffer back to OpenMax to fill.
    OMX_ERRORTYPE omx_err = OMX_FillThisBuffer(m_omx_decoder, buffer->omx_buffer);
    if (omx_err)
      loge("%s::%s - OMX_FillThisBuffer, omx_err(0x%x)\n", CLASSNAME, __func__, omx_err);
  }
  pthread_mutex_unlock(&m_omx_queue_mutex);
}

int COWLVideo::GetPicture(VideoPicture* pVideoPicture)
{
  int returnCode = 0;

  pVideoPicture->pSelf = 0;

  if (!m_omx_output_ready.empty())
  {
    OWLVideoBuffer *buffer;
    // fetch a output buffer and pop it off the ready list
    pthread_mutex_lock(&m_omx_queue_mutex);
    buffer = m_omx_output_ready.front();
    m_omx_output_ready.pop();
    pthread_mutex_unlock(&m_omx_queue_mutex);

   if (buffer->omx_buffer->nFlags & OMX_BUFFERFLAG_DECODEONLY){
      // return the omx buffer back to OpenMax to fill.
      OMX_ERRORTYPE omx_err = OMX_FillThisBuffer(m_omx_decoder, buffer->omx_buffer);
      if (omx_err)
        loge("%s::%s - OMX_FillThisBuffer, omx_err(0x%x)\n",
          CLASSNAME, __func__, omx_err);
	  if (!m_dts_queue.empty())
      {
          m_dts_queue.pop();
      }
    }
    else {
      pVideoPicture->nDts = DVD_NOPTS_VALUE;
      pVideoPicture->nPts = DVD_NOPTS_VALUE;
      pVideoPicture->pSelf = new OWLVideoBufferHolder(buffer);

      if (!m_dts_queue.empty())
      {
          pVideoPicture->nDts = m_dts_queue.front();
          m_dts_queue.pop();
      }

      // nTimeStamp is in microseconds
      pVideoPicture->nPts = pVideoPicture->nDts;
	  returnCode |= VC_PICTURE;
    }

	
  }
  else
  {
  #if defined(OMX_DEBUG_VERBOSE)
    logw("%s::%s - called but m_omx_output_ready is empty\n",
      CLASSNAME, __func__);
  #endif
  }

  pVideoPicture->nFlags  = DVP_FLAG_ALLOCATED;
  pVideoPicture->nFlags |= m_drop_state ? DVP_FLAG_DROPPED : 0;

  return returnCode;
}


// DecoderEmptyBufferDone -- OpenMax input buffer has been emptied
OMX_ERRORTYPE COWLVideo::OnEmptyBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer)
{
  COWLVideo *ctx = static_cast<COWLVideo*>(pAppData);

  #if defined(OMX_DEBUG_EMPTYBUFFERDONE)
  logd("%s::%s - buffer_size(%lu), timestamp(%f)\n",
    CLASSNAME, __func__, pBuffer->nFilledLen, (double)pBuffer->nTimeStamp / 1000.0);
  #endif

  // queue free input buffer to avaliable list.
  pthread_mutex_lock(&m_omx_queue_mutex);
  ctx->m_omx_input_avaliable.push(pBuffer);
  if(!ctx->m_omx_input_avaliable.empty()) {
    if (!ctx->m_demux_queue.empty()) {
      omx_demux_packet demux_packet = m_demux_queue.front();
      ctx->m_demux_queue.pop();
      ctx->EnqueueDemuxPacket(demux_packet);
    }
    else {
      pthread_cond_signal(&m_omx_queue_available);
    }
  }

  pthread_mutex_unlock(&m_omx_queue_mutex);

  return OMX_ErrorNone;
}


// DecoderFillBufferDone -- OpenMax output buffer has been filled
OMX_ERRORTYPE COWLVideo::OnFillBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer)
{
  COWLVideo *ctx = static_cast<COWLVideo*>(pAppData);
  OWLVideoBuffer *buffer = (OWLVideoBuffer*)pBuffer->pAppPrivate;

  #if defined(OMX_DEBUG_FILLBUFFERDONE)
  logd("%s::%s - buffer_size(%lu), timestamp(%f)\n",
    CLASSNAME, __func__, pBuffer->nFilledLen, (double)pBuffer->nTimeStamp / 1000.0);
  #endif

  if (pBuffer->nFlags & OMX_BUFFERFLAG_DECODEONLY){
	  logw("%s::%s - OMX_BUFFERFLAG_DECODEONLY\n", CLASSNAME, __func__);
  }

  if (!ctx->m_portChanging)
  {
    // queue output omx buffer to ready list.
    pthread_mutex_lock(&m_omx_queue_mutex);
    ctx->m_omx_output_ready.push(buffer);
    pthread_mutex_unlock(&m_omx_queue_mutex);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE COWLVideo::PrimeFillBuffers(void)
{
  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OWLVideoBuffer *buffer;

  // tell OpenMax to start filling output buffers
  for (size_t i = 0; i < m_omx_output_buffers.size(); i++)
  {
    buffer = m_omx_output_buffers[i];
    // always set the port index.
    buffer->omx_buffer->nOutputPortIndex = m_omx_output_port;
    // Need to clear the EOS flag.
    buffer->omx_buffer->nFlags &= ~OMX_BUFFERFLAG_EOS;
    buffer->omx_buffer->pAppPrivate = buffer;

    omx_err = OMX_FillThisBuffer(m_omx_decoder, buffer->omx_buffer);
    if (omx_err)
      loge("%s::%s - OMX_FillThisBuffer failed with omx_err(0x%x)\n",
        CLASSNAME, __func__, omx_err);
  }

  return omx_err;
}

OMX_ERRORTYPE COWLVideo::AllocOMXInputBuffers(void)
{
  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  // Obtain the information about the decoder input port.
  OMX_PARAM_PORTDEFINITIONTYPE port_format;
  OMX_INIT_STRUCTURE(port_format);
  port_format.nPortIndex = m_omx_input_port;
  OMX_GetParameter(m_omx_decoder, OMX_IndexParamPortDefinition, &port_format);

  #if defined(OMX_DEBUG_VERBOSE)
  logd("%s::%s - iport(%d), nBufferCountMin(%lu), nBufferCountActual(%lu), nBufferSize(%lu)\n",
    CLASSNAME, __func__, m_omx_input_port, port_format.nBufferCountMin, port_format.nBufferCountActual, port_format.nBufferSize);
  #endif
  for (size_t i = 0; i < port_format.nBufferCountActual; i++)
  {
    OMX_BUFFERHEADERTYPE *buffer = NULL;
	
	omx_err =OMX_AllocateBuffer(m_omx_decoder, &buffer, m_omx_input_port, NULL, port_format.nBufferSize);
    if (omx_err)
    {
      loge("%s::%s - OMX_UseBuffer failed with omx_err(0x%x)\n",
        CLASSNAME, __func__, omx_err);
      return(omx_err);
    }
	buffer->nInputPortIndex = m_omx_input_port;
    buffer->nFilledLen      = 0;
    buffer->nOffset         = 0;
    m_omx_input_buffers.push_back(buffer);
    // don't have to lock/unlock here, we are not decoding
    m_omx_input_avaliable.push(buffer);
  }
  m_omx_input_eos = false;

  return(omx_err);
}
OMX_ERRORTYPE COWLVideo::FreeOMXInputBuffers(bool wait)
{
  OMX_ERRORTYPE omx_err = OMX_ErrorNone;

  // empty input buffer queue. not decoding so don't need lock/unlock.
  while (!m_omx_input_avaliable.empty())
    m_omx_input_avaliable.pop();

  // free omx input port buffers.
  for (size_t i = 0; i < m_omx_input_buffers.size(); i++)
  {
    omx_err = OMX_FreeBuffer(m_omx_decoder, m_omx_input_port, m_omx_input_buffers[i]);
  }
  m_omx_input_buffers.clear();

  while (!m_demux_queue.empty()){
  	delete[] m_demux_queue.front().buff;
    m_demux_queue.pop();
  }
  
  while (!m_dts_queue.empty())
    m_dts_queue.pop();

  return(omx_err);
}


OMX_ERRORTYPE COWLVideo::AllocOMXOutputBuffers(void)
{
  OMX_ERRORTYPE omx_err;
  OWLVideoBuffer *owl_buffer;

  // Obtain the information about the output port.
  OMX_PARAM_PORTDEFINITIONTYPE port_format;
  OMX_INIT_STRUCTURE(port_format);
  port_format.nPortIndex = m_omx_output_port;
  omx_err = OMX_GetParameter(m_omx_decoder, OMX_IndexParamPortDefinition, &port_format);

  logd("%s::%s (1) - oport(%d), nFrameWidth(%lu), nFrameHeight(%lu), nStride(%lx), nBufferCountMin(%lu),  nBufferCountActual(%lu), nBufferSize(%lu)\n",
    CLASSNAME, __func__, m_omx_output_port,
    port_format.format.video.nFrameWidth, port_format.format.video.nFrameHeight,port_format.format.video.nStride,
    port_format.nBufferCountMin, port_format.nBufferCountActual, port_format.nBufferSize);


  for (size_t i = 0; i < port_format.nBufferCountActual; i++)
  {
     logd("%s::%s i ==%d m_decoded_width =%d m_decoded_height =%d\n", 
	 	CLASSNAME, __func__, i,m_decoded_width,m_decoded_height);
  
    OMX_BUFFERHEADERTYPE *buffer = NULL;

    owl_buffer = new OWLVideoBuffer;
    owl_buffer->SetOWLVideo(this);
    owl_buffer->width  = m_decoded_width;
    owl_buffer->height = m_decoded_height;

	omx_err = OMX_AllocateBuffer(m_omx_decoder, &buffer, m_omx_output_port, owl_buffer, port_format.nBufferSize);
	if(omx_err != OMX_ErrorNone)
	{
      loge(" %s::%s OMX_AllocateBuffer failed with omx_err(0x%x)\n", \
	  	CLASSNAME, __func__, omx_err);

      return omx_err;
    }
	owl_buffer->omx_buffer = buffer;

    m_omx_output_buffers.push_back(owl_buffer);

  }
  m_omx_output_eos = false;
  while (!m_omx_output_ready.empty())
    m_omx_output_ready.pop();

  return omx_err;
}

OMX_ERRORTYPE COWLVideo::FreeOMXOutputBuffers(bool wait)
{
  OMX_ERRORTYPE omx_err;

  while(!m_omx_output_ready.empty())
  {
    m_omx_output_ready.pop();
  }

  for (size_t i = 0; i < m_omx_output_buffers.size(); i++)
  {
    OWLVideoBuffer* owl_buffer = m_omx_output_buffers[i];
    // tell decoder output port to stop using the EGLImage
    omx_err = OMX_FreeBuffer(m_omx_decoder, m_omx_output_port, owl_buffer->omx_buffer);
    owl_buffer->Release();
  }
  
  m_omx_output_buffers.clear();

  return omx_err;
}

OMX_ERRORTYPE COWLVideo::OnEventHandler(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 nData1,
  OMX_U32 nData2,
  OMX_PTR pEventData)
{
  OMX_ERRORTYPE omx_err;
  COWLVideo *ctx = static_cast<COWLVideo*>(pAppData);

  logd("COpenMax::%s - hComponent(0x%p), eEvent(0x%x), nData1(0x%lx), nData2(0x%lx), pEventData(0x%p)\n",
    __func__, hComponent, eEvent, nData1, nData2, pEventData);


  switch (eEvent)
  {
    case OMX_EventCmdComplete:
	{
	  onCmdComplete(pAppData,(OMX_COMMANDTYPE)nData1, nData2);
	  break;
    }
    case OMX_EventPortSettingsChanged:
	  logw("%s::%s OMX_EventPortSettingsChanged(port=%u, data2=0x%08x)",
				   CLASSNAME, __func__,nData1, nData2);

	  if (nData2 == 0 || nData2 == OMX_IndexParamPortDefinition) {
	  	ctx->m_portChanging = true;
		onPortSettingsChanged(nData1);
	  } 
    break;

    case OMX_EventError:
	  logw("OMX_EventError(0x%08x, %u)", nData1, nData2);
      setState(ERROR);
      sem_post(&ctx->m_omx_decoder_state_change);
    break;
    default:
      logw("%s::%s - Unknown eEvent(0x%x), nData1(0x%lx), nData2(0x%lx)\n",
        CLASSNAME, __func__, eEvent, nData1, nData2);
    break;
  }

  return OMX_ErrorNone;
}


// StartPlayback -- Kick off video playback.
OMX_ERRORTYPE COWLVideo::StartDecoder(void)
{
  OMX_ERRORTYPE omx_err;

  logd("%s::%s - waiting for state(%d) in\n", CLASSNAME, __func__, mState);
  int tries= 0;
  while (mState != EXECUTING && mState != ERROR)
  {
    if (tries < 5){	
	    struct timespec timeout;

        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1;
        sem_timedwait(&m_omx_decoder_state_change, &timeout);

		tries++;
    }else{
        omx_err = OMX_ErrorUndefined;
        break;
    }
  }
  logd("%s::%s - waiting for state(%d) out\n", CLASSNAME, __func__, mState);

  //prime the omx output buffers.
  omx_err = PrimeFillBuffers();

  return omx_err;
}

// StopPlayback -- Stop video playback
OMX_ERRORTYPE COWLVideo::StopDecoder(void)
{
  OMX_ERRORTYPE omx_err;

   logd("%s::% stop mState=%d\n", CLASSNAME, __func__,mState);
   omx_err = stopOmxComponent_l();

  // free all queues demux packets
  ReleaseDemuxQueue();

  return omx_err;
}

void COWLVideo::ReleaseDemuxQueue(void)
{
  while(!m_demux_queue.empty()) {
    delete[] m_demux_queue.front().buff;
    m_demux_queue.pop();
  }
}


// DecoderEventHandler -- OMX event callback
OMX_ERRORTYPE COWLVideo::DecoderEventHandler(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 nData1,
  OMX_U32 nData2,
  OMX_PTR pEventData)
{
  logw("COpenMax::%s - hComponent(0x%p), eEvent(0x%x), nData1(0x%lx), nData2(0x%lx), pEventData(0x%p)\n",
    __func__, hComponent, eEvent, nData1, nData2, pEventData);

  pthread_mutex_lock(&m_event_queue_mutex);
  OMXMessage* message = new OMXMessage;
  message->type = OMXMessage::EVENT;
  message->u.event_data.hComponent = hComponent;
  message->u.event_data.pAppData = pAppData;
  message->u.event_data.eEvent = eEvent;
  message->u.event_data.nData1 = nData1;
  message->u.event_data.nData2 = nData2;
  message->u.event_data.pEventData = pEventData;

  m_omx_message.push(message);

  logd("COWLVideo::%s out\n",__func__);
  pthread_cond_signal(&m_event_queue_available);
  pthread_mutex_unlock(&m_event_queue_mutex);

  return OMX_ErrorNone;
}


OMX_ERRORTYPE COWLVideo::DecoderEmptyBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer)

{
	COWLVideo *ctx = static_cast<COWLVideo*>(pAppData);
	
#if defined(OMX_DEBUG_EMPTYBUFFERDONE)
	logd("%s::%s - buffer_size(%lu), timestamp(%f)\n",
	  CLASSNAME, __func__, pBuffer->nFilledLen, (double)pBuffer->nTimeStamp / 1000.0);
#endif


  pthread_mutex_lock(&m_event_queue_mutex);
  OMXMessage* message = new OMXMessage;
  message->type = OMXMessage::EMPTY_BUFFER_DONE;
  message->u.buffer_data.hComponent = hComponent;
  message->u.buffer_data.pAppData = pAppData;
  message->u.buffer_data.pBuffer = pBuffer;
  m_omx_message.push(message);

  pthread_cond_signal(&m_event_queue_available);
  pthread_mutex_unlock(&m_event_queue_mutex);

  return OMX_ErrorNone;
}



// DecoderFillBufferDone -- OpenMax output buffer has been filled
OMX_ERRORTYPE COWLVideo::DecoderFillBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer)
{
  COWLVideo *ctx = static_cast<COWLVideo*>(pAppData);
  OWLVideoBuffer *buffer = (OWLVideoBuffer*)pBuffer->pAppPrivate;

  #if defined(OMX_DEBUG_FILLBUFFERDONE)
  logd("%s::%s - buffer_size(%lu), timestamp(%f)\n",
    CLASSNAME, __func__, pBuffer->nFilledLen, (double)pBuffer->nTimeStamp / 1000.0);
  #endif

  pthread_mutex_lock(&m_event_queue_mutex);
  OMXMessage* message = new OMXMessage;
  message->type = OMXMessage::FILL_BUFFER_DONE;
  message->u.buffer_data.hComponent = hComponent;
  message->u.buffer_data.pAppData = pAppData;
  message->u.buffer_data.pBuffer = pBuffer;
  m_omx_message.push(message);

  pthread_cond_signal(&m_event_queue_available);
  pthread_mutex_unlock(&m_event_queue_mutex);

  return OMX_ErrorNone;
}


OWLVideoBufferHolder::OWLVideoBufferHolder(OWLVideoBuffer *OWLVideoBuffer)
{
  m_OWLVideoBuffer = OWLVideoBuffer;
  if (m_OWLVideoBuffer) {
	  m_OWLVideoBuffer->Acquire();
  }
}

OWLVideoBufferHolder::~OWLVideoBufferHolder()
{
  if (m_OWLVideoBuffer) {
    m_OWLVideoBuffer->PassBackToRenderer();
    m_OWLVideoBuffer->Release();
  }
}

OWLVideoBuffer::OWLVideoBuffer()
{
  m_OWLVideo = 0;
}

OWLVideoBuffer::~OWLVideoBuffer()
{
  if (m_OWLVideo) {
    m_OWLVideo->Release();
  }
}

void OWLVideoBuffer::SetOWLVideo(COWLVideo *OWLVideo)
{
  if (m_OWLVideo) {
    m_OWLVideo->Release();
  }

  m_OWLVideo = OWLVideo;

  if (m_OWLVideo) {
    m_OWLVideo->Acquire();
  }
}

void OWLVideoBuffer::PassBackToRenderer()
{
  m_OWLVideo->ReleaseBuffer(this);
}



