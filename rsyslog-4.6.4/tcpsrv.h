/* Definitions for tcpsrv class.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#ifndef INCLUDED_TCPSRV_H
#define INCLUDED_TCPSRV_H

#include "obj.h"
#include "prop.h"
#include "tcps_sess.h"

/* support for framing anomalies */
typedef enum ETCPsyslogFramingAnomaly {
	frame_normal = 0,
	frame_NetScreen = 1,
	frame_CiscoIOS = 2
} eTCPsyslogFramingAnomaly;


/* list of tcp listen ports */
struct tcpLstnPortList_s {
	uchar *pszPort;			/**< the ports the listener shall listen on */
	prop_t *pInputName;
	tcpsrv_t *pSrv;			/**< pointer to higher-level server instance */
	ruleset_t *pRuleset;		/**< associated ruleset */
	tcpLstnPortList_t *pNext;	/**< next port or NULL */
};

#define TCPSRV_NO_ADDTL_DELIMITER -1 /* specifies that no additional delimiter is to be used in TCP framing */

/* the tcpsrv object */
struct tcpsrv_s {
	BEGINobjInstance;	/**< Data to implement generic object - MUST be the first data element! */
	netstrms_t *pNS;	/**< pointer to network stream subsystem */
	int iDrvrMode;		/**< mode of the stream driver to use */
	uchar *pszDrvrAuthMode;	/**< auth mode of the stream driver to use */
	uchar *pszInputName;	/**< value to be used as input name */
	ruleset_t *pRuleset;	/**< ruleset to bind to */
	permittedPeers_t *pPermPeers;/**< driver's permitted peers */
	bool bEmitMsgOnClose;	/**< emit an informational message when the remote peer closes connection */
	int iLstnCurr;		/**< max nbr of listeners currently supported */
	netstrm_t **ppLstn;	/**< our netstream listners */
	tcpLstnPortList_t **ppLstnPort; /**< pointer to relevant listen port description */
	int iLstnMax;		/**< max number of listners supported */
	int iSessMax;		/**< max number of sessions supported */
	tcpLstnPortList_t *pLstnPorts;	/**< head pointer for listen ports */

	int addtlFrameDelim;	/**< additional frame delimiter for plain TCP syslog framing (e.g. to handle NetScreen) */
	tcps_sess_t **pSessions;/**< array of all of our sessions */
	void *pUsr;		/**< a user-settable pointer (provides extensibility for "derived classes")*/
	/* callbacks */
	int      (*pIsPermittedHost)(struct sockaddr *addr, char *fromHostFQDN, void*pUsrSrv, void*pUsrSess);
	rsRetVal (*pRcvData)(tcps_sess_t*, char*, size_t, ssize_t *);
	rsRetVal (*OpenLstnSocks)(struct tcpsrv_s*);
	rsRetVal (*pOnListenDeinit)(void*);
	rsRetVal (*OnDestruct)(void*);
	rsRetVal (*pOnRegularClose)(tcps_sess_t *pSess);
	rsRetVal (*pOnErrClose)(tcps_sess_t *pSess);
	/* session specific callbacks */
	rsRetVal (*pOnSessAccept)(tcpsrv_t *, tcps_sess_t*);
	rsRetVal (*OnSessConstructFinalize)(void*);
	rsRetVal (*pOnSessDestruct)(void*);
	rsRetVal (*OnMsgReceive)(tcps_sess_t *, uchar *pszMsg, int iLenMsg); /* submit message callback */
};


/* interfaces */
BEGINinterface(tcpsrv) /* name must also be changed in ENDinterface macro! */
	INTERFACEObjDebugPrint(tcpsrv);
	rsRetVal (*Construct)(tcpsrv_t **ppThis);
	rsRetVal (*ConstructFinalize)(tcpsrv_t __attribute__((unused)) *pThis);
	rsRetVal (*Destruct)(tcpsrv_t **ppThis);
	rsRetVal (*configureTCPListen)(tcpsrv_t*, uchar *pszPort);
	//rsRetVal (*SessAccept)(tcpsrv_t *pThis, tcpLstnPortList_t*, tcps_sess_t **ppSess, netstrm_t *pStrm);
	rsRetVal (*create_tcp_socket)(tcpsrv_t *pThis);
	rsRetVal (*Run)(tcpsrv_t *pThis);
	/* set methods */
	rsRetVal (*SetAddtlFrameDelim)(tcpsrv_t*, int);
	rsRetVal (*SetInputName)(tcpsrv_t*, uchar*);
	rsRetVal (*SetUsrP)(tcpsrv_t*, void*);
	rsRetVal (*SetCBIsPermittedHost)(tcpsrv_t*, int (*) (struct sockaddr *addr, char*, void*, void*));
	rsRetVal (*SetCBOpenLstnSocks)(tcpsrv_t *, rsRetVal (*)(tcpsrv_t*));
	rsRetVal (*SetCBRcvData)(tcpsrv_t *pThis, rsRetVal (*pRcvData)(tcps_sess_t*, char*, size_t, ssize_t*));
	rsRetVal (*SetCBOnListenDeinit)(tcpsrv_t*, rsRetVal (*)(void*));
	rsRetVal (*SetCBOnDestruct)(tcpsrv_t*, rsRetVal (*) (void*));
	rsRetVal (*SetCBOnRegularClose)(tcpsrv_t*, rsRetVal (*) (tcps_sess_t*));
	rsRetVal (*SetCBOnErrClose)(tcpsrv_t*, rsRetVal (*) (tcps_sess_t*));
	rsRetVal (*SetDrvrMode)(tcpsrv_t *pThis, int iMode);
	rsRetVal (*SetDrvrAuthMode)(tcpsrv_t *pThis, uchar *pszMode);
	rsRetVal (*SetDrvrPermPeers)(tcpsrv_t *pThis, permittedPeers_t*);
	/* session specifics */
	rsRetVal (*SetCBOnSessAccept)(tcpsrv_t*, rsRetVal (*) (tcpsrv_t*, tcps_sess_t*));
	rsRetVal (*SetCBOnSessDestruct)(tcpsrv_t*, rsRetVal (*) (void*));
	rsRetVal (*SetCBOnSessConstructFinalize)(tcpsrv_t*, rsRetVal (*) (void*));
	/* added v5 */
	rsRetVal (*SetSessMax)(tcpsrv_t *pThis, int iMaxSess);	/* 2009-04-09 */
	/* added v6 */
	rsRetVal (*SetOnMsgReceive)(tcpsrv_t *pThis, rsRetVal (*OnMsgReceive)(tcps_sess_t*, uchar*, int)); /* 2009-05-24 */
	rsRetVal (*SetRuleset)(tcpsrv_t *pThis, ruleset_t*); /* 2009-06-12 */
	/* added v7 */
	rsRetVal (*SetLstnMax)(tcpsrv_t *pThis, int iMaxLstn);	/* 2009-08-17 */
	rsRetVal (*SetNotificationOnRemoteClose)(tcpsrv_t *pThis, int bNewVal); /* 2009-10-01 */
ENDinterface(tcpsrv)
#define tcpsrvCURR_IF_VERSION 8 /* increment whenever you change the interface structure! */
/* change for v4:
 * - SetAddtlFrameDelim() added -- rgerhards, 2008-12-10
 * - SetInputName() added -- rgerhards, 2008-12-10
 * change for v5 and up: see above
 */


/* prototypes */
PROTOTYPEObj(tcpsrv);

/* the name of our library binary */
#define LM_TCPSRV_FILENAME "lmtcpsrv"

#endif /* #ifndef INCLUDED_TCPSRV_H */
