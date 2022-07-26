/*
	Copyright (C) 2009 by Ekrem Karacan
	Ekrem Karacan@gmail.com
*/

#define	process_c

#include	<string.h>
#include	<stdlib.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<stdio.h>
#include	<sys/epoll.h>
#include	<fcntl.h>
#include	<signal.h>
#include	<unistd.h>
#include	<errno.h>

#include	"utils.h"
#include	"process.h"

int     _ProcessLineCB2(HandleList *hl, HandleEntry *he, int cmd, char *data, int len, char *ExtraData);
int     _ProcessLineCB3(HandleList *hl, HandleEntry *he, int cmd, char *data, int len, char *ExtraData);

HandleEntry	*AddProcess2(HandleList *hl, void *callback, void *ed) {
	HandleEntry	*he;
	int	 	c;
	int		socks[2]; 
	int     k, pollres;

	LogPush("AddProcess");
	he = GetHandle(hl);
	if (!he) {
		LogPrint("No handle\n");
		LogPop();
		return NULL;
	}
	bzero(((char *) he)+sizeof(llist_entry), sizeof(HandleEntry)-sizeof(llist_entry));
	he->ExtraData = ed;
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, socks) < 0) {
		CloseHandle(hl, he);
		LogPop();
		return NULL;
	}

	he->CallBack = callback;
	c = fork();
	if (c < 0) {
		int	err = errno;
		LogPrint("Err : %s\n", strerror(err));
		close(socks[0]);
		close(socks[1]);
		CloseHandle(hl, he);
		LogPop();
		return NULL;
	} else if (c) {
		he->fhin = socks[0];
		he->fhout = socks[0];
		he->rIP = c;
		he->pid = c;
		close(socks[1]);
		
		fcntl(he->fhin, F_SETFL, O_NONBLOCK);

		strcpy(he->name, "PROCESS");
		he->CallBack = callback;
		he->inuse = 1;
		he->Timeout = 0;
		he->EventTimeout = 0;
		he->handletype = PROCESS_HANDLE;

		k = he->le.ix;
		hl->polls[k].events = EPOLLIN | EPOLLHUP | EPOLLERR; 
		hl->polls[k].data.ptr = he;
		he->pf = hl->polls+k;
		he->pollfd = hl->pollfd;
		if ((pollres = epoll_ctl(he->pollfd, EPOLL_CTL_ADD, he->fhin, he->pf)) < 0) {
			perror("epoll_ctl");
			LogPrint("epoll_ctl add failed\n");
			CloseHandle(hl, he);
			LogPop();
			return NULL;
		}
		if (callback)
			(*he->CallBack)(hl, he, EVENT_CREATE, NULL, 0, NULL);
		LogPrint("Processcreated\n");
		LogPop();
		return he;
	} else if (c == 0) {	
//		signal(SIGINT, (sig_t) SIG_DFL);
		close(socks[0]);
		fcntl(socks[1], F_SETFL, O_NONBLOCK);
		if (dup2(socks[1], STDOUT_FILENO) != STDOUT_FILENO) {
			perror("dup2 error for standard output");
			exit(1);
		}
		if (dup2(socks[1], STDIN_FILENO) != STDIN_FILENO) {
			perror("dup2 error for standard input");
			exit(1);
		}
		if (dup2(socks[1], STDERR_FILENO) != STDERR_FILENO) {
			perror("dup2 error for standard input");
			exit(1);
		}
		if (he->CallBack) {
			int	r;
			//nice(20);
			r = (*he->CallBack)(hl, he, EVENT_EXECUTE, NULL, 0, he->ExtraData);
			//kill(0, SIGTERM);
			exit(r);
		}
		exit(0);
	}
	return NULL;
}

HandleEntry	*AddProcess(HandleList *hl, void *callback) {
	return AddProcess2(hl, callback, NULL);
}


