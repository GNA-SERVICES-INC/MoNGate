/* pam_session.c - PAM Session Management */

/*
 * $Id: pam_session.c,v 1.6 2006/07/24 15:47:40 kukuk Exp $
 */

#include "pam_private.h"

#include <stdio.h>

int pam_open_session(pam_handle_t *pamh, int flags)
{
    int retval;

    D(("called"));

    IF_NO_PAMH("pam_open_session", pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_MODULE(pamh)) {
	D(("called from module!?"));
	return PAM_SYSTEM_ERR;
    }
    retval = _pam_dispatch(pamh, flags, PAM_OPEN_SESSION);

#ifdef HAVE_LIBAUDIT
    retval = _pam_auditlog(pamh, PAM_OPEN_SESSION, retval, flags);
#endif                                                                                
    return retval;
}

int pam_close_session(pam_handle_t *pamh, int flags)
{
    int retval;

    D(("called"));

    IF_NO_PAMH("pam_close_session", pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_MODULE(pamh)) {
	D(("called from module!?"));
	return PAM_SYSTEM_ERR;
    }

    retval = _pam_dispatch(pamh, flags, PAM_CLOSE_SESSION);

#ifdef HAVE_LIBAUDIT
    retval = _pam_auditlog(pamh, PAM_CLOSE_SESSION, retval, flags);
#endif

    return retval;

}
