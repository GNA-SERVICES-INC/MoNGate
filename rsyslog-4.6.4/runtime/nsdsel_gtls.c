/* nsdsel_gtls.c
 *
 * An implementation of the nsd select() interface for GnuTLS.
 * 
 * Copyright (C) 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */
#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <gnutls/gnutls.h>

#include "rsyslog.h"
#include "module-template.h"
#include "obj.h"
#include "errmsg.h"
#include "nsd.h"
#include "nsd_gtls.h"
#include "nsd_ptcp.h"
#include "nsdsel_ptcp.h"
#include "nsdsel_gtls.h"

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(glbl)
DEFobjCurrIf(nsdsel_ptcp)


/* Standard-Constructor
 */
BEGINobjConstruct(nsdsel_gtls) /* be sure to specify the object type also in END macro! */
	iRet = nsdsel_ptcp.Construct(&pThis->pTcp);
ENDobjConstruct(nsdsel_gtls)


/* destructor for the nsdsel_gtls object */
BEGINobjDestruct(nsdsel_gtls) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(nsdsel_gtls)
	if(pThis->pTcp != NULL)
		nsdsel_ptcp.Destruct(&pThis->pTcp);
ENDobjDestruct(nsdsel_gtls)


/* Add a socket to the select set */
static rsRetVal
Add(nsdsel_t *pNsdsel, nsd_t *pNsd, nsdsel_waitOp_t waitOp)
{
	DEFiRet;
	nsdsel_gtls_t *pThis = (nsdsel_gtls_t*) pNsdsel;
	nsd_gtls_t *pNsdGTLS = (nsd_gtls_t*) pNsd;

	ISOBJ_TYPE_assert(pThis, nsdsel_gtls);
	ISOBJ_TYPE_assert(pNsdGTLS, nsd_gtls);
	if(pNsdGTLS->iMode == 1) {
		if(waitOp == NSDSEL_RD && gtlsHasRcvInBuffer(pNsdGTLS)) {
			++pThis->iBufferRcvReady;
			FINALIZE;
		}
		if(pNsdGTLS->rtryCall != gtlsRtry_None) {
			if(gnutls_record_get_direction(pNsdGTLS->sess) == 0) {
				CHKiRet(nsdsel_ptcp.Add(pThis->pTcp, pNsdGTLS->pTcp, NSDSEL_RD));
			} else {
				CHKiRet(nsdsel_ptcp.Add(pThis->pTcp, pNsdGTLS->pTcp, NSDSEL_WR));
			}
			FINALIZE;
		}
	}

	/* if we reach this point, we need no special handling */
	CHKiRet(nsdsel_ptcp.Add(pThis->pTcp, pNsdGTLS->pTcp, waitOp));

finalize_it:
	RETiRet;
}


/* perform the select()  piNumReady returns how many descriptors are ready for IO 
 * TODO: add timeout!
 */
static rsRetVal
Select(nsdsel_t *pNsdsel, int *piNumReady)
{
	DEFiRet;
	nsdsel_gtls_t *pThis = (nsdsel_gtls_t*) pNsdsel;

	ISOBJ_TYPE_assert(pThis, nsdsel_gtls);
	if(pThis->iBufferRcvReady > 0) {
		/* we still have data ready! */
		*piNumReady = pThis->iBufferRcvReady;
	} else {
		iRet = nsdsel_ptcp.Select(pThis->pTcp, piNumReady);
	}

	RETiRet;
}


/* retry an interrupted GTLS operation
 * rgerhards, 2008-04-30
 */
