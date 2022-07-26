/***************************************************************************
 *   Copyright (C) 2022+ by Ekrem Karacan                                  *
 *   Ekrem.Karacan@gmail.com                                               *
 ***************************************************************************/

#define	snifferplayer_c

#include	<string.h>
#include	<pcap.h>
#include	<arpa/inet.h>
#include	<netinet/in.h>
#include	<net/ethernet.h>
#include	<pthread.h>
#include	<unistd.h>
#include	<sys/resource.h>
#include	<sys/sysinfo.h>
#include	<pthread.h>
#include	<semaphore.h>

#include	"bcm_host.h"
#include	"ilclient.h"

int	gJitter = 0; //1000000/24;

#define			MAX_PACKETS	(256 * 8)
#define			MAX_PACKET	10000

typedef	struct {
	int							w, h;
	DISPMANX_DISPLAY_HANDLE_T   display;
	DISPMANX_MODEINFO_T         info;
} screen_info;

typedef	uint64_t	QWORD;
typedef uint32_t	DWORD;
typedef uint16_t	WORD;
typedef	uint8_t		BYTE;
#define	LogPrint	printf


typedef	struct {
	int	rx, ry, rxx, ryy, alpha, layer;
} video_pos;
typedef	struct {
	pcap_t					*pcap;
	char					error_buffer[PCAP_ERRBUF_SIZE]; 
	bpf_u_int32 			subnet_mask, ip;
	struct bpf_program 		filter;
	int						promisc;
	int						snaplen;
	int						timeout;
	pthread_t				thr;
	int						started;
	char					dev[64];
	QWORD					packets, bytes;
    QWORD                   animduration, tsanimstart, tsstart, tsnow, tstick, tslast;

	WORD					sport, dport;
	DWORD					ssrc;
    DWORD                   ip4;
	int						online, bufflag;
	char					bpffilter[512];
	int						rtpsize, chan, offset, state;
	BYTE					b[MAX_PACKET];
	DWORD					rtp_timestamp, rtp_last_timestamp, rtp_start_timestamp;
	DWORD					rtp_seq, rtp_last_seq;
//	int						layer, alpha, rx, ry, rxx, ryy;
	video_pos				current, last;
	OMX_BUFFERHEADERTYPE 	*buf;
	COMPONENT_T				*video_decode, *video_scheduler, *video_render,  *clock,  *resize;
	COMPONENT_T				*list[5];
	TUNNEL_T				tunnel[4];
	int						port_settings_changed;
} pcPlayer;


typedef	struct sniff_ip_st {
    #if BYTE_ORDER == LITTLE_ENDIAN
    DWORD ip_hl:4, /* header length */
    ip_v:4; /* version */
    #if BYTE_ORDER == BIG_ENDIAN
    DWORD ip_v:4, /* version */
    ip_hl:4; /* header length */
    #endif
    #endif /* not _IP_VHL */
    BYTE ip_tos; /* type of service */
    WORD ip_len; /* total length */
    WORD ip_id; /* identification */
    WORD ip_off; /* fragment offset field */
    #define IP_RF 0x8000 /* reserved fragment flag */
    #define IP_DF 0x4000 /* dont fragment flag */
    #define IP_MF 0x2000 /* more fragments flag */
    #define IP_OFFMASK 0x1fff /* mask for fragmenting bits */
    BYTE ip_ttl; /* time to live */
    BYTE ip_p; /* protocol */
    WORD ip_sum; /* checksum */
    DWORD ip_src,ip_dst; /* source and dest address */
} __attribute__ ((__packed__)) sniff_ip;

typedef	struct sniff_tcp_st {
    WORD th_sport; /* source port */
    WORD th_dport; /* destination port */
    DWORD th_seq; /* sequence number */
    DWORD th_ack; /* acknowledgement number */
    #if BYTE_ORDER == LITTLE_ENDIAN
    DWORD th_x2:4, /* (unused) */
    th_off:4; /* data offset */
    #endif
    #if BYTE_ORDER == BIG_ENDIAN
    DWORD th_off:4, /* data offset */
    th_x2:4; /* (unused) */
    #endif
    BYTE th_flags;
    #define TH_FIN 0x01
    #define TH_SYN 0x02
    #define TH_RST 0x04
    #define TH_PUSH 0x08
    #define TH_ACK 0x10
    #define TH_URG 0x20
    #define TH_ECE 0x40
    #define TH_CWR 0x80
    #define TH_FLAGS (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
    WORD th_win; /* window */
    WORD th_sum; /* checksum */
    WORD th_urp; /* urgent pointer */
}  __attribute__ ((__packed__)) sniff_tcp; 

