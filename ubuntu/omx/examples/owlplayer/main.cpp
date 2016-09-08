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

#include "OWLVdecoder.h"
#include <unistd.h>
extern "C"{
#include <libavformat/avformat.h>
}

#include "log.h"

typedef struct {
	AVFormatContext *ifc;
	AVStream *st_video;
	AVStream *st_audio;
	VideoStreamInfo m_hint;
	float dur;	
	float pos;

} Demuxer_t;

#define M(_m) ((Demuxer_t *)_m)

static int debug = 1;

#define dbp(lev, ...) { \
	if (debug) \
		printf("dec: " __VA_ARGS__);	\
	}

static void _init() 
{
	static int a=0;
	if (a++)
		return ;

	av_register_all();
	av_log_set_level(AV_LOG_ERROR);
}


static FILE* pfp = NULL;
static void _dump_NV12(VideoPicture *picture, char *prefix)
{
	char path[256];
	int i;

    if (picture->eFormat == FMT_NV12){
		int h = picture->nHeight;
		int w = picture->nWidth;
		int *linesize = picture->nLineStride;

		if (pfp == NULL)
		{
			sprintf(path, "%s/%s_w%d_h%d", prefix, "NV12",w,h);
			pfp = fopen(path, "wb+");
		}
		for (i = 0; i < 2; i++) {
            if (i == 1)
            {
              h = h >> 1;
            }
			fwrite(picture->pData[i], 1, linesize[i]*h, pfp);
			
		}
    }
}


void *Demuxer_open(char *fname)
{

	_init();

	Demuxer_t *m = (Demuxer_t*)malloc(sizeof(Demuxer_t));
	AVCodec *c;

	memset(m, 0, sizeof(Demuxer_t));

	int i;
	dbp(0, "open %s\n", fname);
	i = avformat_open_input(&m->ifc, fname, NULL, NULL);
	if (i) {
		dbp(0, "  open %s failed err %d\n", fname, i);
		free(m);
		return NULL;
	}

	dbp(1, "  nb_streams: %d\n", m->ifc->nb_streams);
	avformat_find_stream_info(m->ifc, NULL);
	for (i = 0; i < m->ifc->nb_streams; i++) {
		AVStream *st = m->ifc->streams[i];
		c = avcodec_find_decoder(st->codec->codec_id);	
		if (!c)	
			continue;
		if (st->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			dbp(1, "  audio: %s\n", c->name);
			m->st_audio = st;
		}
		if (st->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			dbp(1, "  video: %s\n", c->name);
			m->st_video = st;
		}
	}


	if (!m->st_video && !m->st_audio) {
		dbp(0, "  both video and audio stream not found\n");
		free(m);
		return NULL;
	}

	m->dur = m->ifc->duration/1e6;
	dbp(0, "  duration: %f s\n", m->dur);

	if (!m->st_video){
		free(m);
		return NULL;
	}

	AVStream* pStream =m->st_video;

	if( pStream->codec->extradata && pStream->codec->extradata_size > 0 )
    {
      m->m_hint.extrasize = pStream->codec->extradata_size;
      m->m_hint.extradata= new uint8_t[m->m_hint.extrasize];
      memcpy(m->m_hint.extradata, pStream->codec->extradata, pStream->codec->extradata_size);
    }

	m->m_hint.codec = pStream->codec->codec_id;
	m->m_hint.width = pStream->codec->width;
	m->m_hint.height = pStream->codec->height;

    sprintf(m->m_hint.filename,"%s",fname);

	avformat_seek_file(m->ifc, 0, 0, 0, 0, 0);

	return m;
}


void Demuxer_close(void *pDexmuxer)
{
	Demuxer_t* p = (Demuxer_t*)pDexmuxer;

	if (p->m_hint.extradata)
	{
		delete[] (uint8_t*)p->m_hint.extradata;
	}

	avformat_close_input(&p->ifc);

    free (p);
}


int main(int argc, char *argv[])

{
    char fname[128]= "\0";

	if (argc != 2) return 0;

	sprintf(fname,"%s",argv[1]);

	dbp(0, "  fileName:%s\n",fname);

	Demuxer_t* pDemuxer;
	CVideoCodecOWL* pDecoder;

	pDemuxer = (Demuxer_t*)Demuxer_open(fname);

	if (pDemuxer == NULL)
	{
        loge("demuxer open err \n");
		return -1;
	}

	pDecoder = new CVideoCodecOWL;

    if (!pDecoder->Open(pDemuxer->m_hint))
    {
      printf("%s - failed to open, codec(%d)",  __func__, pDemuxer->m_hint.codec);
      return -1;
    }

    int rtn = 0;
	VideoPicture pVideoPicture;
	int writeCnt = 50;
	while (1) {
		AVPacket pkt;
		if (av_read_frame(pDemuxer->ifc, &pkt)) {
			av_free_packet(&pkt);
			break;
		} 

		if (pDemuxer->st_video && pkt.stream_index == pDemuxer->st_video->index
			&& pkt.size > 0)
		{
		    if (pkt.dts == AV_NOPTS_VALUE) pkt.dts = DVD_NOPTS_VALUE;
			if (pkt.pts == AV_NOPTS_VALUE) pkt.pts = DVD_NOPTS_VALUE;

			if (pkt.data){
				rtn = pDecoder->Decode(pkt.data, pkt.size, pkt.dts, pkt.pts);
			}

			if (rtn& VC_ERROR)
        	{
          		printf("video decoder returned error");
          		break;
        	}

        	// check for a new picture
        	if (rtn & VC_PICTURE)
        	{
				if (pDecoder->GetPicture(&pVideoPicture))
				{
				    if (writeCnt > 0 ){
                    	_dump_NV12(&pVideoPicture, (char*)"/home");
						writeCnt --;
				    }
				    pDecoder->ClearPicture(&pVideoPicture);
				}	
			}
		}
		av_free_packet(&pkt);
	}

	logw("\n player exit\n");

	pDecoder->Dispose();
	Demuxer_close(pDemuxer);
	if (pfp) fclose(pfp);
	return 1;
}