static rsRetVal
doRetry(nsd_gtls_t *pNsd)
{
	DEFiRet;
	int gnuRet;

	dbgprintf("GnuTLS requested retry of %d operation - executing\n", pNsd->rtryCall);

	/* We follow a common scheme here: first, we do the systen call and
	 * then we check the result. So far, the result is checked after the
	 * switch, because the result check is the same for all calls. Note that
	 * this may change once we deal with the read and write calls (but
	 * probably this becomes an issue only when we begin to work on TLS
	 * for relp). -- rgerhards, 2008-04-30
	 */
	switch(pNsd->rtryCall) {
		case gtlsRtry_handshake:
			gnuRet = gnutls_handshake(pNsd->sess);
			if(gnuRet == 0) {
				pNsd->rtryCall = gtlsRtry_None; /* we are done */
				/* we got a handshake, now check authorization */
				CHKiRet(gtlsChkPeerAuth(pNsd));
			}
			break;
		case gtlsRtry_recv:
			dbgprintf("retrying gtls recv, nsd: %p\n", pNsd);
			CHKiRet(gtlsRecordRecv(pNsd));
			pNsd->rtryCall = gtlsRtry_None; /* we are done */
			gnuRet = 0;
			break;
		default:
			assert(0); /* this shall not happen! */
			dbgprintf("ERROR: pNsd->rtryCall invalid in nsdsel_gtls.c:%d\n", __LINE__);
			gnuRet = 0; /* if it happens, we have at least a defined behaviour... ;) */
			break;
	}

	if(gnuRet == 0) {
		pNsd->rtryCall = gtlsRtry_None; /* we are done */
	} else if(gnuRet != GNUTLS_E_AGAIN && gnuRet != GNUTLS_E_INTERRUPTED) {
		uchar *pErr = gtlsStrerror(gnuRet);
		dbgprintf("unexpected GnuTLS error %d in %s:%d: %s\n", gnuRet, __FILE__, __LINE__, pErr);
		free(pErr);
		pNsd->rtryCall = gtlsRtry_None; /* we are also done... ;) */
		ABORT_FINALIZE(RS_RET_GNUTLS_ERR);
	}
	/* if we are interrupted once again (else case), we do not need to
	 * change our status because we are already setup for retries.
	 */
		
finalize_it:
	if(iRet != RS_RET_OK && iRet != RS_RET_CLOSED && iRet != RS_RET_RETRY)
		pNsd->bAbortConn = 1; /* request abort */
	RETiRet;
}


/* check if a socket is ready for IO */
static rsRetVal
IsReady(nsdsel_t *pNsdsel, nsd_t *pNsd, nsdsel_waitOp_t waitOp, int *pbIsReady)
{
	DEFiRet;
	nsdsel_gtls_t *pThis = (nsdsel_gtls_t*) pNsdsel;
	nsd_gtls_t *pNsdGTLS = (nsd_gtls_t*) pNsd;

	ISOBJ_TYPE_assert(pThis, nsdsel_gtls);
	ISOBJ_TYPE_assert(pNsdGTLS, nsd_gtls);
	if(pNsdGTLS->iMode == 1) {
		if(waitOp == NSDSEL_RD && gtlsHasRcvInBuffer(pNsdGTLS)) {
			*pbIsReady = 1;
			FINALIZE;
		}
		if(pNsdGTLS->rtryCall != gtlsRtry_None) {
			CHKiRet(doRetry(pNsdGTLS));
			/* we used this up for our own internal processing, so the socket
			 * is not ready from the upper layer point of view.
			 */
			*pbIsReady = 0;
			FINALIZE;
		}
	}

	CHKiRet(nsdsel_ptcp.IsReady(pThis->pTcp, pNsdGTLS->pTcp, waitOp, pbIsReady));

finalize_it:
	RETiRet;
}


/* ------------------------------ end support for the select() interface ------------------------------ */


/* queryInterface function */
BEGINobjQueryInterface(nsdsel_gtls)
CODESTARTobjQueryInterface(nsdsel_gtls)
	if(pIf->ifVersion != nsdCURR_IF_VERSION) {/* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = (rsRetVal(*)(nsdsel_t**)) nsdsel_gtlsConstruct;
	pIf->Destruct = (rsRetVal(*)(nsdsel_t**)) nsdsel_gtlsDestruct;
	pIf->Add = Add;
	pIf->Select = Select;
	pIf->IsReady = IsReady;
finalize_it:
ENDobjQueryInterface(nsdsel_gtls)


/* exit our class
 */
BEGINObjClassExit(nsdsel_gtls, OBJ_IS_CORE_MODULE) /* CHANGE class also in END MACRO! */
CODESTARTObjClassExit(nsdsel_gtls)
	/* release objects we no longer need */
	objRelease(glbl, CORE_COMPONENT);
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(nsdsel_ptcp, LM_NSD_PTCP_FILENAME);
ENDObjClassExit(nsdsel_gtls)


/* Initialize the nsdsel_gtls class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(nsdsel_gtls, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(nsdsel_ptcp, LM_NSD_PTCP_FILENAME));

	/* set our own handlers */
ENDObjClassInit(nsdsel_gtls)
/* vi:set ai:
 */