typedef struct sniff_udp_st {
	WORD	sport;
	WORD	dport;
	WORD	ulen;
	WORD	usum;			
}  __attribute__ ((__packed__)) sniff_udp; 

typedef struct _RTPHeader {
	BYTE		CC:4;        /* CC field */
	BYTE		X:1;         /* X field */
	BYTE		P:1;         /* padding flag */
	BYTE		version:2;
	BYTE		PT:7;     /* PT field */
	BYTE		M:1;       /* M field */
	WORD		seq_num;      /* length of the recovery */
	DWORD		TS;                   /* Timestamp */
	DWORD		ssrc;
} __attribute__ ((__packed__)) RTPHeader; //12 bytes

typedef struct naluHeader_st {
	unsigned type:5;
	unsigned nri:2;
	unsigned forbidden:1;
} __attribute__ ((__packed__)) naluHeader_1;

typedef union {
	struct	naluHeader_st	h;
	BYTE		b;
} __attribute__ ((__packed__)) naluHeader;

typedef	struct fuaHeader_st {
	unsigned type:5;
	unsigned reserved:1;
	unsigned end:1;
	unsigned start:1;
} __attribute__ ((__packed__)) fuaHeader_1;

typedef	union {
	struct fuaHeader_st	h;
	BYTE	b;
} fuaHeader;

ILCLIENT_T			*client = NULL;
pcPlayer	player;
screen_info	__dm;

#define OMX_INIT_STRUCTURE(a) \
  memset(&(a), 0, sizeof(a)); \
;   (a).nSize = sizeof(a); \
  (a).nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
  (a).nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
  (a).nVersion.s.nRevision = OMX_VERSION_REVISION; \
  (a).nVersion.s.nStep = OMX_VERSION_STEP



typedef struct {
	int						len;
	QWORD					ts;
	int						sync;
	char					d[MAX_PACKET];
} packet;

typedef	struct {
	pthread_mutex_t			w;
	int						rpos, wpos;
	int						processed, total, reset;
	packet					p[MAX_PACKETS];
} circular;

sem_t	rw_sem;
circular	cb;

QWORD	tskip = 0;

QWORD	getts(void) {
	struct timespec	ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
      return ts.tv_sec * 1000000 + (ts.tv_nsec / 1000);
}

int	inputAvailable(void) {
	struct timeval		tv;
	static fd_set 		fds;

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
	return (FD_ISSET(0, &fds));
}

double QuadraticEaseInOut(double p) {
	if(p < 0.5)
	{
		return 2 * p * p;
	}
	else
	{
		return (-2 * p * p) + (4 * p) - 1;
	}
}

int	setwindowOMX(pcPlayer *R) {
	OMX_CONFIG_DISPLAYREGIONTYPE	configDisplay;
	OMX_HANDLETYPE					oh;
	video_pos						*vp;
	video_pos						an;
	oh = ILC_GET_HANDLE(R->video_render);
	
	OMX_INIT_STRUCTURE(configDisplay);
	configDisplay.set = (OMX_DISPLAYSETTYPE)(OMX_DISPLAY_SET_NUM  | OMX_DISPLAY_SET_DEST_RECT | OMX_DISPLAY_SET_PIXEL);
	OMX_GetConfig(oh, OMX_IndexConfigDisplayRegion, &configDisplay);
	
	OMX_INIT_STRUCTURE(configDisplay);
	configDisplay.pixel_x = 0;
    configDisplay.pixel_y = 0;
	vp = &R->current;
// A: 0 0 1919 1079 1 255 10000
// A: 400 400 1519 679 1 255 10000
	if (R->animduration) {
		QWORD	delta = R->tsnow - R->tsanimstart;
		double	d = ((double) delta) / ((double) ( 1000.0 * R->animduration));
		if (d >= 1.0) {
			R->animduration = 0;
			R->tsanimstart = 0;
			memcpy(vp, &R->last, sizeof(R->last)); 
		} else {
			d = QuadraticEaseInOut(d);
			an.rx = vp->rx + d * (R->last.rx - vp->rx);
			an.rxx = vp->rxx + d * (R->last.rxx - vp->rxx);
			an.ry = vp->ry + d * (R->last.ry - vp->ry);
			an.ryy = vp->ryy + d * (R->last.ryy - vp->ryy);
			an.alpha = vp->alpha + d * (R->last.alpha - vp->alpha);
			an.layer = R->last.layer;
			vp = &an;
		}
	}

	configDisplay.set = (OMX_DISPLAYSETTYPE)(OMX_DISPLAY_SET_ALPHA | OMX_DISPLAY_SET_NUM | OMX_DISPLAY_SET_LAYER | OMX_DISPLAY_SET_DEST_RECT | OMX_DISPLAY_SET_NOASPECT | OMX_DISPLAY_SET_FULLSCREEN);
	configDisplay.nPortIndex = 90;
	configDisplay.num = 0;
	configDisplay.layer = vp->layer;
	configDisplay.alpha = vp->alpha;
	configDisplay.fullscreen = 0;
	configDisplay.noaspect = 1;
	configDisplay.dest_rect.x_offset  = (int) vp->rx;
	configDisplay.dest_rect.y_offset  = (int) vp->ry;
	configDisplay.dest_rect.width     = (int) vp->rxx - vp->rx + 1;
	configDisplay.dest_rect.height    = (int) vp->ryy - vp->ry + 1;

	return OMX_SetConfig(oh, OMX_IndexConfigDisplayRegion, &configDisplay);
}

