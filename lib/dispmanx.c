#define		dispmanx_c

#include	"dispmanx.h"
#include	<math.h>
#include	<stdio.h>
#include	<sys/time.h>
#include	<pthread.h>
#include	"texteditor.h"

extern	int	gScreen;

void	Lock(void) {
	pthread_mutex_lock(&fbsem);
}

void	Unlock(void) {
	pthread_mutex_unlock(&fbsem);
}

void	Invalidate(void) {
	if (!__dm.dirty) {
		__dm.update = vc_dispmanx_update_start(0);
		__dm.dirty = 1;
	}
}

struct	timeval	_v0 = {0, 0};
struct	timeval	_v1;

void	Validate(void) {
//	llist_entry	*le;
//	dm_img		*img;
//	DWORD		delta;
	
	if (__dm.bgr) {
		if (_v0.tv_sec == 0 &&
			_v0.tv_usec == 0
		)
			gettimeofday(&_v0, NULL);
		else {
/*			gettimeofday(&_v1, NULL);
			delta = _v1.tv_sec - _v0.tv_sec;
			delta *= 1000000;
			delta += _v1.tv_usec - _v0.tv_usec;
			
			_v0 = _v1;
			if (pthread_mutex_trylock(&__ssync)) {
				for (le = __dm.lh.f; le; le = le->n) {
					img = (dm_img *) le;
					if (!img->focus)
						continue;
					if (img->visible)
						(*img->focus)(img, -1, -1, delta);
				}
				pthread_mutex_unlock(&__ssync);
			} 
			 */ 
		}
	} else
		;//return;
	
	if (__dm.dirty) {
		if (!pthread_mutex_trylock(&fbsem)) {
			//CheckTexts();
//			fprintf(stderr, "SUBMIT UPDATE!\n");
//			fflush(stderr);
			vc_dispmanx_update_submit_sync(__dm.update);
			__dm.dirty = 0;
//			fprintf(stderr, "UPDATE SUBMITTED!\n");
//			fflush(stderr);
			//sem_post(&fbsem);
			pthread_mutex_unlock(&fbsem);
		}
	}
}

void	dm_setbackground(DWORD d) {
	VC_RECT_T		r0;
	__dm.bkg = d;
	Lock();
	Invalidate();
	vc_dispmanx_rect_set( &r0, 0, 0, 1, 1);
	
	vc_dispmanx_resource_write_data(
		__dm.bgr,
		VC_IMAGE_RGBA32,
		(int) 4, 
		(void *) &d,
		&r0
	);
	Unlock();
}

void	InitDispmanX(int screen) { 
	int				ret;
	VC_RECT_T		r0, r1;
	BYTE	bg[4] = {0x0, 0x00, 0x0, 0x0};

	__dm.screen = screen;
	
	init_llist(256*4, &__dm.lh, sizeof(dm_img));
	init_llist(256*4, &__dm.anim_lh, sizeof(dm_anim));
	bcm_host_init();
	__dm.display = vc_dispmanx_display_open(__dm.screen);
	ret = vc_dispmanx_display_get_info(__dm.display, &__dm.info);
	if (ret)
		return;
	__dm.w = __dm.info.width;
	__dm.h = __dm.info.height;
	//fprintf(stderr, "WIDTH: %d HEIGHT: %d\r\n", __dm.w, __dm.h);
	__dm.bgr = vc_dispmanx_resource_create( 
			VC_IMAGE_RGBA32,
//			VC_IMAGE_8BPP,
			1,
			1,
			&__dm.bgh
		);
	vc_dispmanx_rect_set( &r0, 0, 0, 1, 1);
	ret = vc_dispmanx_resource_write_data(
		__dm.bgr,
		VC_IMAGE_RGBA32,
//		VC_IMAGE_8BPP,
		(int) 4, 
		(void *) &bg[0],
		&r0
	);
	//  = {DISPMANX_FLAGS_ALPHA_FIXED_NON_ZERO | DISPMANX_FLAGS_ALPHA_FROM_SOURCE, 
    __dm.bga.flags =
		DISPMANX_FLAGS_ALPHA_FROM_SOURCE;
	__dm.bga.opacity = 255;
	__dm.bga.mask = 0;
	__dm.update = vc_dispmanx_update_start(10);

	vc_dispmanx_rect_set( &r0, 0, 0, 0 << 16, 0 << 16);
	vc_dispmanx_rect_set( &r1, 0, 0, 0, 0); 	
	__dm.bge = vc_dispmanx_element_add(   
			__dm.update,
			__dm.display,
            0,               // layer
            &r1,
            __dm.bgr,
            &r0,
            DISPMANX_PROTECTION_NONE,
            &__dm.bga,
            NULL,             // clamp
			VC_IMAGE_ROT0
		);
	ret = vc_dispmanx_update_submit_sync(__dm.update);
	__dm.dirty = 0;
//	sem_init(&__dm.anim_sem, 0, 1);
	pthread_mutex_init(&__dm.anim_sem, NULL);

}

