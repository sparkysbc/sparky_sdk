/*
* OWL Video decoder framework .
* Copyright (c) Actions(Zhuhai) Technology Co. Ltd.
* Copyright (c) 2015 SY Lee
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*/
#ifndef OWL_VDECODER_H
#define OWL_VDECODER_H

extern "C"{

#ifdef __cplusplus
	#ifndef __STDC_CONSTANT_MACROS
	#define __STDC_CONSTANT_MACROS
	#endif

	#ifndef __STDC_LIMIT_MACROS
	#define __STDC_LIMIT_MACROS
	#endif

	#ifdef _STDINT_H
  		#undef _STDINT_H
 	#endif
 	#include <stdint.h>

	#ifndef INT64_C
	#define INT64_C
	#define UINT64_C
	#endif
#endif

#include "libavcodec/avcodec.h"
}

#define DVD_NOPTS_VALUE    (-1LL<<52)
// VC_ messages, messages can be combined
#define VC_ERROR    0x00000001  // an error occured, no other messages will be returned
#define VC_BUFFER   0x00000002  // the decoder needs more data
#define VC_PICTURE  0x00000004  // the decoder got a picture, call Decode(NULL, 0) again to parse the rest of the data
#define VC_USERDATA 0x00000008  // the decoder found some userdata,  call Decode(NULL, 0) again to parse the rest of the data
#define VC_FLUSHED  0x00000010  // the decoder lost it's state, we need to restart decoding again
#define VC_DROPPED  0x00000020  // needed to identify if a picture was dropped
#define VC_NOBUFFER 0x00000040  // last FFmpeg GetBuffer failed


#define DVP_FLAG_TOP_FIELD_FIRST    0x00000001
#define DVP_FLAG_REPEAT_TOP_FIELD   0x00000002 //Set to indicate that the top field should be repeated
#define DVP_FLAG_ALLOCATED          0x00000004 //Set to indicate that this has allocated data
#define DVP_FLAG_INTERLACED         0x00000008 //Set to indicate that this frame is interlaced

#define DVP_FLAG_NOSKIP             0x00000010 // indicate this picture should never be dropped
#define DVP_FLAG_DROPPED            0x00000020 // indicate that this picture has been dropped in decoder stage, will have no data


typedef struct VIDEOSTREAMINFO
{
	AVCodecID codec;
	char filename[128];

	int height; // height of the stream reported by the demuxer
	int width; // width of the stream reported by the demuxer
	float aspect; // display aspect as reported by demuxer
	bool ptsinvalid;  // pts cannot be trusted (avi's).
	bool forced_aspect; // aspect is forced from container
	int bitsperpixel;
	void*		 extradata; // extra data for codec to use
	unsigned int extrasize; // size of extra data
}VideoStreamInfo;


enum EFormat {
  FMT_YUV420P = 0,
  FMT_NV12,
  FMT_UYVY,
  FMT_YUY2,
};

typedef struct VIDEOPICTURE
{
    EFormat eFormat;
    unsigned int nFlags;
    unsigned int nWidth;
    unsigned int nHeight;
    int64_t nPts;
    int64_t nDts;
    int     nLineStride[4]; 
    char*   pData[4];
    void*   pSelf;
}VideoPicture;

class COWLVideo;
class CVideoCodecOWL 
{
public:
  CVideoCodecOWL();
  virtual ~CVideoCodecOWL();

  // Required overrides
  virtual bool Open(VideoStreamInfo &hints);
  virtual void Dispose(void);

  //retval:
  virtual int  Decode(uint8_t *pData, int iSize, double dts, double pts);
  virtual bool GetPicture(VIDEOPICTURE *pVideoPicture);
  virtual bool ClearPicture(VIDEOPICTURE* pVideoPicture);

private:
  void ConfigureOutputFormat(VideoStreamInfo &hints);
  COWLVideo     *m_omx_decoder;
  VideoPicture   m_videobuffer;
  int            m_src_offset[4];
  int            m_src_stride[4];

	// bitstream to bytestream (Annex B) conversion support.
	bool bitstream_convert_init(void *in_extradata, int in_extrasize);
	bool bitstream_convert(uint8_t* pData, int iSize, uint8_t **poutbuf, int *poutbuf_size);
	static void bitstream_alloc_and_copy( uint8_t **poutbuf, int *poutbuf_size,
  	const uint8_t *sps_pps, uint32_t sps_pps_size, const uint8_t *in, uint32_t in_size);

typedef struct omx_bitstream_ctx {
	uint8_t  length_size;
	uint8_t  first_idr;
	uint8_t *sps_pps_data;
	uint32_t size;

	omx_bitstream_ctx()
	{
	  length_size = 0;
	  first_idr = 0;
	  sps_pps_data = NULL;
	  size = 0;
	}

} omx_bitstream_ctx;

uint32_t		  m_sps_pps_size;
omx_bitstream_ctx m_sps_pps_context;
bool			  m_convert_bitstream;

};



#endif