int	OMXSetup(pcPlayer *R) {
	OMX_VIDEO_PARAM_PORTFORMATTYPE	format;
	OMX_TIME_CONFIG_CLOCKSTATETYPE	cstate;
	OMX_CONFIG_DISPLAYREGIONTYPE	configDisplay;
	OMX_HANDLETYPE					oh;
	OMX_ERRORTYPE					omx_err;
	int								status = 0;

	bzero(R->list, sizeof(R->list));
	if ((status = ilclient_create_component(client, &R->video_decode, "video_decode", ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS)) != 0)
		goto l0;
	R->list[0] = R->video_decode;
	if ((status = ilclient_create_component(client, &R->video_render, "video_render", ILCLIENT_DISABLE_ALL_PORTS)) != 0)
    	goto l1;
	R->list[1] = R->video_render;
	if ((status = ilclient_create_component(client, &R->clock, "clock", ILCLIENT_DISABLE_ALL_PORTS)) != 0)
		goto l1;
	R->list[2] = R->clock;
	if ((status = ilclient_create_component(client, &R->resize, "resize", ILCLIENT_DISABLE_ALL_PORTS)) != 0)
		goto l1;
	R->list[3] = R->resize;
	if ((status = ilclient_create_component(client, &R->video_scheduler, "video_scheduler", ILCLIENT_DISABLE_ALL_PORTS)) != 0)
		goto l1;
	R->list[4] = R->video_scheduler;
	set_tunnel(R->tunnel, R->video_decode, 131, R->video_scheduler, 10);
	set_tunnel(R->tunnel+1, R->video_scheduler, 11, R->video_render, 90);
	set_tunnel(R->tunnel+2, R->clock, 80, R->video_scheduler, 12);
// setup clock tunnel first
	if ((status = ilclient_setup_tunnel(R->tunnel+2, 0, 0)) != 0)
		goto l2;

	memset(&cstate, 0, sizeof(cstate));
	cstate.nSize = sizeof(cstate);
	cstate.nVersion.nVersion = OMX_VERSION;
	cstate.eState = OMX_TIME_ClockStateWaitingForStartTime;
	cstate.nWaitMask = 1;
	if((status = OMX_SetParameter(ILC_GET_HANDLE(R->clock), OMX_IndexConfigTimeClockState, &cstate)) != OMX_ErrorNone)
		goto l1;
	ilclient_change_component_state(R->clock, OMX_StateExecuting);
	ilclient_change_component_state(R->video_decode, OMX_StateIdle);

	setwindowOMX(R);
	memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
	format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
	format.nVersion.nVersion = OMX_VERSION;
	format.nPortIndex = 130;
	format.eCompressionFormat = OMX_VIDEO_CodingAVC;
	format.xFramerate = 0;
	if ((status = OMX_SetParameter(ILC_GET_HANDLE(R->video_decode), OMX_IndexParamVideoPortFormat, &format)) != OMX_ErrorNone)
		goto l2;
	if ((status = ilclient_enable_port_buffers(R->video_decode, 130, NULL, NULL, NULL)))
		goto l2;
	ilclient_change_component_state(R->video_decode, OMX_StateExecuting);
	
	return status;
l2:
	ilclient_disable_tunnel(R->tunnel);
	ilclient_disable_tunnel(R->tunnel+1);
	ilclient_disable_tunnel(R->tunnel+2);
	if (R->video_decode)
		ilclient_disable_port_buffers(R->video_decode, 130, NULL, NULL, NULL);
	ilclient_teardown_tunnels(R->tunnel);

	ilclient_state_transition(R->list, OMX_StateIdle);
	ilclient_state_transition(R->list, OMX_StateLoaded);

l1:
	ilclient_cleanup_components(R->list);
l0:
	if (status) {
		LogPrint("ERROR# OPEMNAX ERROR : %08X\r\n", status);
		fflush(stdout);
	}
	return status;
}