HandleEntry	*AddLineProcess(HandleList *hl, void *callback, char *cmdline, char **params) {
	HandleEntry			*he = NULL;
	_processlinecontext	*lc;
	
	lc = calloc(sizeof(_processlinecontext), 1);
	if (!lc)
		goto l0;
	lc->cb = callback;
	lc->command = cmdline;
	lc->params = params;
	
	he = AddProcess2(hl, _ProcessLineCB2, lc);
	if (he)
		goto l0;
	free(lc);
l0:
	return he;
}
HandleEntry	*AddFixedDataProcess(HandleList *hl, void *callback, char *cmdline, char **params, int l) {
	HandleEntry			*he = NULL;
	_processlinecontext	*lc;
	
	lc = calloc(sizeof(_processlinecontext), 1);
	if (!lc)
		goto l0;
	lc->cb = callback;
	lc->max = l;
	lc->command = cmdline;
	lc->params = params;
	
	he = AddProcess2(hl, _ProcessLineCB3, lc);
	if (he)
		goto l0;
	free(lc);
	
l0:
	return he;
}


int     _ProcessLineCB2(HandleList *hl, HandleEntry *he, int cmd, char *data, int len, char *ExtraData) {
	int		r;
	//LogPrint("_ProcessLineCB %s %d\n", he->name, cmd);
	switch (cmd) {
		case EVENT_CONNECT: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			if (lc) 				
				return (*lc->cb)(hl, he, EVENT_CONNECT, data, len, ExtraData);
			else
				LogPrint("NO CALLBACK %s !!!\n", he->name);
		}
		break;
		case EVENT_FREE: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			if (lc) {
				if (lc->cb)
					(*lc->cb)(hl, he, EVENT_FREE, data, len, ExtraData);
				free(lc);
				he->ExtraData = NULL;
			}
		}
		break;
		case EVENT_DATA: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			data[len] = 0;
			//LogPrint("Data : [%s]\n", data);
			while (len > 0) {
				
				if (*data == '\r' || *data == '\n') {					
					lc->l[lc->p] = 0;
					//LogPrint("Data in < %d [%s]\n", lc->p, lc->l);
					if (lc->p)
						if (lc->cb) 
							if ((r = (*lc->cb)(hl, he, EVENT_DATA, lc->l, lc->p, ExtraData)))
								return r;
					lc->l[lc->p = 0] = 0;
				} else {
					lc->l[lc->p] = *data;
					if (lc->p < 1000)
						lc->p++;
				}
				data++;
				len--;
			}
		}
		break;
		case EVENT_EXECUTE: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			if (lc) {
//				char	**params;
				if (!lc->params) {
					char	*params[32];
					int		i = 0;
					
					char	*p;
					char	sparams[512];
					strcpy(sparams, lc->command);
					p = sparams;
					while (*p) {
						while (*p == 32)
							p++;
						if (!*p)
							break;
						params[i++] = p;
						while (*p && *p != 32)
							p++;
						if (*p)
							*(p++) = 0;
					}
					params[i] = NULL;
					int	j=0;
					for (j=0; j<i; j++)
						fprintf(stderr, "P[%d] = '%s'\n", j, params[j]);
					execvp(params[0], params);
				} else {
					int	i=0;
					//fprintf(stderr, "\t\t\tPARAMS\r\n");
					//for (i=0; lc->params[i]; i++)
					//	fprintf(stderr, "P[%d] = '%s'\n", i, lc->params[i]);
					execvp(lc->command, lc->params);
				}
				//return system(lc->command);
			}
		}
		break;
		case EVENT_WRITE: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			      
			WPDoWrite(hl, he);
				if (lc)
					if (lc->cb) 
						return (*lc->cb)(hl, he, EVENT_WRITE, NULL, len, ExtraData);
			
		}
		break;     
		default:
			LogPrint("EVENT (%d)\n", cmd);
	}
	return 0;  
}

