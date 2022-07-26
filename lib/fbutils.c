#define	fbutils_c

#include 	<stdio.h>
#include 	<stdlib.h>
#include 	<string.h>
#include 	<unistd.h>
#include 	<sys/fcntl.h>
#include 	<sys/ioctl.h>
#include 	<sys/mman.h>
#include 	<sys/time.h>
#include	<errno.h>

#include 	<linux/vt.h>
#include 	<linux/kd.h>
#include 	<linux/fb.h>
#include	<semaphore.h>
#include	<png.h>

#include	"fbutils.h"

int	_yoffset = 1080;
int	_dirty = 1;

#define S3CFB_CHANGE_REQ		_IOW ('F', 308, int)
#define S3CFB_GET_NUM                   _IOWR('F', 306, unsigned int)


static int 				con_fd, last_vt = -1;
static struct			fb_fix_screeninfo fix;
static struct 			fb_var_screeninfo var;
static unsigned char 	*fbuffer;
 unsigned char 	**line_addr;

static int 				bytes_per_pixel;
//static unsigned 		colormap [256];
DWORD 					xres, yres;
//static char		 		*defaultfbdevice = "/dev/fb0";
//static char 			*defaultconsoledevice = "/dev/tty";
static char 			*fbdevice = NULL;
static char 			*consoledevice = NULL;

int	switchfb(void) {
	int	crtc;
	int	oo = _yoffset;

	if (_lock) {
		LogPrint("Lock\r\n");
		return -1;
	}
	if (_screen_saver)
		return 2;
	if (!_dirty)
		return 0;
	
//v3	if (sem_trywait(&fbsem))
	if (pthread_mutex_trylock(&fbsem)) 
		return 0;
	
	var.yoffset = _yoffset;
	_yoffset=_yoffset?0:yres;
	memcpy(line_addr[_yoffset], line_addr[oo], xres*yres*2);
	ioctl(fb_fd, FBIOPAN_DISPLAY, &var);
	crtc = 0;
	ioctl(fb_fd, FBIO_WAITFORVSYNC, &crtc);
	_dirty = 0;	
	//v3 sem_post(&fbsem);
	pthread_mutex_unlock(&fbsem);
	return 1;
}

int open_framebuffer(gimp_image *img) {
	struct	vt_stat	vts;
	char 			vtname[128];
	int 			fd, nr;
	unsigned 		y, addr;
	int				err;

	if (fb_fd > 0)
		return -1;
	_lock = 0;
	LogPush("open_framebuffer");

//	if ((fbdevice = getenv ("TSLIB_FBDEVICE")) == NULL)
//		fbdevice = defaultfbdevice;
	fbdevice = "/dev/fb0";
	consoledevice = "/dev/tty";

	sprintf (vtname,"%s%d", consoledevice, 1);
	fd = open (vtname, O_WRONLY);
	if (fd < 0) {
		err = errno;
		LogPrint("open console:%s\n", strerror(err));
		LogPop();
		return err;
	}

	if (ioctl(fd, VT_OPENQRY, &nr) < 0) {
		err = errno;
		close(fd);
		LogPrint("VT_OPENQRY:%s\n", strerror(err));
		LogPop();
		return err;
	}
	close(fd);

	sprintf(vtname, "%s%d", consoledevice, nr);

	con_fd = open(vtname, O_RDWR | O_NDELAY);
	if (con_fd < 0) {
		err = errno;
		LogPrint("open vt:%s\n", strerror(err));
		LogPop();
		return err;
	}
	
	if (ioctl(con_fd, VT_GETSTATE, &vts) == 0)
		last_vt = vts.v_active;
//	last_vt = 1;
	if (ioctl(con_fd, VT_ACTIVATE, nr) < 0) {
		err = errno;
		LogPrint("VT_ACTIVATE:%s\n", strerror(err));
		close(con_fd);
		LogPop();
		return err;
	}

#ifndef TSLIB_NO_VT_WAITACTIVE
	if (ioctl(con_fd, VT_WAITACTIVE, nr) < 0) {
		//perror("VT_WAITACTIVE");
		err = errno;
		LogPrint("VT_WAITACTIVE:%s\n", strerror(err));
		close(con_fd);
		LogPop();
		return err;
	}
#endif

	if (ioctl(con_fd, KDSETMODE, KD_GRAPHICS) < 0) {
		err = errno;
		LogPrint("KDSETMODE:%s\n", strerror(err));
		close(con_fd);
		LogPop();
		return err;
	}

	fb_fd = open(fbdevice, O_RDWR);
//	printf("fb device = \"%s\"\n", fbdevice);
	if (fb_fd == -1) {
		err = errno;
		LogPrint("open fbdevice:%s\n", strerror(err));
		close(con_fd);
		LogPop();	
		return err;
	}

	if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		close(fb_fd);
		fb_fd = -1;
		err = errno;
		LogPrint("FBIOGET_FSCREENINFO:%s\n", strerror(err));
		LogPop();	
		return err;
	}

	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var) < 0) {
		close(fb_fd);
		fb_fd = -1;
		err = errno;
		LogPrint("FBIOGET_VSCREENINFO:%s\n", strerror(err));
		LogPop();			
		return err;
	}

	xres = var.xres;
	yres = var.yres;
	_yoffset = yres;