void	D_put( pcPlayer *R,  packet *pck, const char *d, int l) {
	if (l + pck->len <= MAX_PACKET) {
		memcpy(pck->d + pck->len, d, l);
		pck->len += l;
	} else {
		fprintf(stderr, "BUFFER TOO SMALL\r\n");
		fflush(stderr);
	}
}

int	D_send( pcPlayer *R,  packet *pck) {
	if (!pck->len)
		return 0;
	
	if (R->rtp_seq != R->rtp_last_seq + 1) {
		if (R->rtp_seq == 0) {
			fprintf(stderr, "SEQUENCE REWIND!!\r\n");
			fflush(stderr);
		} else {
			fprintf(stderr, "SEQUENCE JUMP %u -> %u!!\r\n",  R->rtp_last_seq + 1, R->rtp_seq);
			fflush(stderr);
		}
	}
	if ((pck->sync = R->rtp_timestamp != R->rtp_last_timestamp)) {
		if (1) { //R->rtp_timestamp == 0) {
			R->tsstart = R->tsnow;
			R->rtp_start_timestamp = R->rtp_timestamp;
		//	fprintf(stderr, "TIMESTAMP REWIND!!\r\n");
		//	fflush(stderr);
		} else if (1) { //R->rtp_seq != R->rtp_last_seq + 1) {
			if (1) { //R->rtp_timestamp < R->rtp_last_timestamp) {
				//fprintf(stderr, "REBASE %d\r\n",  R->rtp_timestamp - R->rtp_last_timestamp);
				//fflush(stderr);
			//
				R->tsstart = R->tsnow;
				R->rtp_start_timestamp = R->rtp_timestamp;
			}
		}
	}
	double	d = (double) ((100.0 * (R->rtp_timestamp - R->rtp_start_timestamp)) / 9.0);
	if (!R->tsstart)
		R->tsstart = R->tsnow;
	pck->ts = R->tsstart + d + gJitter * 10000;
		
	return 1;
}

DWORD	tcp_getssrc(const BYTE *p, int l) {
	int	pl = 0;
	for ( ; l; l--) {
		if (*p == '$')
			break;
	}
	if (!l)
		return 0;
	p++;
	l--;
	if (!l)
		return 0;
	if (*p != 0)
		return 0;
	p++;
	l--;
	if (l < 14)
		return 0;
	pl = *(WORD *) p;
	p += 2;
	l -= 2;
	RTPHeader	*rh = (RTPHeader *) p;
	if (rh->version != 2) {
		return 0;
	}
	return ntohl(rh->ssrc);
}

DWORD	udp_getssrc(const BYTE *p, int l) {
	RTPHeader	*rh = (RTPHeader *) p;
	WORD		csrcoff = 0;
	if (rh->version != 2) {
		return 0;
	}
	if (rh->PT != 96) {
		return 0;
	}
	return ntohl(rh->ssrc);
}

int	ParseRTP(pcPlayer *R, packet *pck, const BYTE *data, int len) {
	RTPHeader	*rh;
	int			l;
	int			csrcoff;
	uint8_t		start_sequence[]= { 0, 0, 1 };
	naluHeader	naluHeaderValue;
	uint8_t 	nal_type = 0;

	QWORD		lts = 0;
	DWORD		delta = 0;
	int			z = 0;


	rh = (RTPHeader *) data;
	
	csrcoff = rh->CC * 4;
	rh->ssrc = ntohl(rh->ssrc);
	data += 12 + csrcoff;
	len -= 12 + csrcoff;

	R->rtp_last_timestamp = R->rtp_timestamp;
	R->rtp_timestamp = ntohl(rh->TS);
	if (R->rtp_last_timestamp == 0) {
		R->rtp_start_timestamp = R->rtp_timestamp;
	}
	R->rtp_last_seq = R->rtp_seq;
	R->rtp_seq = htons(rh->seq_num);
	

	naluHeaderValue.b = *data;
	if ((naluHeaderValue.h.type != 28) && ((naluHeaderValue.h.type < 1) || (naluHeaderValue.h.type > 23))) {
		fprintf(stderr, "INFO# UNKNOWN HEADER TYPE : %d\r\n", naluHeaderValue.h.type);
		fflush(stderr);
		return 0;
	}
	
	if (naluHeaderValue.h.type == 28) {
		data++;
		len--;
		uint8_t fu_indicator = naluHeaderValue.b;
		uint8_t fu_header = *data;   // read the fu_header.
		uint8_t start_bit = fu_header >> 7;
		nal_type = (fu_header & 0x1f);
		uint8_t reconstructed_nal;
		reconstructed_nal = fu_indicator & (0xe0);
		reconstructed_nal |= nal_type;
		data++;
		len--;

		if (start_bit) {
			D_put(R, pck, start_sequence, 3);
			D_put(R, pck, &reconstructed_nal, 1);
		}

	} else if (naluHeaderValue.h.type >= 1 && naluHeaderValue.h.type <= 23) {
		D_put(R, pck, start_sequence, 3);
	} 
	D_put(R, pck, data, len);
	return D_send(R, pck);
}

