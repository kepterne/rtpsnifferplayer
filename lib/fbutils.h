#ifndef	fbutils_h
#define	fbutils_h

#include	"utils.h"
#include	<asm/types.h>
#include	<linux/types.h>
//v3 #include	<semaphore.h>
#include	<pthread.h>

#define RGB16(r,g,b) (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
#define	TS_POINTERCAL "/etc/pointercal"

typedef struct {
	int x[5], xfb[5];
	int y[5], yfb[5];
	int a[7];
} calibration;

typedef struct {
	DWORD	width;
	DWORD	height;
	DWORD	rwidth;
	DWORD	rheight;
	int		dirty;
	DWORD	bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */ 
	WORD	data[10];
	
} gimp_image;

typedef	struct {
	int				code;
	unsigned char	left;
	unsigned char	top;
	unsigned char	width;
	unsigned char	height;
	unsigned char	xoff;
	unsigned char	yoff;
	int				pos;
} ebm_font_char;

typedef	struct {
	ebm_font_char	c[256];
} ebm_font;

typedef	struct {
	int	x, y, xx, yy;
} ebounds;

int	_lock;
int	switchfb(void);
int open_framebuffer(gimp_image *img);
void close_framebuffer(void);
gimp_image	*LoadImage(char *fname);
gimp_image	*LoadPNG(char *fname);
ebm_font	*LoadEBMFont(char *fname);
void eput_stringb(int x, int y, char *s, unsigned short col, ebm_font *f, ebounds *b);
void eput_string_centerb(int x, int y, char *s, unsigned short col, ebm_font *f, ebounds *b);
void eput_string(int x, int y, char *s, unsigned short col, ebm_font *f);
void eput_string_center(int x, int y, char *s, unsigned short col, ebm_font *f);
void	emeasure_string(int *x, int *y, int *yy, char *s, ebm_font *f);
void	blt(int x, int y, int w, int h, int ox, int oy, gimp_image *img);
void efill_rect (int x1, int y1, int x2, int y2, unsigned short col, int op);
void	erect(int x, int y, int xx, int yy, int w, unsigned short col, int op);
void	blttrans(int x, int y, int w, int h, int ox, int oy, gimp_image *img);

int	StartTouchScreen(HandleList *hl);
int	StopTouchScreen(HandleList *hl);
extern	int		ts_thread_running;

#ifdef	fbutils_c
	int	_screen_saver = 0;
	int	_slock = 0;
	int 				fb_fd=0;
#else
	extern	int	_slock;
	extern	int	_screen_saver;
	extern int 				fb_fd;
#endif

ebm_font /*	*Futura40, 
			*Futura32,
			*Futura20,
			*Futura22,
			*Futura16,
			*LCD64,
			*LCD40,
			*LCD32,
			*LCD20,
			*LCD16,
			*Menlo40,
			*Menlo32,
			*Menlo20,
			*Menlo16,
			*Cambria40,
			*Cambria32,
			*Cambria20,
			*Cambria16,
		*/	*MainFont16L,
			*MainFont22L,
			*MainFont24L,
			*MainFont32L,
			*MainFont40L,
			*MainFont64L,
			*MainFont16UL,
			*MainFont24UL,
			*MainFont32UL,
			*MainFont40UL,
			*MainFont64UL,
			*MainFont16M,
			*MainFont24M,
			*MainFont32M,
			*MainFont40M,
			*MainFont64M,
			*Futura40,
			*Futura32,
			*Futura24,
			*Futura16
			;

DWORD 					xres, yres;

int	fbThread_stop(void);
int	fbThread_start(void);
//3v	sem_t	fbsem, invsem;
pthread_mutex_t	fbsem, invsem;

void	SwitchFB(int i);
void png_wchar(gimp_image *img, int *ax, int *ay, unsigned char c, DWORD rgba, ebm_font *f);
void png_wstr(gimp_image *img, int x, int y, char *s, DWORD col, ebm_font *f, int ispassword);
gimp_image	*CreateImage(int w, int h);

void	png_rect(gimp_image *g, int x, int y, int xx, int yy, int w, uint32_t col, int op);
void	png_fill_rect (gimp_image *g, int x1, int y1, int x2, int y2, uint32_t col, int op);
void	png_put_png(gimp_image *g, gimp_image *s, int x, int y, int op);
void png_cp_rect(gimp_image *s, gimp_image *g, int x1, int y1, int x2, int y2, int dx, int dy, int op);
#endif