printf("xres = %d\n", xres);
printf("yres = %d\n", yres);
printf("virtual x = \"%d\"\n", var.xres_virtual);
printf("virtual y = \"%d\"\n", var.yres_virtual);
printf("x offset = \"%d\"\n", var.xoffset);
printf("y offset = \"%d\"\n", var.xoffset);
printf("smem len = \"%d\"\n", fix.smem_len/xres*2);
printf("xpan step = \"%d\"\n", fix.xpanstep);
printf("ypan step = \"%d\"\n", fix.ypanstep);
printf("mmio start = \"%lX\"\n", fix.mmio_start);
printf("mmio length = \"%d\"\n", fix.mmio_len);

printf("bits_per_pixel = \"%d\"\n", var.bits_per_pixel);
printf("id = \"%s\"\n", fix.id);
printf("red = \"%d\"\n", var.red.length);
printf("green = \"%d\"\n", var.green.length);
printf("blue = \"%d\"\n", var.blue.length);
printf("trans = \"%d\"\n", var.transp.length);
printf("fix.smem_len = \"%d\"\n", fix.smem_len);

//getchar();
{
	var.xoffset = 0;
	var.yoffset = 0;
	var.activate = FB_ACTIVATE_NOW;
	var.bits_per_pixel = 16;
	var.yres_virtual = var.yres * 2;
	if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &var) == -1) {
		printf("Could not put\n");
		goto l1;
	}
	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var) == -1) {
		goto l1;
	}
	if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) == -1) {
		goto l1;
	}
	if (var.yres_virtual < var.yres * 2) {
		printf("no double buffering\n");
	}
	if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) == -1) {
		goto l1;
	}
/*
	printf("smem_len/1600 = %d\n", fix.smem_len/1600);
	printf("var yres_virtual  = %d\n", var.yres_virtual);
	
	printf("mem: %d l: %d\n", fix.smem_len, fix.line_length * var.yres_virtual);
*/
l1:
	;
}
	fbuffer = mmap(NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fb_fd, 0);
	if (fbuffer == (unsigned char *)-1) {
		err = errno;
		close(fb_fd);
		fb_fd = -1;
		LogPrint("mmap framebuffer:%s\n", strerror(err));
		LogPop();	
		return err;
	}
	if (img) 
		memcpy(fbuffer, img->data, 1920*1080*2);
	else
		memset(fbuffer, 0x0, fix.smem_len);
//	printf("fix.smem_len = \"%d\"\n", fix.smem_len);	
	bytes_per_pixel = (var.bits_per_pixel + 7) / 8;
	line_addr = malloc (sizeof (__u32) * var.yres_virtual);
	addr = 0;
	
	for (y = 0; y < var.yres_virtual; y++, addr += fix.line_length)
		line_addr [y] = fbuffer + addr;
	LogPop();
	return 0;
}

void close_framebuffer(void) {
	if (fb_fd <= 0)
		return;
		
	munmap(fbuffer, fix.smem_len);
	close(fb_fd);
	fb_fd = -1;
	if(strcmp(consoledevice, "none") != 0) {
		if (ioctl(con_fd, KDSETMODE, KD_TEXT) < 0)
			perror("KDSETMODE");

		if (last_vt >= 0)
			if (ioctl(con_fd, VT_ACTIVATE, last_vt))
				perror("VT_ACTIVATE");

		close(con_fd);
	}
	free (line_addr);
}

gimp_image	*LoadImage(char *fname) {
	int		w, h, i; // r
	FILE	*fp;
	gimp_image	*img = NULL;
	unsigned short	*d;
	
	if (!(fp = fopen(fname, "r")))
		goto l1;
	fread(&w, sizeof(w), 1, fp);
	fread(&h, sizeof(h), 1, fp);
//	printf("%s %d x %d\n", fname, w, h);
	if (!(img = calloc(sizeof(gimp_image)+w*h*2, 1)))
		goto l2;
//	printf("Size %d\n", w*h*2);
	img->width = w;
	img->height = h;
	d = &img->data[0];
//	printf("%s %d x %d %p\n", fname, img->width, img->height, d);
	
	for (i=0; i < img->height; i++, d += w) {
//		printf("%d %p\n", i, d);
		fread(d, w, 2, fp);
	}
//	printf("%s loaded\n", fname);
	goto l1;
//l3:	
	free(img);
	img = NULL;
l2:
	fclose(fp);
l1:
	return img;
}