int	ProcessCommand(pcPlayer *pl, const char *p) {
	if (strncasecmp(p, "W: ", 3) == 0) {
		int		x, y, xx, yy, l, a;
		if (sscanf(p + 3, "%d %d %d %d %d %d", &x, &y, &xx, &yy, &l, &a) == 6) {
			pl->current.rx = x;
			pl->current.ry = y;
			pl->current.rxx = xx;
			pl->current.ryy = yy;
			pl->current.layer = l;
			pl->current.alpha = a;
			setwindowOMX(pl);
		}
		return 0;
	} 
	if (strncasecmp(p, "A: ", 3) == 0) {
		int		x, y, xx, yy, l, a, endt;
		if (sscanf(p + 3, "%d %d %d %d %d %d %d", &x, &y, &xx, &yy, &l, &a, &endt) == 7) {
			pl->last.rx = x;
			pl->last.ry = y;
			pl->last.rxx = xx;
			pl->last.ryy = yy;
			pl->last.layer = l;
			pl->last.alpha = a;
			pl->animduration = endt;
			pl->tsanimstart = pl->tsnow;
			setwindowOMX(pl);
		}
		return 0;
	}
	return 1;
}
int	ParseRTP(pcPlayer *R, packet *pck, const BYTE *data, int len);

int	thread_terminated = 0;
void	*capturator(void *param) {
	pcPlayer			*pl = (pcPlayer *) param;
	packet				*pck;
	struct pcap_pkthdr	*header;
	const BYTE			*pkt_data;
	int					r = 0;
	QWORD				q;

	const sniff_ip			*ip_header;
	const sniff_udp			*udp_header;
	int						l;
	int						i;
	const BYTE				*p;
	WORD					sport, dport;
	DWORD					ssr = 0;
	int						inc = 0;

	for ( ; ; ) {
		pck = &cb.p[cb.wpos];
		r = pcap_next_ex(pl->pcap, &header, &pkt_data);
		pck->len = 0;
		q = getts();
		pl->tsnow = q;
		inc = 0;
		if (r == 0) {
			LogPrint("TICK\r\n");
			fflush(stdout);
			//usleep(0);
			
			if (pl->online) {
				if (q - pl->tslast >= 1500000) {
					pl->online = 0;
					fprintf(stderr, "INFO# DISCONNECTED [%X] - %d %d)\r\n", pl->ssrc, pl->sport, pl->dport);
					fflush(stderr);
					exit(11);
					pl->ssrc = pl->sport = pl->dport = pl->offset = 0;
					continue;
				}
			}
			continue;
		}
		if (r < 0) {
			thread_terminated = 1;
			break;
		}	
		
		l = header->caplen;
		if (!l)
			continue;
		
		p = (BYTE *) pkt_data;
		p += 14;
		l -= 14;
		ip_header = (sniff_ip *) p;
		i = (ip_header->ip_hl * 4);
		p += i;
		l -= i;
		udp_header = (sniff_udp *) p;
		sport = ntohs(udp_header->sport);
		dport = ntohs(udp_header->dport);
		i = sizeof(*udp_header);
		p += i;
		l -= i;
		ssr = udp_getssrc(p, l);
		if (cb.reset) {
			cb.reset = 0;
			pl->online = pl->sport = pl->dport = pl->ssrc = 0;
			pl->tslast = pl->tsstart = pl->tsnow;
			pl->rtp_last_seq = pl->rtp_last_timestamp = pl->rtp_start_timestamp = pl->rtp_seq = pl->rtp_timestamp = 0;
		}

		if (!ssr)
			continue;
		if ((pl->sport == sport) && (pl->dport == dport) && pl->ssrc && (pl->ssrc == ssr)) {
			pl->tslast = pl->tsnow;
			pl->packets++;	
			pl->bytes += l;
			if (!pl->online) {
				pl->bufflag = 1;
				pl->online = 1;
			}
			inc = ParseRTP(pl, pck, p, l);
		} else if ((!pl->ssrc) || ((pl->tsnow - pl->tslast) > 200000)) {
			if (pl->ssrc != 0) {
				pl->online = 1;
				pl->bufflag = 1;
			} else {
				pl->bufflag = 1;
			}
			pl->ssrc = ssr;
			pl->sport = sport;
			pl->dport = dport;
			pl->tslast = pl->tsstart = pl->tsnow;
			pl->rtp_last_seq = pl->rtp_last_timestamp = pl->rtp_start_timestamp = pl->rtp_seq = pl->rtp_timestamp = 0;
			if (pl->buf) {
				//pl->buf->nAllocLen = 0;
			}
			pl->packets = 1;
			pl->bytes = l;
			BYTE *B = (BYTE *) &ip_header->ip_src;
			fprintf(stderr, "INFO# UDP CONNECTED %d.%d.%d.%d %d - %d %d) ", B[0], B[1], B[2], B[3],  pl->ssrc, sport, dport);
			fflush(stderr);
			inc = ParseRTP(pl, pck, p, l);
		} else {
		//	fprintf(stderr, "5\r\n");
		//	fflush(stderr);
			continue;
		}
		if (!inc)
			continue;
		cb.wpos = (cb.wpos + 1) % MAX_PACKETS;
		int	total = 0;
		sem_wait(&rw_sem);
		cb.total++;
		total = cb.total;
		sem_post(&rw_sem);
		if (total >= MAX_PACKETS) {
 			fprintf(stderr, "OVERFLOW\r\n");
			fflush(stderr);

			
			sem_wait(&rw_sem);
			bzero(&cb, sizeof(cb));
			sem_post(&rw_sem);
			
			//cb.reset = 1;
			continue;
		}
		/*
		if (sem_trywait(&rw_sem)) {
			if (errno == EAGAIN)
				sem_post(&rw_sem);
		}
		*/
	}
	LogPrint("#INFO THREAD TERMINATED!!!\r\n");
	fflush(stdout);
	return NULL;
}

