#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include	<sys/ipc.h>
#include	<sys/shm.h>

#include	"utils.h"
#include	"h264player.h"
#include	"process.h"

shm_data	*shm_init(int _shmkey) {
	int			shmkey = _shmkey;
	int			shmid;
	shm_data	*r = NULL;
	int			owner = 1;

	shmid = shmget(shmkey, sizeof(shm_data), IPC_CREAT | IPC_EXCL | 0777);
	if (shmid < 0) {
		printf("SHM ID ALREADY EXISTS!\n");
		shmid = shmget(shmkey, sizeof(shm_data), 0777);		
		if (shmid < 0) {
			perror("Could not get!");
			goto l0;			
		}
		owner = 0;
	} 
	
	printf("SHM ID : %d\n", shmid);
	r = shmat(shmid, NULL, 0);
	if (!r)
		goto l1;
	if (owner) {
		bzero(r, sizeof(*r));
		r->id = shmid;
		r->ownerid = shmid;
		r->screen = 0;
		r->layer = 0;
		r->w = 100;
		r->h = 100;
		r->alpha = 255;
		r->changed = 1;
	}
	return r;
l1:
	if (owner)
		shmctl(shmid, IPC_RMID, 0);
l0:
	return r;
}

void	shm_done(shm_data *p) {
	int	r;
	int	isowner = 0;
	if (p)
		isowner = p->ownerid;
		
	r = shmdt(p);
	if (r < 0)
		perror("Cound not free!");
	if (isowner) {
		LogPrint("REMOVING ID!\n");
		shmctl(isowner, IPC_RMID, 0);
	}
}

shm_data	*StartVideoPlayerC(HandleList *hl, void *cb, char *fname, int screen, int l, int x, int y, int w, int h, int alpha) {
	int	fh;
	shm_data	*vc;
	int			channel;
	char		*p[4];
	fh = open(fname, O_RDONLY);
	if (fh < 0)
		return NULL;
	close(fh);
	channel = ftok(fname, 0x290999);
	vc = shm_init(channel);
	if (!vc) 
		return NULL;
	vc->screen = screen;
	vc->layer = l;
	vc->x = x;
	vc->y = y;
	vc->w = w;
	vc->h = h;
	vc->alpha = alpha;
	vc->quit = 0;
	//pthread_create(&vc->th, NULL, VideoThread, vc);
	p[0] = "./Camci";
	p[1] = fname;
	p[2] = " 2>&1";
	p[3] = NULL;
	AddLineProcess(hl, cb, p[0], p);
	return vc;
/*		
	if (fh >= 0)
		video_decode_start(fh, 6, 0, 0, 0, 800, 480);
		 */ 
}

HandleEntry	*StartRTSPPlayer(HandleList *hl, void *cb, char *url, int screen, int x, int y, int w, int h, int layer, int alpha, void *ed) {
	int			channel;
	char		*p[32];
	char		win[128];
	char		disp[64], slayer[64];
	int			ix = 0;
	HandleEntry	*he;

	p[ix++] = "/usr/bin/omxplayer.bin";
	p[ix++] = url;
	p[ix++] = "--live";
	p[ix++] = "-s";
	p[ix++] = "-d";
	p[ix++] = "-w";
	p[ix++] = "--display";
	sprintf(disp, "%d", screen);
	p[ix++] = disp;
	p[ix++] = "--win";
	sprintf(win, "%d,%d,%d,%d", x, y, w, h);
	p[ix++] = win;
	p[ix++] = "--layer";
	sprintf(slayer, "%d", layer);
	p[ix++] = slayer;
	p[ix++] = " 2>&1";
	p[ix++] = NULL;
	if ((he = AddLineProcess(hl, cb, p[0], p))) {
		he->ExtraData2 = ed;
	}
	return he;
/*		
	if (fh >= 0)
		video_decode_start(fh, 6, 0, 0, 0, 800, 480);
		 */ 
}

HandleEntry	*StartVideoPlayerS(HandleList *hl, void *cb, int ch, int screen, int l, int x, int y, int w, int h, int alpha) {
//	int	fh;
	HandleEntry	*he;
	shm_data	*vc;
	int			channel;
	char		*p[3];
	char	fname[4];
	
	channel = ch + 0x290999;
	vc = shm_init(channel);
	sprintf(fname, "%d", ch);
	if (!vc) 
		return NULL;
	
	vc->screen = screen;
	vc->layer = l;
	vc->x = x;
	vc->y = y;
	vc->w = w;
	vc->h = h;
	vc->alpha = alpha;
	vc->quit = 0;
	//pthread_create(&vc->th, NULL, VideoThread, vc);
	p[0] = "./Camci";
	p[1] = fname;
	p[2] = NULL;
	he = AddLineProcess(hl, cb, p[0], p);
	he->ExtraData2 = (char *) vc;
 
	return he;
}

HandleEntry	*StartVideoPlayer(HandleList *hl, void *cb, char *fname, int screen, int l, int x, int y, int w, int h, int alpha, int noloop) {
	int	fh;
	shm_data	*vc;
	int			channel;
	char		*p[3];
	HandleEntry	*he;
	
	fh = open(fname, O_RDONLY);
	if (fh < 0)
		return NULL;
	close(fh);
	channel = ftok(fname, 0x290999);
	vc = shm_init(channel);
	if (!vc) 
		return NULL;
	vc->screen = screen;
	vc->layer = l;
	vc->x = x;
	vc->y = y;
	vc->w = w;
	vc->h = h;
	vc->noloop = noloop;
	vc->alpha = alpha;
	vc->quit = 0;
	//pthread_create(&vc->th, NULL, VideoThread, vc);
	p[0] = "./Camci";
	p[1] = fname;
	p[2] = NULL;
	he = AddLineProcess(hl, cb, p[0], p);
	he->ExtraData2 = (char *) vc;
	strcpy(he->name, fname);
	return he;
/*		
	if (fh >= 0)
		video_decode_start(fh, 6, 0, 0, 0, 800, 480);
		 */ 
}