void	CloseDispmanX(void) {
//	int	ret;
	llist_entry	*le = __dm.lh.f, *n;
	for ( ; le; le = n) {
		n = le->n;
		//LogPrint("Freeing img : %p\n", le);
		dm_freeimage((dm_img *) le);
	}
	Validate();
	__dm.update = vc_dispmanx_update_start(10);	
	vc_dispmanx_element_remove(__dm.update, __dm.bge);
    vc_dispmanx_update_submit_sync(__dm.update);
    vc_dispmanx_resource_delete(__dm.bgr);
    vc_dispmanx_display_close(__dm.display );
//	sem_destroy(&__dm.anim_sem);
	pthread_mutex_destroy(&__dm.anim_sem);
}
int	dm_freeimage(dm_img *di) {
//	dm_img	*i;
	if (!di)
		return 1;
	dm_detach(di);
	while (di->fi)	
		dm_freeimage(di->fi);
	Lock();
	Invalidate();
	dm_DelAnims(di);	
	if (di->element)
		vc_dispmanx_element_remove(
			__dm.update,
			di->element
		);
	if (di->resource)
		vc_dispmanx_resource_delete( 
			di->resource
		);
	bzero(((char *) di) + sizeof(llist_entry), sizeof(*di)-sizeof(llist_entry));
	del_entry(&__dm.lh, &di->le);	
	Unlock();
	return 0;
}

dm_anim	*dm_AddAnim(dm_img *img, WORD kind, int duration, double (*f)(double, dm_img *), int max, int min,
	void *complete, char *data) {
	dm_anim	*da;
	da = (dm_anim *) get_entry(&__dm.anim_lh);
	if (!da) {
		printf("EMPTY ANIM !!!\n"); fflush(stdout);
		return NULL;
	}
//	if (! pthread_mutex_trylock(&__dm.inanim)) {
//		pthread_mutex_lock(&__dm.anim_sem);
//		inanim = 1;
//	}
//	sem_wait(&__dm.anim_sem);
	bzero(((char *) da) + sizeof(llist_entry), sizeof(*da)-sizeof(llist_entry));
	da->img = img;
	da->duration = duration;
	da->f = f;
	da->pos = 0;
	da->kind = kind;
	da->min = min;
	da->data = data;
	if (da->min == -999) {
		if (kind & ANIM_X)
			da->min = da->img->x;
		if (kind & ANIM_Y)
			da->min = da->img->y;
		if (kind & ANIM_TRANS)
			da->min = da->img->opacity;
	}
	da->max = max;
	if (da->max == -999) {
		if (kind & ANIM_X)
			da->max = da->img->x;
		if (kind & ANIM_Y)
			da->max = da->img->y;
		if (kind & ANIM_TRANS)
			da->max = da->img->opacity;
	}
	da->complete = (int (*)(dm_img *, struct dm_anim_st *, char *)) complete;
	da->active = 1;
	da->r = 1.0;
	da->cur = da->min;
//	sem_post(&__dm.anim_sem);
//	if (!__dm.inanim)
//		pthread_mutex_unlock(&__dm.anim_sem);
	return da;
}
int	dm_DelAnims(dm_img *img) {
	llist_entry	*le, *n;
	dm_anim		*da;
	int			i;

//	if (!__dm.inanim)			
	//sem_wait(&__dm.anim_sem);
//		pthread_mutex_lock(&__dm.anim_sem);
	
	for (i=0, le = __dm.anim_lh.f; le; le = n) {
		n = le->n;
		da = (dm_anim *) le;
		if (da->img != img)
			continue;
				
		del_entry(&__dm.anim_lh, le);
		i++;
	}
//	if (!__dm.inanim)			
//	sem_post(&__dm.anim_sem);
//		pthread_mutex_unlock(&__dm.anim_sem);
	
//	sem_post(&__dm.anim_sem);
	return i;
}

