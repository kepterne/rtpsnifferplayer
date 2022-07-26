#ifndef	texteditor_h
#define	texteditor_h

typedef	struct text_editor_st {
	char		*text;
	dm_img	*iText;
	dm_img	*iCaret;
	int		maxlen;
	int		pos;
	int		focused;
	int		x, y, layer, alpha;
	DWORD		cFocus, cNormal, cDisabled, cHover;
	ebm_font	*fnt;
#define	TE_PASSWORD		0x01
#define	TE_IP			0x02
#define	TE_TEXT			0x03
#define	TE_DISABLED		0x04
#define	TE_FOCUSED		0x08
	DWORD	eType;
	struct text_editor_st *next, *prev;
	
	char	*placeholder;
} TextEditor;

TextEditor	*dm_TextEdit(
		TextEditor	*te
	);
void	CaretPos(char *text, ebm_font *fnt, int *n, int *x, int *xx);
void	UnselectEditor(TextEditor *te);
void	SelectEditor(TextEditor *te, int x);

#ifndef	texteditor_c
extern	TextEditor	*t_active;
#else
		TextEditor	*t_active = NULL;
#endif	
#endif