#include"OWLVdecoder.h"
#include "owl_private.h"
#include "OWLCodec.h"
#include "log.h"

#define CLASSNAME "OWLVdecoder"

CVideoCodecOWL::CVideoCodecOWL()
{
  m_omx_decoder = NULL;
  m_convert_bitstream = false;

  memset(&m_videobuffer, 0, sizeof(VideoPicture));
}

CVideoCodecOWL::~CVideoCodecOWL()
{
  Dispose();
}

bool CVideoCodecOWL::Open(VideoStreamInfo &hints)
{

	m_convert_bitstream = false;
	  
	switch (hints.codec)
	{
		case AV_CODEC_ID_H264:
		{
		  if (hints.extrasize < 7 || hints.extradata == NULL)
		  {
			logw("%s::%s - avcC data too small or missing", CLASSNAME, __func__);
			return false;
		  }
		  // valid avcC data (bitstream) always starts with the value 1 (version)
		  if ( *(char*)hints.extradata == 1 )
			m_convert_bitstream = bitstream_convert_init(hints.extradata, hints.extrasize);
		}
		break;
	    default:
			break;

	}
    m_omx_decoder = new COWLVideo;
    if (!m_omx_decoder->Open(hints))
    {
      loge("%s::%s - failed to open, codec(%d)", CLASSNAME, __func__, hints.codec);
      return false;
    }

    memset(&m_videobuffer, 0, sizeof(VideoPicture));

    m_videobuffer.nDts = DVD_NOPTS_VALUE;
    m_videobuffer.nPts = DVD_NOPTS_VALUE;
    m_videobuffer.eFormat = FMT_NV12;
    m_videobuffer.nFlags  = DVP_FLAG_ALLOCATED;
    m_videobuffer.nWidth  = hints.width;
    m_videobuffer.nHeight = hints.height;

	ConfigureOutputFormat(hints);

    return true;
}

void CVideoCodecOWL::ConfigureOutputFormat(VideoStreamInfo &hints)
{
  OMXOutputFormat* output;
  output = m_omx_decoder->getOutFormat();
  
  int width       = hints.width;//output->width;
  int height      = hints.height;//output->height;
  int stride      = output->stride;
  int slice_height= output->slice_height;
  int color_format= output->color_format;
  int crop_left   = output->crop_left;
  int crop_top    = output->crop_top;
  int crop_right  = output->crop_right;
  int crop_bottom = output->crop_bottom;


  logd("%s::width(%d), height(%d), stride(%d), slice-height(%d), color-format(%d)",
    CLASSNAME,width, height, stride, slice_height, color_format);
  logd("%s:: "
    "crop-left(%d), crop-top(%d), crop-right(%d), crop-bottom(%d)",CLASSNAME,
    crop_left, crop_top, crop_right, crop_bottom);

  {

    // No color-format? Initialize with the one we detected as valid earlier
    if (color_format == 0)
      color_format = OMX_COLOR_FormatYUV420SemiPlanar;
    if (stride <= width)
      stride = width;
    if (!crop_right)
      crop_right = width-1;
    if (!crop_bottom)
      crop_bottom = height-1;
    if (slice_height <= height)
    {
      slice_height = height;
    }

    // default picture format to none
    for (int i = 0; i < 4; i++)
      m_src_offset[i] = m_src_stride[i] = 0;

    // setup picture format and data offset vectors
    if(color_format == OMX_COLOR_FormatYUV420SemiPlanar)
    {
      logd("%s::%s OMX_COLOR_FormatYUV420SemiPlanar",CLASSNAME, __func__);

      // Y plane
      m_src_stride[0] = stride;
      m_src_offset[0] = crop_top * stride;
      m_src_offset[0]+= crop_left;

      // UV plane
      m_src_stride[1] = stride;
      //  skip over the Y plane
      m_src_offset[1] = slice_height * stride;
      m_src_offset[1]+= crop_top * stride;
      m_src_offset[1]+= crop_left;
      m_videobuffer.eFormat = FMT_NV12;
    }
    else
    {
    
      loge("%s:: Fixme unknown color_format(%d)",CLASSNAME, color_format);
      return;
    }
  }

  if (width)
    m_videobuffer.nWidth  = width;
  if (height)
    m_videobuffer.nHeight = height;


}

