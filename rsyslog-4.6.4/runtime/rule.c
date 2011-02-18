/* rule.c - rsyslog's rule object
 *
 * See file comment in rule.c for the overall structure of rule processing.
 *
 * Module begun 2009-06-10 by Rainer Gerhards
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "rsyslog.h"
#include "obj.h"
#include "action.h"
#include "rule.h"
#include "errmsg.h"
#include "vm.h"
#include "var.h"
#include "srUtils.h"
#include "unicode-helper.h"
#include "dirty.h" /* for getFIOPName */

/* static data */
DEFobjStaticHelpers
DEFobjCurrIf(errmsg)
DEFobjCurrIf(expr)
DEFobjCurrIf(var)
DEFobjCurrIf(vm)

/* iterate over all actions, this is often needed, for example when HUP processing 
 * must be done or a shutdown is pending.
 */
static rsRetVal
iterateAllActions(rule_t *pThis, rsRetVal (*pFunc)(void*, void*), void* pParam)
{
	return llExecFunc(&pThis->llActList, pFunc, pParam);
}



/* helper to processMsg(), used to call the configured actions. It is
 * executed from within llExecFunc() of the action list.
 * rgerhards, 2007-08-02
 */
typedef struct processMsgDoActions_s {
	int bPrevWasSuspended; /* was the previous action suspended? */
	msg_t *pMsg;
} processMsgDoActions_t;
DEFFUNC_llExecFunc(processMsgDoActions)
{
	DEFiRet;
	rsRetVal iRetMod;	/* return value of module - we do not always pass that back */
	action_t *pAction = (action_t*) pData;
	processMsgDoActions_t *pDoActData = (processMsgDoActions_t*) pParam;

	assert(pAction != NULL);

	if((pAction->bExecWhenPrevSusp  == 1) && (pDoActData->bPrevWasSuspended == 0)) {
		dbgprintf("not calling action because the previous one is not suspended\n");
		ABORT_FINALIZE(RS_RET_OK);
	}

	iRetMod = actionCallAction(pAction, pDoActData->pMsg);
	if(iRetMod == RS_RET_DISCARDMSG) {
		ABORT_FINALIZE(RS_RET_DISCARDMSG);
	} else if(iRetMod == RS_RET_SUSPENDED) {
		/* indicate suspension for next module to be called */
		pDoActData->bPrevWasSuspended = 1;
	} else {
		pDoActData->bPrevWasSuspended = 0;
	}

finalize_it:
	RETiRet;
}


/* This functions looks at the given message and checks if it matches the
 * provided filter condition.
 */
