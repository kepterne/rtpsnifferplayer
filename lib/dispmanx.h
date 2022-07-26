#ifndef	dispmanx_h
#define	dispmanx_h

#include	"utils.h"
#include	"fbutils.h"
#include	"bcm_host.h"

typedef	struct {
	llist_hdr					lh;
	llist_hdr					anim_lh;
	int							screen;
	int							w, h;
	DISPMANX_DISPLAY_HANDLE_T   display;
    DISPMANX_MODEINFO_T         info;
    void                       	*image;
    DISPMANX_UPDATE_HANDLE_T    update;
    DISPMANX_RESOURCE_HANDLE_T  resource;
	DISPMANX_RESOURCE_HANDLE_T  bgr;
	uint32_t 					bgh;
	DISPMANX_ELEMENT_HANDLE_T   bge;
    VC_DISPMANX_ALPHA_T 		bga;
	DISPMANX_RESOURCE_HANDLE_T  alpha;
    DISPMANX_ELEMENT_HANDLE_T   element;
	uint32_t                    vc_image_ptr;
	DWORD						bkg;
	int							dirty;
//	int							inanim;
	pthread_mutex_t				anim_sem;
} dispmanx;

typedef	struct	dm_img_st dm_img;
typedef		int	(*focus_f)(struct dm_img_st *, int, int, DWORD) ;

typedef	struct dm_img_st {
	llist_entry					le;
	DISPMANX_RESOURCE_HANDLE_T  resource;
    DISPMANX_ELEMENT_HANDLE_T   element;
	uint32_t					img;
	VC_RECT_T       			dr, sr;   
	char						*text;
	char						oldvalue[256];
	int							x, y;
	int	(*f)(DWORD delta, struct dm_img_st *img);
	gimp_image					*g;
	ebm_font					*fnt;
	int							toff;
	int							layer;
	DWORD						col;
	double						ratio;
	int							opacity;
	int							visible;
	char						*data;
	int							twidth, theight;
	struct dm_anim_st			*amousein, *amouseout;
	focus_f						focus;
	int							ispassword;
	void						*extradata;
	int							tag;
	int							alwaysvisible;
	struct	dm_img_st			*pa, *ne, *pr;
	struct	dm_img_st			*fi, *la;
} dm_img;


typedef	struct dm_anim_st {
	llist_entry		le;
	int				active;
	int				delete;
	dm_img			*img;
	DWORD			duration, pos;
	double			r;
	WORD			kind;
	int				min, max, cur;
	double			(*f)(double, dm_img *);	
	char			*data;
	int				(*complete)(dm_img *i, struct dm_anim_st *, char *data);
#define		ANIM_LOOP	0x0001
#define		ANIM_X		0x0002
#define		ANIM_Y		0x0004
#define		ANIM_XW		0x0008
#define		ANIM_YW		0x0020
#define		ANIM_SIN	0x0040
#define		ANIM_SQR	0x0080
#define		ANIM_TRANS	0x0010
} dm_anim;

dm_anim	*dm_AddAnim(dm_img *img, WORD kind, int duration, double (*f)(double, dm_img *), int max, int min,
	void *complete, 
	char *data);
int	dm_DelAnims(dm_img *img);	
//int	dm_DelAnim(dm_anim *a);	
int	dm_DelAnimsbyKind(dm_img *img, WORD kind);

void	dm_Animate(DWORD delta);
	
dm_img	*dm_AddText(char *text, ebm_font *f, int x, int y, DWORD col, int layer, int opacity, int ispassword);
dm_img	*dm_SetText(dm_img *img, DWORD col);
void	CheckTexts(void);

dispmanx	__dm;

dm_img	*dm_CreatePNG(char *fname);
dm_img	*dm_createimage(gimp_image *g);
void	dm_showimage(int layer, int opacity, int x, int y, dm_img *img);
void	dm_moveimage(int x, int y, dm_img *di, int opacity);
void	InitDispmanX(int screen);
void	CloseDispmanX(void);
void	dm_setbackground(DWORD d);
void	Invalidate(void);
void	Validate(void);
void	SetAllTransparent(int v);
int		dm_freeimage(dm_img *di);
void	dm_HideImage(dm_img *img);
void	dm_UnhideImage(dm_img *img);

void	dm_attach(dm_img *p, dm_img *c);
void	dm_detach(dm_img *c);


#ifdef	dispmanx_c
	dm_img		*gLogo = NULL;
	gimp_image	*iLogo = NULL;
	int			__mx = 1920/2, __my=1080/2;
	gimp_image	*iCursor = NULL;
	dm_img		*gCursor = NULL;

#else
	extern	dm_img		*gLogo;
	extern	gimp_image	*iLogo;
	extern	int			__mx, __my;
	extern	gimp_image	*iCursor;
	extern	dm_img		*gCursor;
#endif
void	ShowAll(void);
void	DumpImgs(void);
gimp_image	*dm_LoadPNG(char *fname);
dm_img	*dm_refreshimage(dm_img *di, gimp_image *g);

void	Lock(void); 
void	Unlock(void);

#endif