void	blt(int x, int y, int w, int h, int ox, int oy, gimp_image *img) {
	unsigned short	*s, *d;
	int	i; //, j;
	
	if (!img)
		return;
	if (ox >= img->width)
		return;
	if (oy >= img->height)
		return;
	_dirty = 1;
	if (w >= (ox+img->width)) {
		w = img->width - ox;
	}
	if (h>= (oy+img->height)) {
		h = img->height - oy;
	}
	
	s = &img->data[0];
	s += oy*img->width + ox;
//	printf("w: %d, h: %d\n", w, h);
	for (i = 0; i < h; i++, s += img->width) {
		d = (unsigned short *) line_addr[i+y+_yoffset];
		d += x;
		memcpy(d, s, w*2);
	}
}

void	blttrans(int x, int y, int w, int h, int ox, int oy, gimp_image *img) {
	unsigned short	*s, *d, *ss;
	unsigned short	bkg;
	
	int	i; //, j;
	
	if (!img)
		return;
	if (ox >= img->width)
		return;
	if (oy >= img->height)
		return;
	_dirty = 1;
	if (w >= (ox+img->width)) {
		w = img->width - ox;
	}
	if (h>= (oy+img->height)) {
		h = img->height - oy;
	}
	s = &img->data[0];
	bkg = *s;
	s += oy*img->width + ox;
//	printf("w: %d, h: %d, %d %d\n", w, h, img->width, img->height);
	for (i = 0; i < h; i++, s += img->width) {
		int	j;
		d = (unsigned short *) line_addr[i+y+_yoffset];
		d += x;
		for (j=0, ss = s; j<w; j++, ss++, d++) {
			if (*ss != bkg) 
				*d = *ss;
		}
	}
}

void emeasure_char(int *ax, int *ay, unsigned char c, ebm_font *f) {
	ebm_font_char	*ec;
	ebm_font_char	*e0;
//	ebm_font_char	*eg;
	
	int	h;
	if (!f)
		return;
	ec = &f->c[c];
	e0 = &f->c[10];
//	eg = &f->c['g'];
	h = e0->top;
	if (h >= *ay)
		*ay = h;
/*	
	h = ec->top;
	if (h >= *ay)
		*ay = h;
*/	ec = &f->c[c];
	*ax += ec->xoff;
}

void emeasure_char2(int *ax, int *ay, int *ayy, unsigned char c, ebm_font *f) {
	ebm_font_char	*ec;
	ebm_font_char	*e0;
	ebm_font_char	*eg;
	int	h;
	if (!f)
		return;
	ec = &f->c[c];
	e0 = &f->c[10];
	eg = &f->c['g'];
	
	h = e0->top;
	if (h > *ay)
		*ay = h;
	
//	h = ec->top;
//	if (h > *ay)
//		*ay = h;

	h = eg->height-eg->top;
	if (h > *ayy)
		*ayy = h;
//	h = ec->height-ec->top;
//	if (h > *ayy)
//		*ayy = h;

	*ax += ec->xoff;
}

void eput_char(int *ax, int *ay, unsigned char c, unsigned short col, ebm_font *f) {
	int i, j, x, y; // k
	unsigned char	*bits;
	unsigned short	*d, v;
	ebm_font_char	*ec;

	if (!f)
		return;
	ec = &f->c[c];
	x = *ax;
	y = *ay;
	if (ec->left >= 200)
		ec->left = 0;
	y -= ec->top;
	if (ec->left > ec->width || ec->xoff > ec->width*2)
		; //printf("%c l:%d w:%d xo:%d\n", c, ec->left, ec->width, ec->xoff);
	x += ec->left;
	
	bits = (unsigned char *) f;
	bits += ec->pos;

	for (i = 0; i < ec->height; i++) {
		if (i + y < 0)
			continue;
		if (i + y >= yres)
			continue;
		d = (unsigned short *) line_addr[i + y+_yoffset];
		d += x;		
		for (j = 0; j < ec->width; j++, bits++, d++) {
			if (x < 0)
				continue;
			if (x >= xres)
				continue;
			if ((v = *bits)) {
//				if (v == 0xff)
//					*d = col;
//				else {
				 {
					int	r, g, b, r2, b2, g2;
					double	oran = v / 255.0;	
					b = col & 31;
					g = (col >> 5) & 63;
					r = (col >> 11) & 31;

					b2 = *d & 31;
					g2 = (*d >> 5) & 63;
					r2 = (*d >> 11) & 31;
					
					r = (1.0-oran)*r2 + oran * r;
					r &= 0xff;
					g = (1.0-oran)*g2 + oran * g;
					g &= 0xff;
					b = (1.0-oran)*b2 + oran * b;
					b &= 0xff;
					
					*d =  (r << 11) | (g << 5) | b;
				}
			};
			
		}
//		printf("\n");
	}
	*ax += ec->xoff; //+ec->bbox;
	*ay += ec->yoff;
	_dirty = 1;
}