int     _ProcessLineCB3(HandleList *hl, HandleEntry *he, int cmd, char *data, int len, char *ExtraData) {
	int		r;
	//LogPrint("_ProcessLineCB %s %d\n", he->name, cmd);
	switch (cmd) {
		case EVENT_CREATE: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			if (lc) 				
				return (*lc->cb)(hl, he, EVENT_CREATE, data, len, ExtraData);
			else
				LogPrint("NO CALLBACK %s !!!\n", he->name);
			break;
		}

		case EVENT_TIMEOUT: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			if (lc) 				
				return (*lc->cb)(hl, he, EVENT_TIMEOUT, data, len, ExtraData);
			else
				LogPrint("NO CALLBACK %s !!!\n", he->name);
			break;
		}
		case EVENT_CONNECT: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			if (lc) 				
				return (*lc->cb)(hl, he, EVENT_CONNECT, data, len, ExtraData);
			else
				LogPrint("NO CALLBACK %s !!!\n", he->name);
		}
		break;
		case EVENT_FREE: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			if (lc) {
				if (lc->cb)
					(*lc->cb)(hl, he, EVENT_FREE, data, len, ExtraData);
				free(lc);
				he->ExtraData = NULL;
			}
		}
		break;
		case EVENT_DATA: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
//			data[len] = 0;
			//LogPrint("Data : [%s]\n", data);
			while (len > 0) {
				lc->l[lc->p++] = *data;
					
				if (lc->p == lc->max) {					
					if (lc->cb) 
						if ((r = (*lc->cb)(hl, he, EVENT_DATA, lc->l, lc->p, ExtraData)))
							return r;
					lc->l[lc->p = 0] = 0;
				} 
				data++;
				len--;
			}
		}
		break;
		case EVENT_EXECUTE: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			if (lc) {
//				char	**params;
				if (!lc->params) {
					char	*params[32];
					int		i = 0;
					
					char	*p;
					char	sparams[512];
					strcpy(sparams, lc->command);
					p = sparams;
					while (*p) {
						while (*p == 32)
							p++;
						if (!*p)
							break;
						params[i++] = p;
						while (*p && *p != 32)
							p++;
						if (*p)
							*(p++) = 0;
					}
					params[i] = NULL;
					int	j=0;
//					for (j=0; j<i; j++)
//						fprintf(stderr, "P[%d] = '%s'\n", j, params[j]);
					execvp(params[0], params);
				} else
					execvp(lc->command, lc->params);
				//return system(lc->command);
			}
		}
		break;
		case EVENT_WRITE: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			         
			WPDoWrite(hl, he);
					      
			if (lc)
				if (lc->cb) 
					return (*lc->cb)(hl, he, EVENT_WRITE, NULL, len, ExtraData);
		}
		break;     
		default:
			LogPrint("EVENT (%d)\n", cmd);
	}
	return 0;  
}


int     _ProcessLineCallBack(HandleList *hl, HandleEntry *he, int cmd, char *data, int len, char *ExtraData) {
	int		r;
	//LogPrint("_ProcessLineCB %s %d\n", he->name, cmd);
	switch (cmd) {
		case EVENT_CONNECT: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			if (lc) 
				return (*lc->cb)(hl, he, EVENT_CONNECT, data, len, ExtraData);
			else
				LogPrint("NO CALLBACK %s !!!\n", he->name);
		}
		break;
		case EVENT_FREE: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			if (lc) {
				if (lc->cb)
					(*lc->cb)(hl, he, EVENT_FREE, data, len, ExtraData);
				free(lc);
				he->ExtraData = NULL;
			}
		}
		break;
		case EVENT_DATA: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			data[len] = 0;
			
			while (len > 0) {
				
				if (*data == '\r' || *data == '\n') {					
					lc->l[lc->p] = 0;
					if (lc->p)
						if (lc->cb)
							if ((r = (*lc->cb)(hl, he, EVENT_DATA, lc->l, lc->p, ExtraData)))
								return r;
					lc->l[lc->p = 0] = 0;
				} else {
					lc->l[lc->p] = *data;
					if (lc->p < 1020)
						lc->p++;
				}
				data++;
				len--;
			}
		}
		break;
		case EVENT_EXECUTE: {
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			if (lc) {
				char	*cmd = lc->command;
//				char	**params = lc->params;
			//	CloseHandles(hl);
				execvp(cmd, lc->params);
//				exit(0);
//				system(cmd);
//				exit(0);
			}
		}
		break;
		case EVENT_WRITE:     {     
			_processlinecontext	*lc = (_processlinecontext *) he->ExtraData;
			  
			WPDoWrite(hl, he);
			if (lc)
				if (lc->cb) 
					return (*lc->cb)(hl, he, EVENT_WRITE, NULL, len, ExtraData);
		
		}
		break;     
		default:
			LogPrint("EVENT (%d)\n", cmd);
	}
	return 0;  
}