void	D_play(pcPlayer *R, packet *p) {
	if (!R->buf) { 
		if (!R->video_decode)
			return;
		R->buf = ilclient_get_input_buffer(R->video_decode, 130, 1);
	}
	if (!R->buf)
		return;
	if (tskip) {
		if (p->ts <= tskip) {
			return;
		} else { //if (p->sync) {
			fprintf(stderr, "SKIP END!\r\n");
			fflush(stderr);
			tskip = 0;
		}
	}
	sem_wait(&rw_sem);
	int total = cb.total;
	sem_post(&rw_sem);
	if (p->sync) {
		if (!tskip && p->ts > 10000000l && p->len)
			if (p->ts < (R->tsnow - gJitter * 10000)) 
				if (total > MAX_PACKETS / 10) {
					tskip = p->ts;
					//pck->len = 0;
					fprintf(stderr, "SKIP START! %llu %llu %llu %d\r\n", p->ts, R->tsnow, R->tsnow - p->ts, total);
					fflush(stderr);
					return;		
				}
	}
	if (p->len <= R->buf->nAllocLen) {
		memcpy(R->buf->pBuffer, p->d, p->len);
		R->buf->nFilledLen = p->len;
	} else {
		LogPrint("ERROR# BUFFER OVERFLOW ! %d %d \r\n", R->buf->nAllocLen, p->len);
		fflush(stdout);
	}
	if (!R->port_settings_changed) {
		int	status = 0;
	    if ((status = ilclient_remove_event(R->video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1)) == 0) {
			R->port_settings_changed = 1;
			LogPrint("INFO# PORT SETTINGS CHANGED\r\n");
			fflush(stdout);
			if ((status = ilclient_setup_tunnel(R->tunnel, 0, 0)) == 0) {
				ilclient_change_component_state(R->video_scheduler, OMX_StateExecuting);
				// now setup tunnel to video_render
				if ((status = ilclient_setup_tunnel(R->tunnel+1, 0, 1000)) == 0) {	
					ilclient_change_component_state(R->video_render, OMX_StateExecuting);
				}
			}
		}
	
	}
	R->buf->nFilledLen = p->len;
	R->buf->nFlags = OMX_BUFFERFLAG_STARTTIME; 		
	R->buf->nTimeStamp.nHighPart = (p->ts >> 32) & 0xffffffff;
	R->buf->nTimeStamp.nLowPart = p->ts & 0xffffffff;
	int r = OMX_EmptyThisBuffer(ILC_GET_HANDLE(R->video_decode), R->buf);
	R->buf = NULL;	
}