void eput_string(int x, int y, char *s, unsigned short col, ebm_font *f) {
	int i;
	int	st = 0;
	int	bx = x;
	if (!f)
		return;

	for (i = 0; *s; i++, s++) {
		if (*s == 0xc3 || *s == 0xc4 || *s == 0xc5)
			st = *s;
		else {
			if (st) {
				st = st << 8 | *s;
				switch (st) {
				case 0xc387: st = 0; break;
				case 0xc3a7: st = 1; break;
				case 0xc49e: st = 2; break;
				case 0xc49f: st = 3; break;
				case 0xc4b0: st = 4; break;
				case 0xc4b1: st = 5; break;
				case 0xc396: st = 6; break;
				case 0xc3b6: st = 7; break;
				case 0xc59e: st = 8; break;
				case 0xc59f: st = 9; break;
				case 0xc39c: st = 10; break;
				case 0xc3bc: st = 11; break;
				default:
					st = *s;
				}
				eput_char (&x, &y, st, col, f);
				st = 0;
			} else if (*s == '\n') {
				x = bx;
				y += f->c[10].height * 1.5;
			} else
				eput_char (&x, &y, *s, col, f);
		}
	}
	_dirty = 1;
}

void eput_string_center(int x, int y, char *s, unsigned short col, ebm_font *f) {
//	int i;
	int	xx = 0, yy = 0, yyy=0;
//	char	*ss = s;
	if (!f)
		return;
	emeasure_string(&xx, &yy, &yyy, s, f);
/*	for (i = 0; *s; i++, s++) {
		emeasure_char2(&xx, &yy, &yyy, *s, f);
	}
*/
//y -= yy;
//	y -= yy/2;
	x -= xx/2;
	eput_string(x, y, s, col, f);
/*
	for (s= ss, i = 0; *s; i++, s++) {
		eput_char (&x, &y, *s, col, f);
	}
*/
}

void eput_stringb(int x, int y, char *s, unsigned short col, ebm_font *f, ebounds *b) {
	int i;
	int	st = 0;
	int	xx = 0, yy = 0, yyy=0;
	if (!f)
		return;
	emeasure_string(&xx, &yy, &yyy, s, f);

	if (x < 0) {
		x = xres + x - xx;
	}
	if (y < 0) {
		y = -y+yy;
	}
	if (b) {
		b->x = x; b->xx = xx+x;
		b->y = y-yy; b->yy = y+yyy;
	}
	for (i = 0; *s; i++, s++) {
		if (*s == 0xc3 || *s == 0xc4 || *s == 0xc5)
			st = *s;
		else {
			if (st) {
				st = st << 8 | *s;
				switch (st) {
				case 0xc387: st = 0; break;
				case 0xc3a7: st = 1; break;
				case 0xc49e: st = 2; break;
				case 0xc49f: st = 3; break;
				case 0xc4b0: st = 4; break;
				case 0xc4b1: st = 5; break;
				case 0xc396: st = 6; break;
				case 0xc3b6: st = 7; break;
				case 0xc59e: st = 8; break;
				case 0xc59f: st = 9; break;
				case 0xc39c: st = 10; break;
				case 0xc3bc: st = 11; break;
				default:
					st = *s;
				}
				eput_char (&x, &y, st, col, f);
				st = 0;
			} else
				eput_char (&x, &y, *s, col, f);
		}
	}
	_dirty = 1;
}

void eput_string_centerb(int x, int y, char *s, unsigned short col, ebm_font *f, ebounds *b) {
//	int i;
	int	xx = 0, yy = 0, yyy=0;
//	char	*ss = s;
	if (!f)
		return;
	emeasure_string(&xx, &yy, &yyy, s, f);
/*	for (i = 0; *s; i++, s++) {
		emeasure_char2(&xx, &yy, &yyy, *s, f);
	}
*/
//y -= yyy+yy;
//	y -= yy/2;
	x -= xx/2;
	eput_string(x, y, s, col, f);
	b->x = x; b->xx = xx+x;
	b->y = y; b->yy = y+yyy+yy;
/*
	for (s= ss, i = 0; *s; i++, s++) {
		eput_char (&x, &y, *s, col, f);
	}
*/
}