static rsRetVal
shouldProcessThisMessage(rule_t *pRule, msg_t *pMsg, int *bProcessMsg)
{
	DEFiRet;
	unsigned short pbMustBeFreed;
	uchar *pszPropVal;
	int bRet = 0;
	size_t propLen;
	vm_t *pVM = NULL;
	var_t *pResult = NULL;

	ISOBJ_TYPE_assert(pRule, rule);
	assert(pMsg != NULL);

	/* we first have a look at the global, BSD-style block filters (for tag
	 * and host). Only if they match, we evaluate the actual filter.
	 * rgerhards, 2005-10-18
	 */
	if(pRule->eHostnameCmpMode == HN_NO_COMP) {
		/* EMPTY BY INTENSION - we check this value first, because
		 * it is the one most often used, so this saves us time!
		 */
	} else if(pRule->eHostnameCmpMode == HN_COMP_MATCH) {
		if(rsCStrSzStrCmp(pRule->pCSHostnameComp, (uchar*) getHOSTNAME(pMsg), getHOSTNAMELen(pMsg))) {
			/* not equal, so we are already done... */
			dbgprintf("hostname filter '+%s' does not match '%s'\n", 
				rsCStrGetSzStrNoNULL(pRule->pCSHostnameComp), getHOSTNAME(pMsg));
			FINALIZE;
		}
	} else { /* must be -hostname */
		if(!rsCStrSzStrCmp(pRule->pCSHostnameComp, (uchar*) getHOSTNAME(pMsg), getHOSTNAMELen(pMsg))) {
			/* not equal, so we are already done... */
			dbgprintf("hostname filter '-%s' does not match '%s'\n", 
				rsCStrGetSzStrNoNULL(pRule->pCSHostnameComp), getHOSTNAME(pMsg));
			FINALIZE;
		}
	}
	
	if(pRule->pCSProgNameComp != NULL) {
		int bInv = 0, bEqv = 0, offset = 0;
		if(*(rsCStrGetSzStrNoNULL(pRule->pCSProgNameComp)) == '-') {
			if(*(rsCStrGetSzStrNoNULL(pRule->pCSProgNameComp) + 1) == '-')
				offset = 1;
			else {
				bInv = 1;
				offset = 1;
			}
		}
		if(!rsCStrOffsetSzStrCmp(pRule->pCSProgNameComp, offset,
			(uchar*) getProgramName(pMsg, LOCK_MUTEX), getProgramNameLen(pMsg, LOCK_MUTEX)))
			bEqv = 1;

		if((!bEqv && !bInv) || (bEqv && bInv)) {
			/* not equal or inverted selection, so we are already done... */
			DBGPRINTF("programname filter '%s' does not match '%s'\n", 
				rsCStrGetSzStrNoNULL(pRule->pCSProgNameComp), getProgramName(pMsg, LOCK_MUTEX));
			FINALIZE;
		}
	}
	
	/* done with the BSD-style block filters */

	if(pRule->f_filter_type == FILTER_PRI) {
		/* skip messages that are incorrect priority */
dbgprintf("testing filter, f_pmask %d\n", pRule->f_filterData.f_pmask[pMsg->iFacility]);
		if ( (pRule->f_filterData.f_pmask[pMsg->iFacility] == TABLE_NOPRI) || \
		    ((pRule->f_filterData.f_pmask[pMsg->iFacility] & (1<<pMsg->iSeverity)) == 0) )
			bRet = 0;
		else
			bRet = 1;
	} else if(pRule->f_filter_type == FILTER_EXPR) {
		CHKiRet(vm.Construct(&pVM));
		CHKiRet(vm.ConstructFinalize(pVM));
		CHKiRet(vm.SetMsg(pVM, pMsg));
		CHKiRet(vm.ExecProg(pVM, pRule->f_filterData.f_expr->pVmprg));
		CHKiRet(vm.PopBoolFromStack(pVM, &pResult));
		dbgprintf("result of expression evaluation: %lld\n", pResult->val.num);
		/* VM is destructed on function exit */
		bRet = (pResult->val.num) ? 1 : 0;
	} else {
		assert(pRule->f_filter_type == FILTER_PROP); /* assert() just in case... */
		pszPropVal = MsgGetProp(pMsg, NULL, pRule->f_filterData.prop.propID, &propLen, &pbMustBeFreed);

		/* Now do the compares (short list currently ;)) */
		switch(pRule->f_filterData.prop.operation ) {
		case FIOP_CONTAINS:
			if(rsCStrLocateInSzStr(pRule->f_filterData.prop.pCSCompValue, (uchar*) pszPropVal) != -1)
				bRet = 1;
			break;
		case FIOP_ISEQUAL:
			if(rsCStrSzStrCmp(pRule->f_filterData.prop.pCSCompValue,
					  pszPropVal, ustrlen(pszPropVal)) == 0)
				bRet = 1; /* process message! */
			break;
		case FIOP_STARTSWITH:
			if(rsCStrSzStrStartsWithCStr(pRule->f_filterData.prop.pCSCompValue,
					  pszPropVal, ustrlen(pszPropVal)) == 0)
				bRet = 1; /* process message! */
			break;
		case FIOP_REGEX:
			if(rsCStrSzStrMatchRegex(pRule->f_filterData.prop.pCSCompValue,
					(unsigned char*) pszPropVal, 0, &pRule->f_filterData.prop.regex_cache) == RS_RET_OK)
				bRet = 1;
			break;
		case FIOP_EREREGEX:
			if(rsCStrSzStrMatchRegex(pRule->f_filterData.prop.pCSCompValue,
					  (unsigned char*) pszPropVal, 1, &pRule->f_filterData.prop.regex_cache) == RS_RET_OK)
				bRet = 1;
			break;
		default:
			/* here, it handles NOP (for performance reasons) */
			assert(pRule->f_filterData.prop.operation == FIOP_NOP);
			bRet = 1; /* as good as any other default ;) */
			break;
		}

		/* now check if the value must be negated */
		if(pRule->f_filterData.prop.isNegated)
			bRet = (bRet == 1) ?  0 : 1;

		if(Debug) {
			dbgprintf("Filter: check for property '%s' (value '%s') ",
			        propIDToName(pRule->f_filterData.prop.propID), pszPropVal);
			if(pRule->f_filterData.prop.isNegated)
				dbgprintf("NOT ");
			dbgprintf("%s '%s': %s\n",
			       getFIOPName(pRule->f_filterData.prop.operation),
			       rsCStrGetSzStrNoNULL(pRule->f_filterData.prop.pCSCompValue),
			       bRet ? "TRUE" : "FALSE");
		}

		/* cleanup */
		if(pbMustBeFreed)
			free(pszPropVal);
	}

finalize_it:
	/* destruct in any case, not just on error, but it makes error handling much easier */
	if(pVM != NULL)
		vm.Destruct(&pVM);

	if(pResult != NULL)
		var.Destruct(&pResult);

	*bProcessMsg = bRet;
	RETiRet;
}



