/*
(C) 2009+ Ekrem Karacan
*/
#ifndef	TCPSERVER_H
#define	TCPSERVER_H

#include	"utils.h"

HandleEntry	*AddTCPServer(HandleList *hl, int port, void *callback);
HandleEntry	*AddTCPServerToIP(HandleList *hl, char *addr, int port, void *callback);
HandleEntry	*AcceptTCPClient(HandleList *hl, HandleEntry *hep, char *name);
HandleEntry	*AddTCPClient(HandleList *hl, char *remotehost, WORD remoteport, char *name, void *callback);
HandleEntry	*AddTCPClient2(HandleList *hl, char *remotehost, WORD remoteport,
	char *name, void *callback, void *ed);
HandleEntry	*AddTCPClientToHost(HandleList *hl, char *remotehost, WORD remoteport, char *name, void *callback);
HandleEntry	*AddUDPServer(HandleList *hl, int port, void *callback);
HandleEntry	*AddLineTCPClient(HandleList *hl, char *remotehost, WORD remoteport, char *name, void *callback);
HandleEntry	*AddUDPBCServer(HandleList *hl, int port, void *callback);
HandleEntry	*AddUDPBCServerToA(HandleList *hl, int port, void *callback, char *addr);
int     GetUDPBcast(int port);
int     GetAUDPBcast(int port, struct  sockaddr_in *LocalAddr);
HandleEntry	*AddUDPClient(HandleList *hl, char *remotehost, int port, char *name, void *callback);
HandleEntry	*AddUDPClient_tomem(HandleList *hl, char *remotehost, int port, char *name, void *callback);
HandleEntry	*DNSQuery(HandleList *hl, char *name, void *callback);
int     cbDNS(HandleList *hl, HandleEntry *he, int cmd, char *data, int len, char *ExtraData);

HandleEntry	*AddLineTCPClientS(HandleList *hl, char *remotehost, WORD remoteport, char *name, void *callback);
int     GetUDPBcastToA(int port, char *addr);
int     GetUDPToA(int port, char *addr);

#endif