int		StartPCAPPlayer(char *dev, pcPlayer *pl, int promisc, int snaplen, int timeout, int ip, video_pos *vp) {
	int					r = 0;
	struct pcap_pkthdr	*header;
	const BYTE			*pkt_data, *p;
	int					l, i;
	struct ether_header	*eth_header;
	const sniff_ip		*ip_header;
    const sniff_tcp		*tcp_header;
	const sniff_udp		*udp_header;
	WORD				sport, dport;
	DWORD				ssr = 0;

	if (!dev)
		return 1;
	bzero(&cb, sizeof(circular));
	bzero(pl, sizeof(*pl));
	memcpy(&pl->current, vp, sizeof(*vp));
	pthread_mutex_init(&cb.w, NULL);
// sems {
	sem_init(&rw_sem, 0, 0);
	sem_post(&rw_sem);
// sems }


	/*pl->current.layer = 100;
	pl->current.alpha = 255;
	pl->current.rx = pl->current.ry = 0;
	pl->current.rxx = __dm.w - 1;
	pl->current.ryy = __dm.h - 1;
	*/
	i = OMXSetup(pl);
	if (i)
		return i;
	pcap_if_t *alldevs;
    int status = pcap_findalldevs(&alldevs, pl->error_buffer);
	if(status != 0) {
        return 2;
    }
	pl->ip4 = 0;
	for( pcap_if_t *d = alldevs; d != NULL; d = d->next) {
       	if (strcmp(d->name, dev))
			continue;
        for (pcap_addr_t *a = d->addresses; a != NULL; a = a->next) {
            if (a->addr->sa_family == AF_INET) {
               pl->ip4 = ((struct sockaddr_in*)a->addr)->sin_addr.s_addr;
			   break;
			}
        }
    }
	if (!pl->ip4)
		return 3;
	pcap_freealldevs(alldevs);
	pl->promisc = promisc;
	pl->snaplen = snaplen;
	pl->timeout = timeout;
	pl->packets = pl->bytes = 0;
	strcpy(pl->dev, dev);
	if (pcap_lookupnet(dev, &pl->ip, &pl->subnet_mask, pl->error_buffer) == -1) {
        LogPrint("ERROR# Could not get information for device: %s\r\n", dev);
		fflush(stdout);
        pl->ip = 0;
        pl->subnet_mask = 0;
		r = 4;
		goto l0;
    }
	pl->pcap = pcap_create(pl->dev, pl->error_buffer);
	if (!pl->pcap) {
		LogPrint("ERROR# Could not create pcap for device: %s\r\n", pl->dev);
		fflush(stdout);
		r = 4;
		goto l0;
	}
	if ((r = pcap_set_promisc(pl->pcap, pl->promisc))) {
		LogPrint("ERROR# pcap_set_promisc %s\r\n",  pcap_geterr(pl->pcap));
		fflush(stdout);
		goto l1;
	}
	if ((r = pcap_set_snaplen(pl->pcap, pl->snaplen))) {
		LogPrint("ERROR# pcap_set_snaplen %s\r\n",  pcap_geterr(pl->pcap));
		fflush(stdout);
		goto l1;
	}
	if ((r = pcap_set_timeout(pl->pcap, pl->timeout))) {
		LogPrint("ERROR# pcap_set_timeout %s\r\n",  pcap_geterr(pl->pcap));
		fflush(stdout);
		goto l1;
	}

	if ((r = pcap_set_immediate_mode(pl->pcap, 1))) {
		LogPrint("ERROR# pcap_set_immediate_mode %s\r\n",  pcap_geterr(pl->pcap));
		goto l1;
	}
	if ((r = pcap_activate(pl->pcap))) {
		LogPrint("ERROR# pcap_activate %s\r\n",  pcap_geterr(pl->pcap));
		goto l1;
	}
	if ((r = pcap_setdirection(pl->pcap, PCAP_D_IN))) {
		LogPrint("ERROR# pcap_setdirection %s\r\n",  pcap_geterr(pl->pcap));
		goto l1;
	}
	BYTE	*b = (BYTE *) &ip;
	sprintf(pl->bpffilter, "udp and src %d.%d.%d.%d and udp[8] & 0xC0 == 0x80 and udp[9] & 127 == 96 and length > 24", b[0], b[1], b[2], b[3]);
	if ((r = pcap_compile(pl->pcap, &pl->filter, pl->bpffilter, 1, PCAP_NETMASK_UNKNOWN)) == -1) {
		LogPrint("ERROR# Bad filter - %s\r\n", pcap_geterr(pl->pcap));
		goto l1;
	}
	if ((r = pcap_setfilter(pl->pcap, &pl->filter)) == -1) {
		LogPrint("ERROR# Error setting filter - %s\r\n", pcap_geterr(pl->pcap));
		goto l2;
	}
	char	line[256];
	int		lp = 0;
	packet	*pck;
	
	QWORD	q;
	int		total;

	pthread_create(&pl->thr, NULL, capturator, (void *) pl);
	//while((r = pcap_next_ex(pl->pcap, &header, &pkt_data)) >= 0){
	for ( ; ; ) {
		if (thread_terminated)
			break;
		while (inputAvailable()) {
			BYTE	c;
			if (read(0, &c, 1) != 1)
				break;
		
			if (c == 13 || c == 10) {
				line[lp] = 0;
				if (lp) {
					
					int	i = ProcessCommand(pl, line);
					if (i)
						goto l2;
				}
				line[lp = 0] = 0;
			} else {
				line[lp] = c;
				if (lp < sizeof(line))
					lp++;
			}
		}
		if (pl->tsanimstart) {
			
			pl->tsnow = getts(); 
					
			setwindowOMX(pl);
		}
		
	//	
	
lnc:
		
		sem_wait(&rw_sem);
		total = cb.total;
		sem_post(&rw_sem);
		q = getts(); 
		if (!total) {
			usleep(50000);
			continue;
		}
		pck = &cb.p[cb.rpos];
		if (pck->len) {
			if (pck->ts > q) {
				//fprintf(stderr, "<%llu : %llu>\r", pck->ts, q);
				//fflush(stderr);
				if ((pck->ts - q) > 50000)
					usleep(50000);
				else
					usleep(pck->ts - q);
				continue;			
			} 
		} else
			goto lc;

		D_play(pl, pck);
		pck->len = 0;
lc:
		cb.rpos = (cb.rpos + 1) % MAX_PACKETS;	
		sem_wait(&rw_sem);	
		cb.total--;
		int	total = cb.total;
		sem_post(&rw_sem);
		cb.processed++;
		if (tskip) {
			if (total)
				goto lnc;
			else {
				tskip = 0;
				fprintf(stderr, "SKIP END 1\r\n");
				fflush(stderr);
			}
		}
	//	fprintf(stderr, "< %12u : %12u >\r", cb.processed, cb.total);
    }
l2:
	pcap_freecode(&pl->filter);
l1:
	pcap_close(pl->pcap);
	pl->pcap = NULL;
l0:
	return r;
}