/* Process (consume) a received message. Calls the actions configured.
 * rgerhards, 2005-10-13
 */
static rsRetVal
processMsg(rule_t *pThis, msg_t *pMsg)
{
	int bProcessMsg;
	processMsgDoActions_t DoActData;
	DEFiRet;

	ISOBJ_TYPE_assert(pThis, rule);
	assert(pMsg != NULL);

	/* first check the filters... */
	CHKiRet(shouldProcessThisMessage(pThis, pMsg, &bProcessMsg));
	if(bProcessMsg) {
		DoActData.pMsg = pMsg;
		DoActData.bPrevWasSuspended = 0;
		CHKiRet(llExecFunc(&pThis->llActList, processMsgDoActions, (void*)&DoActData));
	}

finalize_it:
	RETiRet;
}


/* Standard-Constructor
 */
BEGINobjConstruct(rule) /* be sure to specify the object type also in END macro! */
ENDobjConstruct(rule)


/* ConstructionFinalizer
 * rgerhards, 2008-01-09
 */
static rsRetVal
ruleConstructFinalize(rule_t *pThis)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, rule);

	/* note: actionDestruct is from action.c API! */
	CHKiRet(llInit(&pThis->llActList, actionDestruct, NULL, NULL));
	
finalize_it:
	RETiRet;
}


/* destructor for the rule object */
BEGINobjDestruct(rule) /* be sure to specify the object type also in END and CODESTART macros! */
CODESTARTobjDestruct(rule)
	if(pThis->pCSHostnameComp != NULL)
		rsCStrDestruct(&pThis->pCSHostnameComp);
	if(pThis->pCSProgNameComp != NULL)
		rsCStrDestruct(&pThis->pCSProgNameComp);

	if(pThis->f_filter_type == FILTER_PROP) {
		if(pThis->f_filterData.prop.pCSCompValue != NULL)
			rsCStrDestruct(&pThis->f_filterData.prop.pCSCompValue);
		if(pThis->f_filterData.prop.regex_cache != NULL)
			rsCStrRegexDestruct(&pThis->f_filterData.prop.regex_cache);
	} else if(pThis->f_filter_type == FILTER_EXPR) {
		if(pThis->f_filterData.f_expr != NULL)
			expr.Destruct(&pThis->f_filterData.f_expr);
	}

	llDestroy(&pThis->llActList);
ENDobjDestruct(rule)


/* set the associated ruleset */
static rsRetVal
setAssRuleset(rule_t *pThis, ruleset_t *pRuleset)
{
	DEFiRet;
	ISOBJ_TYPE_assert(pThis, rule);
	ISOBJ_TYPE_assert(pRuleset, ruleset);
	pThis->pRuleset = pRuleset;
	RETiRet;
}

/* get the associated ruleset (may be NULL if not set!) */
static ruleset_t*
getAssRuleset(rule_t *pThis)
{
	ISOBJ_TYPE_assert(pThis, rule);
	return pThis->pRuleset;
}


/* helper to DebugPrint, to print out all actions via
 * the llExecFunc() facility.
 */
DEFFUNC_llExecFunc(dbgPrintInitInfoAction)
{
	DEFiRet;
	iRet = actionDbgPrint((action_t*) pData);
	dbgprintf("\n");
	RETiRet;
}


/* debugprint for the rule object */
BEGINobjDebugPrint(rule) /* be sure to specify the object type also in END and CODESTART macros! */
	int i;
