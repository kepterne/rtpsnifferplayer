#ifndef	process_h
#define	process_h

int     _ProcessLineCB(HandleList *hl, HandleEntry *he, int cmd, char *data, int len, char *ExtraData);
HandleEntry	*AddLineProcess(HandleList *hl, void *callback, char *cmdline, char **params);
HandleEntry	*AddNewLineProcess(HandleList *hl, void *callback, char *cmdline, char **params, char *ed2);
HandleEntry	*AddFixedDataProcess(HandleList *hl, void *callback, char *cmdline, char **params, int l);

#endif