int	dm_DelAnimsbyKind(dm_img *img, WORD kind) {
	llist_entry	*le, *n;
	dm_anim		*da;
	int			i;
//	if (!__dm.inanim)			
//	sem_wait(&__dm.anim_sem);
//		pthread_mutex_lock(&__dm.anim_sem);	
	for (i=0, le = __dm.anim_lh.f; le; le = n) {
		n = le->n;
		da = (dm_anim *) le;
		if (da->img != img)
			continue;
		if ((da->kind & kind) != kind)
			continue;

				
		del_entry(&__dm.anim_lh, le);
			
		i++;
	}
//	sem_post(&__dm.anim_sem);
//	if (!__dm.inanim)			
//	sem_post(&__dm.anim_sem);
//		pthread_mutex_unlock(&__dm.anim_sem);

	return i;
}
gimp_image	*dm_LoadPNG(char *fname) {
	gimp_image	*i;
	dm_img		*di = NULL;
	char	fn[128];
	char	*p;
	strcpy(fn, fname);
	p = strstr(fn, ".png");
	if (p)
		sprintf(p, ".%d.png", gScreen);	
	if (!(i = LoadPNG(fn))) {
	//	LogPrint("Loading png: %s\n", fname);
		i = LoadPNG(fname);
	} else
	; //	LogPrint("Loading png: %s\n", fn);
	return i;	
}
dm_img	*dm_CreatePNG(char *fname) {
	gimp_image	*i;
	dm_img		*di = NULL;
/*	char	fn[128];
	char	*p;
	strcpy(fn, fname);
	p = strstr(fn, ".png");
	if (p)
		sprintf(p, ".%d.png", gScreen);	
	if (!(i = LoadPNG(fn))) {
		LogPrint("Loading png: %s\n", fname);
		i = LoadPNG(fname);
		
	} else
		LogPrint("Loading png: %s\n", fn);
*/	
	if ((i = dm_LoadPNG(fname))) {
		
		di = dm_createimage(i);
		free(i);
	}
	return di;
}

dm_img	*dm_refreshimage(dm_img *di, gimp_image *g) {
	VC_RECT_T	dr;
	if (!di)
		return NULL;
	if (!di->g)
		return NULL;
	if ((di->g->width != g->width)
		|| (di->g->height != g->height)
	) 
		return NULL;
	if (!di->resource)
		return NULL;
	Lock();
	Invalidate();
	g->dirty = 0;
	int	r = vc_dispmanx_rect_set(&dr, 0, 0,  g->width, g->height);
	if (!r)	
		vc_dispmanx_resource_write_data(
			di->resource,
			VC_IMAGE_RGBA32,
			(int) g->width*4, 
			(void *) &g->data[0],
			&dr
		);
	if (r) {
		LogPrint("WRITE ERROR %d\r\n", r);
	}
	Unlock();
	return di;
}

