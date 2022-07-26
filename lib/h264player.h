#ifndef	h264player_h
#define	h264player_h

typedef	struct {
	int	id;
	int	ownerid;
	int	screen;
	int	layer;
	int	alpha;
	int	x, y, w, h;
	int	quit;
	int	changed;
	int	noloop;
	int	i1, i2, i3, i4;
} shm_data;

shm_data	*StartVideoPlayerC(HandleList *hl, void *cb, char *fname, int screen, int l, int x, int y, int w, int h, int alpha);
HandleEntry	*StartVideoPlayerS(HandleList *hl, void *cb, int ch, int screen, int l, int x, int y, int w, int h, int alpha);
HandleEntry	*StartVideoPlayer(HandleList *hl, void *cb, char *fname, int screen, int l, int x, int y, int w, int h, int alpha, int noloop);
HandleEntry	*StartRTSPPlayer(HandleList *hl, void *cb, char *url, int screen, int x, int y, int w, int h, int layer, int alpha, void *ed);
void	shm_done(shm_data *p);

#endif