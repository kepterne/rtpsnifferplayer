#include	<stdio.h>
#include	<sys/stat.h>
#include	<stdlib.h>
#include	<string.h>

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


extern	int	gScreen;

ebm_font	*LoadEBMFont(char *fname) {
	ebm_font		*f = NULL;
	struct stat		st;
	int				size;
	FILE			*fp = NULL;
	
	char	fn[128];
	char	*p;
//	int				i;
	if (stat(fname, &st))
		goto l0;
	size = st.st_size;
	if (!(f = calloc(size, 1)))
		goto l0;
	strcpy(fn, fname);
	p = strstr(fn, ".ebf");
	if (p) {
		sprintf(p, ".%d.ebf", gScreen);		
		fp = fopen(fn, "r");
	}
	if (!fp)
		if (!(fp = fopen(fname, "r")))
			goto l1;
	fread(f, size, 1, fp);
	fclose(fp);
	
//	for (i=0; i<256; i++)
//		f->c[i].pos += sizeof(ebm_font_char)*256;
	goto l0;
l1:
	free(f);
l0:
	return f;
}