dm_img	*dm_createimage(gimp_image *g) {
	dm_img				*di;
	di = (dm_img *) get_entry(&__dm.lh);
	if (!di)
		return NULL;
	bzero(((char *) di) + sizeof(llist_entry), sizeof(*di)-sizeof(llist_entry));

	Lock();
	Invalidate();
	di->g = g;
	
	di->g = calloc(sizeof(gimp_image), 1);
	if (!di->g) {
		LogPrint("PIS ERROR\r\n");
	}
	memcpy(di->g, g, sizeof(gimp_image));	
	
	vc_dispmanx_rect_set(&di->sr, 0, 0,  g->width << 16, g->height << 16);
	vc_dispmanx_rect_set(&di->dr, 0, 0,  g->width, g->height);
	di->resource = vc_dispmanx_resource_create( 
		VC_IMAGE_RGBA32,
		g->width,
		g->height,
		&di->img
	);
	if (di->resource) {
		int r = vc_dispmanx_resource_write_data(
			di->resource,
			VC_IMAGE_RGBA32,
			(int) g->width*4, 
			(void *) &g->data[0],
			&di->dr
		);
		if (r) {
			LogPrint("WRITE ERROR %d\r\n", r);
		}
	}
	di->ratio = 1.0;
	di->twidth = g->width;
	di->theight = g->height;

	Unlock();
//	LogPrint("Return : %p\n", di);
	return di;
}
dm_img	*dm_AddText(char *text, ebm_font *f, int x, int y, DWORD col, int layer, int opacity, int ispassword) {
	int			w=0, h=0, hh=0, tw, th, rw;
	gimp_image	*g;
	dm_img		*di = NULL;
	int			align = 0;
	if (*text == '-')
		align = -1;
	if (*text == '-')
		align = 1;
	if (align)
		text++;
	if (!*text)
		strcpy(text, " ");
	emeasure_string(&w, &h, &hh, text, f);
	//LogPrint("w: %d y:%d yy:%d\n", w, h, hh);	
	hh += h;
	rw = w;
	tw = w;
	th = hh;
	if (hh % 16) 
		hh = hh - (hh % 16) + 16;
	if (w % 32)
		w = w - (w % 32) + 32;
	
	if ((g = CreateImage(w, hh))) {
		png_wstr(g, 0, h, text, col, f, ispassword);
		if ((di = dm_createimage(g))) {	
			if (di->g)
				free(di->g);
			di->g = g;
			strncpy(di->oldvalue, text, 63);
			di->ispassword = ispassword;
			if (x < 0)
				x = -x - g->rwidth / 2;
			if (align)
				x = x - rw;
			di->text = text;
			di->fnt = f;
			di->toff = h;
			di->col = col;
			di->twidth = tw;
			di->theight = th;	
			dm_showimage(layer, opacity, x, y, di);
			if (!di->theight) {
				LogPrint("ZERO HEIGHT\r\n");
			}
		}
	} else {
		LogPrint("FAILED TO CREATE IMAGE\r\n");
	}
	return di;
}