void	emeasure_string(int *x, int *y, int *yy, char *s, ebm_font *f) {
	int	i, st = 0;
	if (!f)
		return;
	*x = 0; *y = 0; *yy = 0;
	for (i = 0; *s; i++, s++) {
		if (*s == 0xc3 || *s == 0xc4 || *s == 0xc5)
			st = *s;
		else {
			if (st) {
				st = st << 8 | *s;
				switch (st) {
				case 0xc387: st = 0; break;
				case 0xc3a7: st = 1; break;
				case 0xc49e: st = 2; break;
				case 0xc49f: st = 3; break;
				case 0xc4b0: st = 4; break;
				case 0xc4b1: st = 5; break;
				case 0xc396: st = 6; break;
				case 0xc3b6: st = 7; break;
				case 0xc59e: st = 8; break;
				case 0xc59f: st = 9; break;
				case 0xc39c: st = 10; break;
				case 0xc3bc: st = 11; break;
				default:
					st = *s;
				}
				emeasure_char2(x, y, yy, st, f);
				st = 0;
			} else if (*s == '\n') {
				int	x2 = 0, y2 = 0, yy2 = 0;
				int	a;
				emeasure_string(&x2, &y2, &yy2, s+1, f);
				*x = MAX(*x, x2);
				//LogPrint("MEASURE %d %d %d %d\n", *y, *yy, y2, yy2);
				a = (1.5 * f->c[10].height);
				*yy = a + yy2 + *y;
				*y = *yy;
				
				*y /= 2;
				*yy /= 2;
				*yy += 5;
				*x += 5;
				//LogPrint("MEASURED %d %d %d\n", *x, *y, *yy);
				return;
			} else
				emeasure_char2(x, y, yy, *s, f);
		}
	}
}

void efill_rect (int x1, int y1, int x2, int y2, unsigned short col, int op) {
	unsigned short *p;
	int	j;
	
	/* Clipping and sanity checking */
	if (x1 > x2) { j = x1; x1 = x2; x2 = j; }
	if (y1 > y2) { j = y1; y1 = y2; y2 = j; }
	if (x1 < 0) x1 = 0; if ((__u32)x1 >= xres) x1 = xres - 1;
	if (x2 < 0) x2 = 0; if ((__u32)x2 >= xres) x2 = xres - 1;
	if (y1 < 0) y1 = 0; if ((__u32)y1 >= yres) y1 = yres - 1;
	if (y2 < 0) y2 = 0; if ((__u32)y2 >= yres) y2 = yres - 1;

	if ((x1 > x2) || (y1 > y2))
		return;

	switch (op) {
		case 0: // COPY
		for (; y1 <= y2; y1++) {
			p = (unsigned short *) line_addr [y1 +_yoffset];
			p += x1;
			for (j = x1; j <= x2; j++) 
				*(p++) = col;
		}
		break;
		case 1: // XOR
		for (; y1 <= y2; y1++) {
			p = (unsigned short *) line_addr [y1+_yoffset];
			p += x1;
			for (j = x1; j <= x2; j++) 
				*(p++) ^= col;
		}
		break;
		case 2: // OR
		for (; y1 <= y2; y1++) {
			p = (unsigned short *) line_addr [y1+_yoffset];
			p += x1;
			for (j = x1; j <= x2; j++) {
				unsigned short	s = *p;
				unsigned char r, g, b;
				r = (s >> 11) & 31;
				g = (s >> 5) & 63;
				b = s & 31;
			//	r /= 2;
			//	g /= 2;
			//	b /= 2;
				r = (r + ((col >> 11) & 31)) << 2;
				g = (g + ((col >> 5) & 63)) << 1;
				b = (b + (col & 31)) << 2;
			//	r <<= 3;
			//	g <<= 2;
			//	b <<= 3;
				 
				*(p++) = RGB16(r, g, b);
			}
			
		}
		break;
		default:
		for (; y1 <= y2; y1++) {
			p = (unsigned short *) line_addr [y1+_yoffset];
			p += x1;
			for (j = x1; j <= x2; j++) {
				unsigned short	s = *p;
				unsigned char r, g, b;
				r = (s >> 11) & 31;
				g = (s >> 5) & 63;
				b = s & 31;
				r = (r * op) / 100;
				g = (g * op) / 100;
				b = (b * op) / 100;
			//	r /= 2;
			//	g /= 2;
			//	b /= 2;
				r += (((((col >> 11) & 31)) << 3) * (100-op))/100;
				g += (((((col >> 5) & 63)) << 2) * (100-op))/100;
				b += ((((col & 31)) << 3) * (100-op))/100;
			//	r <<= 3;
			//	g <<= 2;
			//	b <<= 3;
				 
				*(p++) = RGB16(r, g, b);
			}
			
		}
	}
	_dirty = 1;
}

void	erect(int x, int y, int xx, int yy, int w, WORD col, int op) {
//	int	ix, iy; //, ixx; //, iyy;

	efill_rect(x, y, xx, y+w, col, op);
	efill_rect(x, yy-w, xx, yy, col, op);
	efill_rect(x, y+w+1, x+w, yy-w-1, col, op);
	efill_rect(xx-w, y+w+1, xx, yy-w-1, col, op);
}
#include	<pthread.h>

void	*fbThread(void *data);
int			_fbThread_running = 0;
int			_fbThread_stop = 0;
pthread_t	_fb_thread;