void CVideoCodecOWL::Dispose()
{
  logd("%s::%s in",CLASSNAME, __func__);

  if (m_omx_decoder)
  {
    m_omx_decoder->Close();
    m_omx_decoder->Release();
    m_omx_decoder = NULL;
  }

   if (m_convert_bitstream)
  {
    if (m_sps_pps_context.sps_pps_data)
    {
      free(m_sps_pps_context.sps_pps_data);
      m_sps_pps_context.sps_pps_data = NULL;
    }
  }
  
  logd("%s::%s out",CLASSNAME, __func__);
}



int CVideoCodecOWL::Decode(uint8_t* pData, int iSize, double dts, double pts)
{
  if (pData)
  {
    int rtn;
    int demuxer_bytes = iSize;
    uint8_t *demuxer_content = pData;

    bool bitstream_convered  = false;

    if (m_convert_bitstream)
    {
      // convert demuxer packet from bitstream to bytestream (AnnexB)
      int bytestream_size = 0;
      uint8_t *bytestream_buff = NULL;

      bitstream_convert(demuxer_content, demuxer_bytes, &bytestream_buff, &bytestream_size);
      if (bytestream_buff && (bytestream_size > 0))
      {
        bitstream_convered = true;
        demuxer_bytes = bytestream_size;
        demuxer_content = bytestream_buff;
      }
    }

    rtn = m_omx_decoder->Decode(demuxer_content, demuxer_bytes, dts, pts);

    if (bitstream_convered)
      free(demuxer_content);
	
    return rtn;
  }
  
  return m_omx_decoder->Decode(0, 0, 0, 0);
}


bool CVideoCodecOWL::GetPicture(VideoPicture* pVideoPicture)
{
  int returnCode = 0;

  m_videobuffer.nDts = DVD_NOPTS_VALUE;
  m_videobuffer.nPts = DVD_NOPTS_VALUE;

  returnCode = m_omx_decoder->GetPicture(&m_videobuffer);

  
  if (returnCode & VC_PICTURE){
	  OWLVideoBufferHolder *bufferHolder = (OWLVideoBufferHolder*)(m_videobuffer.pSelf);
      OWLVideoBuffer *buffer = bufferHolder->m_OWLVideoBuffer;
  
	  char *src_ptr = (char*)buffer->omx_buffer->pBuffer;
	  int offset = buffer->omx_buffer->nOffset;
		  
	  src_ptr += offset;

	  m_videobuffer.eFormat = FMT_NV12;
	  
	  char *src_0   = src_ptr + m_src_offset[0];
	  m_videobuffer.nLineStride[0] = m_src_stride[0];
      m_videobuffer.pData[0]= src_0;

	  char *src_1	 = src_ptr + m_src_offset[1];
	  m_videobuffer.nLineStride[1] = m_src_stride[1];
	  m_videobuffer.pData[1]= src_1;
	  
	  *pVideoPicture = m_videobuffer;
	  return true;
  }

  return false;
}

bool CVideoCodecOWL::ClearPicture(VideoPicture* pVideoPicture)
{
    if (pVideoPicture->pSelf){
	    m_omx_decoder->ClearPicture(pVideoPicture);
    }
    memset(pVideoPicture, 0, sizeof(VideoPicture));
    return true;
}


////////////////////////////////////////////////////////////////////////////////////////////
bool CVideoCodecOWL::bitstream_convert_init(void *in_extradata, int in_extrasize)
{
  // based on h264_mp4toannexb_bsf.c (ffmpeg)
  // which is Copyright (c) 2007 Benoit Fouet <benoit.fouet@free.fr>
  // and Licensed GPL 2.1 or greater

  m_sps_pps_size = 0;
  m_sps_pps_context.sps_pps_data = NULL;
  
  // nothing to filter
  if (!in_extradata || in_extrasize < 6)
    return false;

  uint16_t unit_size;
  uint32_t total_size = 0;
  uint8_t *out = NULL, unit_nb, sps_done = 0;
  const uint8_t *extradata = (uint8_t*)in_extradata + 4;
  static const uint8_t nalu_header[4] = {0, 0, 0, 1};

  // retrieve length coded size
  m_sps_pps_context.length_size = (*extradata++ & 0x3) + 1;
  if (m_sps_pps_context.length_size == 3)
    return false;

  // retrieve sps and pps unit(s)
  unit_nb = *extradata++ & 0x1f;  // number of sps unit(s)
  if (!unit_nb)
  {
    unit_nb = *extradata++;       // number of pps unit(s)
    sps_done++;
  }
  while (unit_nb--)
  {
    unit_size = extradata[0] << 8 | extradata[1];
    total_size += unit_size + 4;
    if ( (extradata + 2 + unit_size) > ((uint8_t*)in_extradata + in_extrasize) )
    {
      free(out);
      return false;
    }
    uint8_t* new_out = (uint8_t*)realloc(out, total_size);
    if (new_out)
    {
      out = new_out;
    }
    else
    {
      loge("bitstream_convert_init failed - %s : could not realloc the buffer out",  __FUNCTION__);
      free(out);
      return false;
    }

    memcpy(out + total_size - unit_size - 4, nalu_header, 4);
    memcpy(out + total_size - unit_size, extradata + 2, unit_size);
    extradata += 2 + unit_size;

    if (!unit_nb && !sps_done++)
      unit_nb = *extradata++;     // number of pps unit(s)
  }

  m_sps_pps_context.sps_pps_data = out;
  m_sps_pps_context.size = total_size;
  m_sps_pps_context.first_idr = 1;

  return true;
}

