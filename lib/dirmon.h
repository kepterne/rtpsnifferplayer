#ifndef	dirmon_h
#define	dirmon_h

#include        <dirent.h>
#include        <sys/inotify.h>

HandleEntry	*AddDirMon(HandleList *hl, char *directory, void *callback);

#endif