CODESTARTobjDebugPrint(rule)
	dbgoprint((obj_t*) pThis, "rsyslog rule:\n");
	if(pThis->pCSProgNameComp != NULL)
		dbgprintf("tag: '%s'\n", rsCStrGetSzStrNoNULL(pThis->pCSProgNameComp));
	if(pThis->eHostnameCmpMode != HN_NO_COMP)
		dbgprintf("hostname: %s '%s'\n",
			pThis->eHostnameCmpMode == HN_COMP_MATCH ?
				"only" : "allbut",
			rsCStrGetSzStrNoNULL(pThis->pCSHostnameComp));
	if(pThis->f_filter_type == FILTER_PRI) {
		for (i = 0; i <= LOG_NFACILITIES; i++)
			if (pThis->f_filterData.f_pmask[i] == TABLE_NOPRI)
				dbgprintf(" X ");
			else
				dbgprintf("%2X ", pThis->f_filterData.f_pmask[i]);
	} else if(pThis->f_filter_type == FILTER_EXPR) {
		dbgprintf("EXPRESSION-BASED Filter: can currently not be displayed");
	} else {
		dbgprintf("PROPERTY-BASED Filter:\n");
		dbgprintf("\tProperty.: '%s'\n", propIDToName(pThis->f_filterData.prop.propID));
		dbgprintf("\tOperation: ");
		if(pThis->f_filterData.prop.isNegated)
			dbgprintf("NOT ");
		dbgprintf("'%s'\n", getFIOPName(pThis->f_filterData.prop.operation));
		dbgprintf("\tValue....: '%s'\n",
		       rsCStrGetSzStrNoNULL(pThis->f_filterData.prop.pCSCompValue));
		dbgprintf("\tAction...: ");
	}

	dbgprintf("\nActions:\n");
	llExecFunc(&pThis->llActList, dbgPrintInitInfoAction, NULL); /* actions */

	dbgprintf("\n");
ENDobjDebugPrint(rule)


/* queryInterface function
 * rgerhards, 2008-02-21
 */
BEGINobjQueryInterface(rule)
CODESTARTobjQueryInterface(rule)
	if(pIf->ifVersion != ruleCURR_IF_VERSION) { /* check for current version, increment on each change */
		ABORT_FINALIZE(RS_RET_INTERFACE_NOT_SUPPORTED);
	}

	/* ok, we have the right interface, so let's fill it
	 * Please note that we may also do some backwards-compatibility
	 * work here (if we can support an older interface version - that,
	 * of course, also affects the "if" above).
	 */
	pIf->Construct = ruleConstruct;
	pIf->ConstructFinalize = ruleConstructFinalize;
	pIf->Destruct = ruleDestruct;
	pIf->DebugPrint = ruleDebugPrint;

	pIf->IterateAllActions = iterateAllActions;
	pIf->ProcessMsg = processMsg;
	pIf->SetAssRuleset = setAssRuleset;
	pIf->GetAssRuleset = getAssRuleset;
finalize_it:
ENDobjQueryInterface(rule)


/* Exit the rule class.
 * rgerhards, 2009-04-06
 */
BEGINObjClassExit(rule, OBJ_IS_CORE_MODULE) /* class, version */
	objRelease(errmsg, CORE_COMPONENT);
	objRelease(expr, CORE_COMPONENT);
	objRelease(var, CORE_COMPONENT);
	objRelease(vm, CORE_COMPONENT);
ENDObjClassExit(rule)


/* Initialize the rule class. Must be called as the very first method
 * before anything else is called inside this class.
 * rgerhards, 2008-02-19
 */
BEGINObjClassInit(rule, 1, OBJ_IS_CORE_MODULE) /* class, version */
	/* request objects we use */
	CHKiRet(objUse(errmsg, CORE_COMPONENT));
	CHKiRet(objUse(expr, CORE_COMPONENT));
	CHKiRet(objUse(var, CORE_COMPONENT));
	CHKiRet(objUse(vm, CORE_COMPONENT));

	/* set our own handlers */
	OBJSetMethodHandler(objMethod_DEBUGPRINT, ruleDebugPrint);
	OBJSetMethodHandler(objMethod_CONSTRUCTION_FINALIZER, ruleConstructFinalize);
ENDObjClassInit(rule)

/* vi:set ai:
 */