//dm_img	*dm_Set
dm_img	*dm_SetText(dm_img *img, DWORD col) {
	int			xx, yy, w=0, h=0, hh=0;
	VC_RECT_T	dr;
	
	if (!strcmp(img->oldvalue, img->text) && col == img->col)
		return img;
	img->col = col;
	strcpy(img->oldvalue, img->text);
	
	emeasure_string(&w, &h, &hh, img->text, img->fnt);
//	LogPrint("w: %d y:%d yy:%d\n", w, h, hh);
	hh += h;
	if (hh % 16) 
		hh = hh - (hh % 16) + 16;
	if (w % 32)
		w = w - (w % 32) + 32;
	Lock();
	Invalidate();		
	if (w > img->g->width || hh > img->g->height) {
		VC_DISPMANX_ALPHA_T	alpha;
	
		free(img->g);
		img->g = CreateImage(w, hh);
		png_wstr(img->g, 0, img->toff, img->text, img->col, img->fnt, img->ispassword);		
		vc_dispmanx_element_remove(__dm.update, img->element);
		vc_dispmanx_resource_delete(img->resource);
		img->resource = vc_dispmanx_resource_create( 
			VC_IMAGE_RGBA32,
			img->g->width,
			img->g->height,
			&img->img
		);
		vc_dispmanx_rect_set(&img->sr, 0, 0,  img->g->width << 16, img->g->height << 16);
		vc_dispmanx_rect_set(&img->dr, 0, 0,  img->g->width, img->g->height);
		vc_dispmanx_resource_write_data(
			img->resource,
			VC_IMAGE_RGBA32,
			(int) img->g->width*4, 
			(void *) &img->g->data[0],
			&img->dr
		);
		alpha.flags =
			DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_MIX;
		alpha.opacity = img->opacity;
		alpha.mask = 0;
		xx = img->x+img->g->width/2;
		yy = img->y+img->g->height/2;
		vc_dispmanx_rect_set(&img->dr, 
			xx-img->g->width*img->ratio*0.5, 
			yy-img->g->height*img->ratio*0.5,
			img->g->width*img->ratio, 
			img->g->height * img->ratio
		);
		vc_dispmanx_rect_set(&img->sr, 0, 0, img->g->width << 16, img->g->height << 16);	
	
		img->element = vc_dispmanx_element_add(   
			__dm.update,
			__dm.display,
            img->layer,               // layer
            &img->dr,
            img->resource,
            &img->sr,
            DISPMANX_PROTECTION_NONE,
            &alpha,
            NULL,             // clamp
			VC_IMAGE_ROT0
		);
	} else {
		int	r;
		bzero(&img->g->data[0], img->g->width * img->g->height * 4);
		png_wstr(img->g, 0, img->toff, img->text, img->col, img->fnt, img->ispassword);

		r = vc_dispmanx_rect_set(&dr, 0, 0,  img->g->width, img->g->height);
		if (r) 
			LogPrint("vc_dispmanx_rect_set\r\n");
		r = vc_dispmanx_resource_write_data(
			img->resource,
			VC_IMAGE_RGBA32,
			(int) img->g->width*4, 
			(void *) &img->g->data[0],
			&dr
		);
		if (r) 
			LogPrint("vc_dispmanx_resource_write_data 2\r\n");
	}
	Unlock();
	return img;
}

void	dm_showimage(int layer, int opacity, int x, int y, dm_img *di) {
//	int	ret;
	VC_DISPMANX_ALPHA_T	alpha;
	Lock();
	Invalidate();

	alpha.flags =
		DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_MIX;
	alpha.opacity = opacity;
	alpha.mask = 0;
	if (x >= 0)
		di->x = x;
	if (y >= 0)
		di->y = y;
	if (opacity > 0)
		di->opacity = opacity;
	di->visible = 1;
	if (layer > 0)
		di->layer = layer;

	vc_dispmanx_rect_set(&di->dr, x+di->g->width*(1.0-di->ratio), y+di->g->height*(1.0-di->ratio),  di->g->width*di->ratio, di->g->height*di->ratio);
	vc_dispmanx_rect_set(&di->sr, 0, 0, di->g->width << 16, di->g->height << 16);	
  
	if (di->element) {
		fprintf(stderr, "SUP SHOW\n");
		Unlock();
		return;
		
	}
	di->element = vc_dispmanx_element_add(   
			__dm.update,
			__dm.display,
            layer,               // layer
            &di->dr,
            di->resource,
            &di->sr,
            DISPMANX_PROTECTION_NONE,
            &alpha,
            NULL,             // clamp
			VC_IMAGE_ROT0
		);/*
	vc_dispmanx_element_change_attributes(
			__dm.update,
			di->element,
			2, //((opacity >= 0 && opacity < 256)? 0x8 : 0),
			0,
			opacity*di->visible, // layer
			&di->dr,
			NULL,
			0,
			0
		);		*/
	Unlock();
}


