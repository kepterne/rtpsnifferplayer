#define	texteditor_c

#include	"utils.h"
#include	"dispmanx.h"
#include	"texteditor.h"

double	blink(double r, dm_img *img) {
	BYTE	b;
	DWORD	col;
	
	b = ((BYTE) (0xff * r)) ;
	/*if (b > 127) {
		b = ((b - 127) *2);
		col = 0xff000000 | (b << 8);
	} else 
		*/{
//		b *= 2;
		col = 0xff000000 | b  | (b << 8) | (b << 16) ;
	}
	dm_SetText(img, col);
	return r;
}

void	CaretPos(char *text, ebm_font *fnt, int *n, int *x, int *xx) {
	int		h, hh;
	char	ttext[64];
	int		l = strlen(text);
	
	strcpy(ttext, text);
//	strcat(ttext, " ");
//	emeasure_string(x, &h, &hh, ttext, fnt);


	if (l < *n) 
		*n = l;
	ttext[*n] = 0;
	emeasure_string(x, &h, &hh, ttext, fnt);
	//if (!ttext[*n]) 
	{
		ttext[*n] = ' ';
		ttext[*n+1] = 0;
		
		emeasure_string(xx, &h, &hh, ttext, fnt);
	}
}
void	UnselectEditor(TextEditor *te) {
	if (t_active != te)
		return;
	if (!t_active)
		return;
	te->focused = 0;
	if (te->placeholder) {
		if (!te->text[0] || (te->text[0] == ' ' && !te->text[1]))
			strcpy(te->text, te->placeholder);
	}
	dm_SetText(te->iText, te->cNormal);
	//dm_SetText(te->iCaret, te->cFocus);
	dm_DelAnims(te->iCaret);
	dm_HideImage(te->iCaret);	
	t_active = NULL;
}

void	SelectEditor(TextEditor *te, int x) {
	int	k, cx, cxx, lx = 0, isdone = 0;
//	if (te == t_active)
//		return;
	if (te->placeholder)
		if (!strcmp(te->text, te->placeholder))
			te->text[0] = 0;
	if (t_active && t_active != te)
		UnselectEditor(t_active);
	for (k = 0; k < te->maxlen && te->text[k]; k++) {
		int	z = k;
		CaretPos(te->text, te->fnt, &z, &cx, &cxx);
		
		if (cx >= x)
			if (z > 0) 
				if (te->pos != z) {
					te->pos = z-1;
					if (te->text[te->pos] > 0xc0)
						te->pos++;
					else if (te->pos) {
						if (te->text[te->pos-1] > 0xC0)
							te->pos--;
					}
					isdone = 1;
					dm_moveimage(te->x+lx-8, te->y, te->iCaret, te->iCaret->opacity);
					break;
				}
		lx = cx;
	}
	if (!isdone) {
		if (strlen(te->text)) {
			int	y, yy;
			te->pos = strlen(te->text);
			emeasure_string(&lx, &y, &yy, te->text, te->fnt);
			dm_moveimage(te->x+lx-8, te->y, te->iCaret, te->iCaret->opacity);
		}
	}
	te->focused = 1;
	dm_SetText(te->iText, te->cFocus);
	//dm_SetText(te->iCaret, te->cFocus);
pthread_mutex_lock(&__dm.anim_sem);		
	dm_DelAnims(te->iCaret);
	dm_AddAnim(te->iCaret, ANIM_TRANS | ANIM_LOOP | ANIM_SIN, 250000*4, blink, 255, 0, NULL, NULL);
pthread_mutex_unlock(&__dm.anim_sem);		
	dm_UnhideImage(te->iCaret);	
	t_active = te;
}

extern	int	__mx;

int	TextFocus(dm_img *img, int x, int y, int delta) {
	TextEditor	*te;
	if (img)
		te = (TextEditor *) img->data;
	else
		return 1;
	
	if (delta == -2) {
		int	xx = __mx - te->x;
		if (//!te->focused && 
		!(te->eType & TE_DISABLED)) {
			SelectEditor(te, xx);
		}
	}
	if (delta == 0) { // MOUSE ENTER
		if (!te->focused && !(te->eType & TE_DISABLED)) {
			if (img->col != te->cHover)
				dm_SetText(img, te->cHover);
		}
	} else if (delta == -10) { // MOUSE LEAVE
		if (!te->focused && !(te->eType & TE_DISABLED)) {
			if (img->col != te->cNormal)
				dm_SetText(img, te->cNormal);
		}
	}
	return 1;
}

char	__Caret[6] = "|";

TextEditor	*dm_TextEdit(
		TextEditor	*te
	) {
	DWORD		col;
	int			cx, cxx;
	if (!te)
		return NULL;
	if (te->eType & TE_DISABLED)
		col = te->cDisabled;
	else if (te->eType & TE_FOCUSED)
		col = te->cFocus;
	else
		col = te->cNormal;
		
	te->iText = dm_AddText(te->text, te->fnt, te->x, te->y, col, te->layer, te->alpha, te->eType & TE_PASSWORD);
	
	if (!te->iText)
		goto l0;
	te->pos = 0;
	CaretPos(te->text, te->fnt, &te->pos, &cx, &cxx);
	te->iCaret = dm_AddText(__Caret, te->fnt, te->x+cx-8, te->y, 0xcc000000, te->layer-1, 255, 0);
	if (!te->iCaret)
		goto l1;
	te->focused = te->eType & TE_FOCUSED;
	if (!te->focused) {
		dm_HideImage(te->iCaret);
		if (te->placeholder) {
			if (!te->text[0] || (te->text[0] == ' ' && !te->text[1])) {
				strcpy(te->text, te->placeholder);
			}
		}
	}
	te->iText->data = (char *) te;
	if (!te->focused)
		if (!(te->eType & TE_DISABLED))
			te->iText->focus = (focus_f) TextFocus;
	return te;
l1:
	dm_freeimage(te->iText);
	free(te);
l0:
	return NULL;
}
	
