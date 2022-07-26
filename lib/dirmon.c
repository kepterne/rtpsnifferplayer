#define	dirmon_c

#include	"utils.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<locale.h>
#include	<unistd.h>
#include	<sys/epoll.h>
#include	<strings.h>
#include	<fcntl.h>
#include	<dirent.h>
#include	<sys/inotify.h>
#include	<limits.h>
#include	<string.h>

HandleEntry	*AddDirMon(HandleList *hl, char *directory, void *callback) {
	HandleEntry		*he;
	int			i, k, pollres;
	int			idx;
	LogPush("AddDirMon");

	he = GetHandle(hl);
	
	if (!he) {
		LogPrint("No handle!\n");
		LogPop();
		return NULL;
	}
	idx = he->__idx;
	bzero(((char *) he)+sizeof(llist_entry), sizeof(HandleEntry)-sizeof(llist_entry));
	he->__idx = idx;
	he->fhin = inotify_init();
	if (he->fhin <= 0) {
		LogPrint("No inotify fh!\n");
		LogPop();
		goto l1;
	}
	he->fhout = he->fhin;
	i = inotify_add_watch(he->fhin, directory, IN_ALL_EVENTS);
	if (i < 0) {
		goto l1;
	}
	he->rIP = i;
	strcpy(he->name, directory);
	fcntl(he->fhin,  F_SETFL, O_NONBLOCK);
	he->CallBack = callback;
	he->inuse = 1;
	he->Timeout = 0;
	he->EventTimeout = 0;
	he->handletype = RAW_HANDLE;
	
	if (he->CallBack) {
		int	r;
		if ((r = (*he->CallBack)(hl, he, EVENT_CREATE, NULL, 0, he->ExtraData))) {
			//CloseHandle(hl, he);
			goto l1;
		}
	}
	he->pf = NULL;
	k = he->le.ix;
	hl->polls[k].events = EPOLLIN;
	hl->polls[k].data.ptr = he;
	he->pf = hl->polls+k;
	he->pollfd = hl->pollfd;
	if ((pollres = epoll_ctl(he->pollfd, EPOLL_CTL_ADD, he->fhin, he->pf)) < 0) {
		perror("epoll_ctl");
		//CloseHandle(hl, he);
		goto l1;
	}
	LogPop();
	return he;
l1:
	;
	CloseHandle(hl, he);
	LogPrint("Failed\n");
	LogPop();
	return NULL;
}