int	fbThread_start(void) {
	if (_fbThread_running)
		return 1;
	//v3 sem_init(&fbsem, 0, 1);
	//v3 sem_init(&invsem, 0, 1);
	pthread_mutex_init(&fbsem, NULL);
	pthread_mutex_init(&invsem, NULL);
	
	_fbThread_stop = 0;
	pthread_create(&_fb_thread, NULL, fbThread, NULL);
	return 0;
}

int	fbThread_stop(void) {
	if (!_fbThread_running)
		return 1;
//	sem_post(&invsem);
	pthread_mutex_unlock(&invsem);
	_fbThread_stop = 1;
	while (_fbThread_running) {
		usleep(100000);
	}
//	sem_destroy(&invsem);
	pthread_mutex_destroy(&invsem);
	return 0;
}

void	*fbThread(void *data) {
	int	crtc;
	int	oo = _yoffset;
	int	a = 0;
//	int	sval = 0;
	
	_fbThread_running = 1;
	while (!_fbThread_stop) {
//		if (sem_wait(&invsem))
		if (pthread_mutex_lock(&invsem))
			continue;
		if (_slock) {
			LogPrint("SSSLock\r\n");
			goto l1;
		}
		if (_lock) {
			LogPrint("Lock\r\n");
			goto l1;
		}
		if (_screen_saver)
			goto l1;
		if (!_dirty)
			goto l1;
		//v3sem_wait(&fbsem);
		pthread_mutex_lock(&fbsem);
//		LogPrint("SEM 2 IN\n");
		_lock = 1;
		_dirty = 0;
		var.yoffset = _yoffset;
		_yoffset=_yoffset?0:yres;
		memcpy(line_addr[_yoffset], line_addr[oo], xres*yres*2);
		ioctl(fb_fd, FBIOPAN_DISPLAY, &var);	
		crtc = 0;
		ioctl(fb_fd, FBIO_WAITFORVSYNC, &crtc);
		a++;	
		_lock = 0;
		printf("\n\rFRAMES %d\n\r", a);
		fflush(stdout);
//		LogPrint("SEM 2 out\n");
		//v3sem_post(&fbsem);
		pthread_mutex_unlock(&fbsem);
//		sem_getvalue(&invsem, &sval);
//		LogPrint("Sem Value : %d\n", sval);
//		sem_post(&invsem);
		pthread_mutex_unlock(&invsem);
		continue;
l1:
		sleep(0);
	}
	printf("\n\rFRAMES %d\n\r", a);
	fflush(stdout);
	_fbThread_running = 0;
	return NULL;
}

void	SwitchFB(int i) {
	if (ioctl(con_fd, VT_ACTIVATE, i))
		perror("VT_ACTIVATE");	
}

gimp_image	*LoadPNG(char *fname) {
	FILE		*fp = fopen(fname, "rb");
	char		buf[4096];
	gimp_image	*g = NULL;
	int width,	height, lwidth, lheight;
	png_byte	color_type;
	png_byte	bit_depth;

	png_structp	png_ptr = NULL;
	png_infop	info_ptr = NULL;
	int			number_of_passes;
	png_bytep	*row_pointers;
	int			i;
	if (!fp)
		return NULL;
	if (fread(buf, 1, 4, fp) != 4)
		goto l1;
	if (png_sig_cmp((png_bytep) buf, 0, 4))
		goto l1;
	
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		goto l1;
	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		goto l2;

	if (setjmp(png_jmpbuf(png_ptr)))
		goto l2;
	
	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 4);

	png_read_info(png_ptr, info_ptr);

	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	if ((lwidth = width & 31))
		lwidth = width + 32 - lwidth;
	else
		lwidth = width;
	if ((lheight = height & 15))
		lheight = height + 15 - lheight;
	else
		lheight = height;
	color_type = png_get_color_type(png_ptr, info_ptr);
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);

	number_of_passes = png_set_interlace_handling(png_ptr);
	if (bit_depth == 16)
		png_set_strip_16(png_ptr);

	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);

  // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

  // These color_type don't have an alpha channel then fill it with 0xff.
	if (color_type == PNG_COLOR_TYPE_RGB ||
		color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_PALETTE
	)
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

	if (color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_GRAY_ALPHA
	)
		png_set_gray_to_rgb(png_ptr);

	png_read_update_info(png_ptr, info_ptr);
	row_pointers = (png_bytep*) calloc(sizeof(png_bytep), height);
	if (!row_pointers)
		goto l2;
	g = calloc(sizeof(gimp_image) + 4 * lwidth * lheight, 1);
	if (!g)
		goto l3;
	for (i=0; i<height; i++) {
		row_pointers[i] = (png_bytep) &g->data[i * lwidth * 2];
	}
	png_read_image(png_ptr, row_pointers);
	g->width = lwidth;
	g->height = lheight;
	g->rheight = height;
	g->rwidth = width;
	g->bytes_per_pixel = 4;
