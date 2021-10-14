#include <string.h>
#include "rtp_profile.h"

static const rtp_profile_t s_profiles[] = {
	// audio
	{ 0, AVMEDIA_TYPE_AUDIO,	1, 8000,	"PCMU" }, // G711 mu-law
	{ 2, AVMEDIA_TYPE_UNKNOWN,	1, 8000,	"G721" }, // reserved
	{ 3, AVMEDIA_TYPE_AUDIO,	1, 8000,	"GSM"  },
	{ 4, AVMEDIA_TYPE_AUDIO,	1, 8000,	"G723" },
	{ 5, AVMEDIA_TYPE_AUDIO,	1, 8000,	"DVI4" },
	{ 6, AVMEDIA_TYPE_AUDIO,	1, 16000,	"DVI4" },
	{ 7, AVMEDIA_TYPE_AUDIO,	1, 8000,	"LPC"  },
	{ 8, AVMEDIA_TYPE_AUDIO,	1, 8000,	"PCMA" }, // G711 A-law
	{ 9, AVMEDIA_TYPE_AUDIO,	1, 8000,	"G722" },
	{ 10,AVMEDIA_TYPE_AUDIO,	2, 44100,	"L16"  }, // PCM S16BE
	{ 11,AVMEDIA_TYPE_AUDIO,	1, 44100,	"L16"  }, // PCM S16BE
	{ 12,AVMEDIA_TYPE_AUDIO,	1, 8000,	"QCELP"},
	{ 13,AVMEDIA_TYPE_AUDIO,	1, 8000,	"CN"   },
	{ 14,AVMEDIA_TYPE_AUDIO,	2, 90000,	"MPA"  }, // MPEG-1/MPEG-2 audio 1/2 channels
	{ 15,AVMEDIA_TYPE_AUDIO,	1, 8000,	"G728" },
	{ 16,AVMEDIA_TYPE_AUDIO,	1, 11025,	"DVI4" },
	{ 17,AVMEDIA_TYPE_AUDIO,	1, 22050,	"DVI4" },
	{ 18,AVMEDIA_TYPE_AUDIO,	1, 8000,	"G729" },

	// video
	{ 25,AVMEDIA_TYPE_VIDEO,	0, 90000,	"CELB" }, // SUN CELL-B
	{ 26,AVMEDIA_TYPE_VIDEO,	0, 90000,	"JPEG" }, // MJPEG
	{ 28,AVMEDIA_TYPE_VIDEO,	0, 90000,	"nv"   },
	{ 31,AVMEDIA_TYPE_VIDEO,	0, 90000,	"H261" },
	{ 32,AVMEDIA_TYPE_VIDEO,	0, 90000,	"MPV"  }, // MPEG-1/MPEG-2 video
	{ 34,AVMEDIA_TYPE_VIDEO,	0, 90000,	"H263" },
	//{ 0, "H263-1998",90000,	0 },

	// 35-71 unassigned
	// 72-76 reserved
	// 77-95 unassigned
	// 96-127 dynamic
	//{ 96,AVMEDIA_TYPE_VIDEO,	0, 90000,	"MPG4" }, // RFC3640 RTP Payload Format for Transport of MPEG-4 Elementary Streams
	//{ 97,AVMEDIA_TYPE_SYSTEM,	0, 90000,	"MP2P" }, // RFC3555 4.2.11 Registration of MIME media type video/MP2P
	//{ 98,AVMEDIA_TYPE_VIDEO,	0, 90000,	"H264" }, // RFC6184 RTP Payload Format for H.264 Video
};

const rtp_profile_t* rtp_profile_find(int payload_type)
{
	int i = 0;
    for (i = 0; sizeof(s_profiles)/sizeof(rtp_profile_t); i++) {
        if (s_profiles[i].pt == payload_type) {
            return &s_profiles[i];
        }
    }
    return NULL;
}