void	dm_moveimage(int x, int y, dm_img *di, int opacity) {
	int		xx, yy;
	int		dx, dy;
	dm_img	*i;

	Lock();
	Invalidate();
	
	dx = x - di->x;
	dy = y - di->y;
	
	di->x = x;
	di->y = y;
	
	xx = di->x+di->g->width/2;
	yy = di->y+di->g->height/2;
	vc_dispmanx_rect_set(&di->dr, 
		xx-di->g->width*di->ratio*0.5, 
		yy-di->g->height*di->ratio*0.5,
		di->g->width*di->ratio, 
		di->g->height * di->ratio
	);
	//printf("Opacity : %d\n", opacity);
	vc_dispmanx_element_change_attributes(
		__dm.update,
		di->element,
		4, //((opacity >= 0 && opacity < 256)? 0x8 : 0),
		0,
		opacity,               // layer
		&di->dr,
		NULL,
		0,
		0
	);
	if (opacity >= 0 && opacity < 256) {
		//int	ret;
		//ret = 
		vc_dispmanx_element_change_attributes(
			__dm.update,
			di->element,
			2, //((opacity >= 0 && opacity < 256)? 0x8 : 0),
			0,
			opacity*di->visible, // layer
			&di->dr,
			NULL,
			0,
			0
		);		
		di->opacity = opacity;
//		LogPrint("%p << %4d >> %4d\n", di, ret, opacity);
	}
	Unlock();
//	Validate();
	for (i = di->fi; i; i = i->ne) {
		i->visible = di->visible;
		dm_moveimage(i->x + dx, i->y + dy, i, opacity);		
	}
}



void	dm_fadeimage(int x, int y, dm_img *di, int opacity) {
	int		xx, yy;
	int		dx, dy;
	dm_img	*i;
	
	Lock();
	Invalidate();

	dx = x - di->x;
	dy = y - di->y;
	di->x = x;
	di->y = y;
	
	xx = di->x+di->g->width/2;
	yy = di->y+di->g->height/2;
	vc_dispmanx_rect_set(&di->dr, 
		xx-di->g->width*di->ratio*0.5, 
		yy-di->g->height*di->ratio*0.5,
		di->g->width*di->ratio, 
		di->g->height * di->ratio
	);
	
	if (opacity >= 0 && opacity < 256) {
//		int	ret;
//		ret = 
		vc_dispmanx_element_change_attributes(
			__dm.update,
			di->element,
			2, //((opacity >= 0 && opacity < 256)? 0x8 : 0),
			0,
			opacity*di->visible, // layer
			NULL,
			NULL,
			0,
			0
		);		
		di->opacity = opacity;
		//LogPrint("%p %4d %4d\n", di, ret, opacity);
	}
	Unlock();
	for (i = di->fi; i; i = i->ne)
		dm_moveimage(i->x + dx, i->y + dy, i, opacity);
}

void	SetAllTransparent(int v) {
	llist_entry	*le;
	dm_img		*img;
	
	for (le = __dm.lh.f; le; le = le->n) {
		img = (dm_img *) le;
		if (!v) {
			if (img->alwaysvisible)
				continue;
			if (img->pa)
				if (img->pa->alwaysvisible)
					continue;
		}
//		LogPrint("img : %p - %d\n", img, img->opacity);
		if (!v)
			img->visible = 0;
		
		dm_moveimage(img->x, img->y, img, img->opacity);

		LogPrint("img : %p - %d\n", img, img->opacity);
	}
//	Unlock();
}


void	dm_HideImage(dm_img *img) {
	img->visible =0;
	dm_moveimage(img->x, img->y, img, img->opacity);
	dm_DelAnims(img);
}

void	dm_UnhideImage(dm_img *img) {
	img->visible = 1;
	dm_moveimage(img->x, img->y, img, img->opacity);	
}

void	ShowAll(void) {
	llist_entry	*le;
	dm_img		*img;
	
	for (le = __dm.lh.f; le; le = le->n) {
		img = (dm_img *) le;
		if (img == gLogo)
			continue;
		if (img->visible)
			continue;
		img->visible = 1;
		dm_moveimage(img->x, img->y, img, img->opacity);
	}
	gCursor->visible = 1;
	dm_moveimage(__mx, __my, gCursor, -1);
}

void	CheckTexts(void) {
	llist_entry	*le;
	dm_img		*img;
	
	for (le = __dm.lh.f; le; le = le->n) {
		img = (dm_img *) le;
		if (img == gLogo)
			continue;
		if (!img->visible)
			continue;

		if (!img->text)
			continue;
		dm_SetText(img, img->col);
	}
}