l3:
	free(row_pointers);
l2:
	png_destroy_read_struct(&png_ptr, info_ptr?(&info_ptr):NULL, NULL);;
l1:
	fclose(fp);
	return g;
}

void png_wchar(gimp_image *img, int *ax, int *ay, unsigned char c, DWORD rgba, ebm_font *f) {
	int i, j, x, y; // k
	unsigned char	*bits;
	unsigned char	v;
	ebm_font_char	*ec;
	unsigned	char	*cp;

	if (!f)
		return;
	ec = &f->c[c];
	x = *ax;
	y = *ay;
	if (ec->left >= 200)
		ec->left = 0;
	y -= ec->top;
	if (ec->left > ec->width || ec->xoff > ec->width*2)
		; //printf("%c l:%d w:%d xo:%d\n", c, ec->left, ec->width, ec->xoff);
	x += ec->left;
	
	bits = (unsigned char *) f;
	bits += ec->pos;

	for (i = 0; i < ec->height; i++) {
		if (i + y < 0)
			continue;
		if (i + y >= img->height)
			continue;
		cp = (unsigned char *) img->data;
		cp += (y + i) * img->width * 4;
		
		cp += x * 4;		
		for (j = 0; j < ec->width; j++, bits++, cp+=4) {
			if (x < 0)
				continue;
			if (x >= img->width)
				continue;
			if ((v = *bits)) {
//				if (v == 0xff)
//					*d = col;
//				else {
				 {
					if (v == 255) {
						cp[0] = rgba & 0xff;
						cp[1] = (rgba >> 8) & 0xff;
						cp[2] = (rgba >> 16) & 0xff;
						cp[3] = 0xff; //(rgba >> 24) & 0xff;
					} else {
						double	oran = v / 255.0;	
						if (cp[0] == 0 && cp[1] == 0 && cp[2] == 0) {
							cp[0] = rgba & 0xff;
							cp[1] = (rgba >> 8) & 0xff;
							cp[2] = (rgba >> 16) & 0xff;
							cp[3] = oran *  ((rgba >> 24) & 0xff);
						} else {
							cp[0] = (1.0-oran) * cp[0] + oran * (rgba & 0xff);
							cp[1] = (1.0-oran) * cp[1] + oran * ((rgba >> 8) & 0xff);
							cp[2] = (1.0-oran) * cp[2] + oran * ((rgba >> 16) & 0xff);
						//	cp[3] = (1.0-oran) * cp[3] + oran * ((rgba >> 24) & 0xff);
						}
					/*	cp[0] = rgba & 0xff;
						cp[1] = (rgba >> 8) & 0xff;
						cp[2] = (rgba >> 16) & 0xff;
						cp[3] = oran *  ((rgba >> 24) & 0xff);
					*/
					}
					
				}
			};
			
		}
//		printf("\n");
	}
	*ax += ec->xoff; //+ec->bbox;
	*ay += ec->yoff;
	//_dirty = 1;
}

void png_wstr(gimp_image *img, int x, int y, char *s, DWORD col, ebm_font *f, int ispassword) {
	int i;
	int	st = 0;
	int	bx = x;
	if (!f)
		return;

	for (i = 0; *s; i++, s++) {
		if (*s == 0xc3 || *s == 0xc4 || *s == 0xc5)
			st = *s;
		else {
			if (st) {
				st = st << 8 | *s;
				switch (st) {
				case 0xc387: st = 0; break;
				case 0xc3a7: st = 1; break;
				case 0xc49e: st = 2; break;
				case 0xc49f: st = 3; break;
				case 0xc4b0: st = 4; break;
				case 0xc4b1: st = 5; break;
				case 0xc396: st = 6; break;
				case 0xc3b6: st = 7; break;
				case 0xc59e: st = 8; break;
				case 0xc59f: st = 9; break;
				case 0xc39c: st = 10; break;
				case 0xc3bc: st = 11; break;
				default:
					st = *s;
				}
				if (ispassword)
					st = '*';
				png_wchar (img, &x, &y, st, col, f);
				st = 0;
			} else if (*s == '\n') {
				x = bx;
				y += f->c[10].height * 1.5;
			} else {
				if (ispassword)
					png_wchar(img, &x, &y, '*', col, f);				
				else
					png_wchar(img, &x, &y, *s, col, f);				
			}
		}
	}
	//_dirty = 1;
}

gimp_image	*CreateImage(int w, int h) {
	gimp_image	*g = NULL;
//	BYTE		*p;
	
	g = calloc(sizeof(gimp_image) + 4 * w * h, 1);
	if (!g)
		goto l3;
	g->width = w;	
	g->height = h;
	g->rwidth = w;
	g->rheight = h;
	g->bytes_per_pixel = 4;
	g->dirty = 1;
l3:
	return g;
}