HandleEntry	*AddNewProcess(HandleList *hl, void *callback, void *ed, void *ed2) {
	HandleEntry	*he;
	int	 	c;
	int		socks[2]; 
	int     k, pollres;

	LogPush("AddProcess");
	he = GetHandle(hl);
	if (!he) {
		LogPrint("No handle\n");
		LogPop();
		return NULL;
	}
	bzero(((char *) he)+sizeof(llist_entry), sizeof(HandleEntry)-sizeof(llist_entry));
	he->ExtraData = ed;
	he->ExtraData2 = ed2;
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, socks) < 0) {
		CloseHandle(hl, he);
		LogPop();
		return NULL;
	}

	he->CallBack = callback;
	c = fork();
	if (c < 0) {
		int	err = errno;
		LogPrint("Err : %s\n", strerror(err));
		close(socks[0]);
		close(socks[1]);
		CloseHandle(hl, he);
		LogPop();
		return NULL;
	} else if (c) {
		he->fhin = socks[0];
		he->fhout = socks[0];
		he->rIP = c;
		he->pid = c;
		close(socks[1]);
		
		he->handletype = PROCESS_HANDLE;
		fcntl(he->fhin, F_SETFL, O_NONBLOCK);

		strcpy(he->name, "PROCESS");
		he->CallBack = callback;
		he->inuse = 1;
		he->Timeout = 0;
		he->EventTimeout = 0;
		he->handletype = PROCESS_HANDLE;

		k = he->le.ix;
		hl->polls[k].events = EPOLLIN | EPOLLHUP | EPOLLERR; 
		hl->polls[k].data.ptr = he;
		he->pf = hl->polls+k;
		he->pollfd = hl->pollfd;
		if ((pollres = epoll_ctl(he->pollfd, EPOLL_CTL_ADD, he->fhin, he->pf)) < 0) {
			perror("epoll_ctl");
			LogPrint("epoll_ctl add failed\n");
			CloseHandle(hl, he);
			LogPop();
			return NULL;
		}
		if (callback)
			(*he->CallBack)(hl, he, EVENT_CREATE, NULL, 0, NULL);
		LogPrint("Processcreated\n");
		LogPop();
		return he;
	} else if (c == 0) {	
//		signal(SIGINT, (sig_t) SIG_DFL);
		close(socks[0]);
		fcntl(socks[1], F_SETFL, O_NONBLOCK);
		if (dup2(socks[1], STDOUT_FILENO) != STDOUT_FILENO) {
			perror("dup2 error for standard output");
			exit(1);
		}
		if (dup2(socks[1], STDIN_FILENO) != STDIN_FILENO) {
			perror("dup2 error for standard input");
			exit(1);
		}
		if (dup2(socks[1], STDERR_FILENO) != STDERR_FILENO) {
			perror("dup2 error for standard input");
			exit(1);
		}
		if (he->CallBack) {
			int	r;
			r = (*he->CallBack)(hl, he, EVENT_EXECUTE, NULL, 0, he->ExtraData);
			exit(r);
		}
		exit(0);
	}
	return NULL;
}


HandleEntry	*AddNewLineProcess(HandleList *hl, void *callback, char *cmdline, char **params, char *ed2) {
	HandleEntry			*he = NULL;
	_processlinecontext	*lc;
	
	lc = calloc(sizeof(_processlinecontext), 1);
	if (!lc)
		goto l0;
	lc->cb = callback;
	lc->command = cmdline;
	lc->params = params;
	
	he = AddNewProcess(hl, _ProcessLineCallBack, lc, ed2);
	if (he)
		goto l0;
	free(lc);
l0:
	return he;
}