bool CVideoCodecOWL::bitstream_convert(uint8_t* pData, int iSize, uint8_t **poutbuf, int *poutbuf_size)
{
  // based on h264_mp4toannexb_bsf.c (ffmpeg)
  // which is Copyright (c) 2007 Benoit Fouet <benoit.fouet@free.fr>
  // and Licensed GPL 2.1 or greater

  uint8_t *buf = pData;
  uint32_t buf_size = iSize;
  uint8_t  unit_type;
  int32_t  nal_size;
  uint32_t cumul_size = 0;
  const uint8_t *buf_end = buf + buf_size;

  do
  {
    if (buf + m_sps_pps_context.length_size > buf_end)
      goto fail;

    if (m_sps_pps_context.length_size == 1)
      nal_size = buf[0];
    else if (m_sps_pps_context.length_size == 2)
      nal_size = buf[0] << 8 | buf[1];
    else
      nal_size = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];

    buf += m_sps_pps_context.length_size;
    unit_type = *buf & 0x1f;

    if (buf + nal_size > buf_end || nal_size < 0)
      goto fail;

    // prepend only to the first type 5 NAL unit of an IDR picture
    if (m_sps_pps_context.first_idr && unit_type == 5)
    {
      bitstream_alloc_and_copy(poutbuf, poutbuf_size,
        m_sps_pps_context.sps_pps_data, m_sps_pps_context.size, buf, nal_size);
      m_sps_pps_context.first_idr = 0;
    }
    else
    {
      bitstream_alloc_and_copy(poutbuf, poutbuf_size, NULL, 0, buf, nal_size);
      if (!m_sps_pps_context.first_idr && unit_type == 1)
          m_sps_pps_context.first_idr = 1;
    }

    buf += nal_size;
    cumul_size += nal_size + m_sps_pps_context.length_size;
  } while (cumul_size < buf_size);

  return true;

fail:
  free(*poutbuf);
  *poutbuf = NULL;
  *poutbuf_size = 0;
  return false;
}

void CVideoCodecOWL::bitstream_alloc_and_copy(
  uint8_t **poutbuf,      int *poutbuf_size,
  const uint8_t *sps_pps, uint32_t sps_pps_size,
  const uint8_t *in,      uint32_t in_size)
{
  // based on h264_mp4toannexb_bsf.c (ffmpeg)
  // which is Copyright (c) 2007 Benoit Fouet <benoit.fouet@free.fr>
  // and Licensed GPL 2.1 or greater

  #define CHD_WB32(p, d) { \
    ((uint8_t*)(p))[3] = (d); \
    ((uint8_t*)(p))[2] = (d) >> 8; \
    ((uint8_t*)(p))[1] = (d) >> 16; \
    ((uint8_t*)(p))[0] = (d) >> 24; }

  uint32_t offset = *poutbuf_size;
  uint8_t nal_header_size = offset ? 3 : 4;

  *poutbuf_size += sps_pps_size + in_size + nal_header_size;
  *poutbuf = (uint8_t*)realloc(*poutbuf, *poutbuf_size);
  if (sps_pps)
    memcpy(*poutbuf + offset, sps_pps, sps_pps_size);

  memcpy(*poutbuf + sps_pps_size + nal_header_size + offset, in, in_size);
  if (!offset)
  {
    CHD_WB32(*poutbuf + sps_pps_size, 1);
  }
  else
  {
    (*poutbuf + offset + sps_pps_size)[0] = 0;
    (*poutbuf + offset + sps_pps_size)[1] = 0;
    (*poutbuf + offset + sps_pps_size)[2] = 1;
  }
}