void	png_rect(gimp_image *g, int x, int y, int xx, int yy, int w, uint32_t col, int op) {
//	int	ix, iy; //, ixx; //, iyy;

	png_fill_rect(g, x, y, xx, y+w, col, op);
	png_fill_rect(g, x, yy-w, xx, yy, col, op);
	png_fill_rect(g, x, y+w+1, x+w, yy-w-1, col, op);
	png_fill_rect(g, xx-w, y+w+1, xx, yy-w-1, col, op);
}

void png_fill_rect (gimp_image *g, int x1, int y1, int x2, int y2, uint32_t col, int op) {
	uint32_t *p;
	int	j;
	
	/* Clipping and sanity checking */
	if (x1 > x2) { j = x1; x1 = x2; x2 = j; }
	if (y1 > y2) { j = y1; y1 = y2; y2 = j; }
	if (x1 < 0) x1 = 0; if ((__u32)x1 >= g->width) x1 = g->width - 1;
	if (x2 < 0) x2 = 0; if ((__u32)x2 >= g->width) x2 = g->width - 1;
	if (y1 < 0) y1 = 0; if ((__u32)y1 >= g->height) y1 = g->height - 1;
	if (y2 < 0) y2 = 0; if ((__u32)y2 >= g->height) y2 = g->height - 1;

	if ((x1 > x2) || (y1 > y2))
		return;
	int	ofs = g->width - x2 + x1 - 1;
	
	p = (uint32_t *) g->data;
	p += (y1 * g->width) + x1;		
	for (; y1 <= y2; y1++, p += ofs) {
		for (j = x1; j <= x2; j++)
			switch (op) {
				case 0:
					*(p++) = col; break;
				case 1: // XOR
					*(p++) ^= col; break;
			
				case 2: // OR
					*(p++) |= col; break;
			}
		
	}
}


void png_cp_rect(gimp_image *s, gimp_image *g, int x1, int y1, int x2, int y2, int dx, int dy, int op) {
	uint8_t *p, *ps, col;
	int	j, w, h;
	float	r;
	/* Clipping and sanity checking */
	if (x1 > x2) { j = x1; x1 = x2; x2 = j; }
	if (y1 > y2) { j = y1; y1 = y2; y2 = j; }
	if (x1 < 0) x1 = 0; if ((__u32)x1 >= g->width) x1 = g->width - 1;
	if (x2 < 0) x2 = 0; if ((__u32)x2 >= g->width) x2 = g->width - 1;
	if (y1 < 0) y1 = 0; if ((__u32)y1 >= g->height) y1 = g->height - 1;
	if (y2 < 0) y2 = 0; if ((__u32)y2 >= g->height) y2 = g->height - 1;

	if ((x1 > x2) || (y1 > y2))
		return;

	w = x2 - x1 + dx;
	h = y2 - y1 + dy;
	if ((w < 0) || (h < 0))
		return;
	if (w >= s->width) {
		w = s->width - 1;
		x2 = w - dx + x1;
	}
	if (h >= s->height) {
		h = s->height - 1;
		y2 = h - dy + y1;
	}
	int	ofs = g->width - x2 + x1 - 1;
	int	ofs2 = s->width - w + dx - 1;
	
	p = (uint8_t *) g->data;
	ps = (uint8_t *) s->data;
	p += 4 * ((y1 * g->width) + x1);
	ps += 4 * ((dy * s->width) + dx);		
	for (; y1 <= y2; y1++, p += ofs * 4, ps += 4 * ofs2) {
		uint8_t	r, g, b, t;
		for (j = x1; j <= x2; j++) {
			switch (op) {
				case 0:
					b = *(ps++);
					r = *(ps++);
					g = *(ps++);
					t = *(ps++);
					
					*p = ((*p * (255 - t)) + (b * t)) / 255; 
					p++;
					*p = ((*p * (255 - t)) + (r * t)) / 255; 
					p++;
					*p = ((*p * (255 - t)) + (g * t)) / 255; 
					p++;
										
					p++;
					break;
				case 1: // XOR
					*(p++) ^= *(ps++); 
					*(p++) ^= *(ps++); 
					*(p++) ^= *(ps++); 
					p++; ps++;
					break;
			
				case 2: // OR
					
					*(p++) |= *(ps++); 
					*(p++) |= *(ps++); 
					*(p++) |= *(ps++); 
					p++; ps++;
					break;
				case 3:
					*(p++) = *(ps++); 
					*(p++) = *(ps++); 
					*(p++) = *(ps++); 
					*(p++) = *(ps++); 
					
			}
		}
		
	}
}

void	png_put_png(gimp_image *g, gimp_image *s, int x, int y, int op) {
	png_cp_rect(s, g, x, y, x + s->width - 1, y + s->height - 1, 0, 0, op);
}