void	dm_Animate(DWORD delta) {
	llist_entry	*le, *n;
	dm_anim		*da;


//	if (!__dm.bgr)
//		return;	
//	sem_wait(&__dm.anim_sem);
	pthread_mutex_lock(&__dm.anim_sem);
	
//	__dm.inanim = 1;
	for (le = __dm.anim_lh.f; le; le = n) {
		da = (dm_anim *) le;
		n = le->n;
		if (!da->img || da->delete) {
			da->delete = 0;
			del_entry(&__dm.anim_lh, le);
			continue;
		}
		if (!da->img->visible)
			continue;	
		if (!da->active) {
			//goto l1;
			continue;
		}
		da->pos += delta;
		if (da->pos > da->duration) {
			da->pos = da->duration;
		}		
		da->r = (da->pos * 1.0) / (da->duration * 1.0);

		if (da->f)
			da->r = da->f(da->r, da->img);
			
		if (da->kind & ANIM_SIN) {
				da->r = 1.0 * sin(M_PI * da->r);
		}
		if (da->kind & ANIM_SQR) {
				da->r *= da->r;
		}

		da->cur = da->min + ((da->max - da->min) * da->r);
		
		if (da->kind & ANIM_X) {
			dm_moveimage(da->cur, da->img->y, da->img, -1);
		} else if (da->kind & ANIM_Y) {
			dm_moveimage(da->img->x, da->cur, da->img, -1);
		} else if (da->kind & ANIM_TRANS) {
			dm_fadeimage(da->img->x, da->img->y, da->img, da->cur);
		}
		if (da->kind & ANIM_XW) {
			da->img->ratio = da->cur / 100.0;
			//LogPrint("da->cur = %d, %d:%d\n", da->cur, da->img->x, da->img->y);
			
			dm_moveimage(da->img->x, da->img->y, da->img, -1);
		} 
//l1:
		if (da->pos >= da->duration) {
			if (da->kind & ANIM_LOOP)
				da->pos = 0;
			else {
				da->active = 0;
				if (da->complete) {
					if (!(*da->complete)(da->img, da, da->data)) {
//						del_entry(&__dm.anim_lh, le);
						continue;
					}					
				}
				del_entry(&__dm.anim_lh, le);
				continue;
			}
			
		}	
	}
//	__dm.inanim = 0;	
	pthread_mutex_unlock(&__dm.anim_sem);
}

void	dm_attach(dm_img *p, dm_img *c) {
		if (!p || !c)
			return;
		if (c->pa) {
			if ((c->pa == p))
				return;
			else
				dm_detach(c);				
		}
		c->pa = p;
		c->pr = p->la;
		c->ne = NULL;
		c->alwaysvisible = p->alwaysvisible;
		if (p->la) 
			p->la->ne = c;			
		else 
			p->fi = c;
		p->la = c;
}

void	dm_detach(dm_img *c) {
	if (!c)
		return;
	if (!c->pa)
		return;
	if (c->ne) {
		if ((c->ne->pr = c->pr))
			c->pr->ne = c->ne;
	} else {
		if ((c->pa->la = c->pr))
			c->pr->ne = NULL;
	}
	if (c->pr) {
		if ((c->pr->ne = c->ne))
			c->ne->pr = c->pr;
	} else {
		if ((c->pa->fi = c->ne))
			c->ne->pr = NULL;
	}
}

void	DumpImgs(void) {
	llist_entry	*le;
	dm_img	*img;
	int		i = 0;
	LogPrint("------------- %4d -------------\r\n", __dm.lh.c);
	//if (pthread_mutex_trylock(&__ssync)) {
		for (le = __dm.lh.f; le; le = le->n, i++) {
			img = (dm_img *) le;
			LogPrint("%4d: %X %d,%d %dx%d\t\n", i, img->resource, img->x, img->y, img->twidth, img->theight);
		}
	//	pthread_mutex_unlock(&__ssync);
	//} 

	LogPrint("--------------------------------\r\n\r\n");
}