int		main(int argc, char **argv) {
	char 	dev[32] = "eth0";
	int		ip = 0;
	video_pos	pos = {
		0, 0, 100, 100, 255, 1
	};

	int		i = 0;
//	if(geteuid() != 0) {
//		LogPrint("ERROR# THIS PROGRAM MUST BE RUN AS ROOT, TRY sudo %s\r\n", argv[0]);
//		return 0;
//	}
	for (i = 1; i < argc; i++) {
		if (strncasecmp(argv[i], "dev=", 4) == 0) {		
			strncpy(dev, argv[i] + 4, 30);
		} else if (strncasecmp(argv[i], "ip=", 3) == 0) {		
			sscanf(argv[i] + 3, "%d", &ip);
		} else if (strncasecmp(argv[i], "host=", 5) == 0) {	
			ip = (int) inet_addr(argv[i] + 5);
		} else if (strncasecmp(argv[i], "x=", 2) == 0) {	
			pos.rx = (int) atoi(argv[i] + 2);
		} else if (strncasecmp(argv[i], "xx=", 3) == 0) {	
			pos.rxx = (int) atoi(argv[i] + 3);
		} else if (strncasecmp(argv[i], "y=", 2) == 0) {	
			pos.ry = (int) atoi(argv[i] + 2);
		} else if (strncasecmp(argv[i], "yy=", 3) == 0) {	
			pos.ryy = (int) atoi(argv[i] + 3);
		} else if (strncasecmp(argv[i], "layer=", 6) == 0) {	
			pos.layer = (int) atoi(argv[i] + 6);
		} else if (strncasecmp(argv[i], "alpha=", 6) == 0) {	
			pos.alpha = (int) atoi(argv[i] + 6);
		} else if (strncasecmp(argv[i], "j=", 2) == 0) {	
			gJitter = (int) atol(argv[i] + 2);
			LogPrint("INFO# JITTER %u\r\n", gJitter);
		}
	}
	BYTE	*b = (BYTE *) &ip;
	LogPrint("INFO# HOST : %d.%d.%d.%d\r\n", b[0], b[1], b[2], b[3]);
	LogPrint("INFO# pos : %d.%d.%d.%d %d %d\r\n", pos.rx, pos.ry, pos.rxx, pos.ryy, pos.layer, pos.alpha);
	fflush(stdout);
	if (!ip)
		return 99;

	bcm_host_init();

	client = ilclient_init();
	
	if (!client)
		return 100;

	__dm.display = vc_dispmanx_display_open(0);
	i = vc_dispmanx_display_get_info(__dm.display, &__dm.info);
	if (i)
		goto l1;
	__dm.w = __dm.info.width;
	__dm.h = __dm.info.height;
	
	if ((i = OMX_Init()) != OMX_ErrorNone) {
		i = 101;
		goto l1;
	}
	setpriority(PRIO_PROCESS, 0, -20);	
	i = StartPCAPPlayer(dev, &player, 0, MAX_PACKET, 10000, ip, &pos);
l1:	
	ilclient_destroy(client);
	bcm_host_deinit();
	LogPrint("QUIT#\r\n");
	fflush(stdout);
	return i;
}
