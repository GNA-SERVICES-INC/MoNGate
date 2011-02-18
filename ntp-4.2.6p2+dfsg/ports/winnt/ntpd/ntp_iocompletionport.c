#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined (HAVE_IO_COMPLETION_PORT)

#include <stddef.h>
#include <stdio.h>
#include <process.h>
#include <syslog.h>

#include "ntp_stdlib.h"
#include "ntp_machine.h"
#include "ntp_fp.h"
#include "ntp.h"
#include "ntpd.h"
#include "ntp_refclock.h"
#include "ntp_iocompletionport.h"
#include "transmitbuff.h"
#include "ntp_request.h"
#include "ntp_assert.h"
#include "clockstuff.h"
#include "ntp_io.h"
#include "ntp_lists.h"
#include "clockstuff.h"

/*
 * Request types
 */
enum {
	SOCK_RECV,
	SOCK_SEND,
	SERIAL_WAIT,
	SERIAL_READ,
	SERIAL_WRITE
};

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 201)		/* nonstd extension nameless union */
#endif

typedef struct IoCompletionInfo {
	OVERLAPPED		overlapped;	/* must be first */
	int			request_type;
	union {
		recvbuf_t *	recv_buf;
		transmitbuf_t *	trans_buf;
	};
#ifdef DEBUG
	struct IoCompletionInfo *link;
#endif
} IoCompletionInfo;

#ifdef _MSC_VER
# pragma warning(pop)
#endif

/*
 * local function definitions
 */
static int QueueSerialWait(struct refclockio *, recvbuf_t *buff, IoCompletionInfo *lpo, BOOL clear_timestamp);

static int OnSocketRecv(ULONG_PTR, IoCompletionInfo *, DWORD, int);
static int OnSerialWaitComplete(ULONG_PTR, IoCompletionInfo *, DWORD, int);
static int OnSerialReadComplete(ULONG_PTR, IoCompletionInfo *, DWORD, int);
static int OnWriteComplete(ULONG_PTR, IoCompletionInfo *, DWORD, int);

/* keep a list to traverse to free memory on debug builds */
#ifdef DEBUG
static void free_io_completion_port_mem(void);
IoCompletionInfo *	compl_info_list;
CRITICAL_SECTION	compl_info_lock;
#define LOCK_COMPL()	EnterCriticalSection(&compl_info_lock);
#define UNLOCK_COMPL()	LeaveCriticalSection(&compl_info_lock);
#endif

/* #define USE_HEAP */

#ifdef USE_HEAP
static HANDLE hHeapHandle = NULL;
#endif

static HANDLE hIoCompletionPort = NULL;

static HANDLE WaitableIoEventHandle = NULL;
static HANDLE WaitableExitEventHandle = NULL;

#ifdef NTPNEEDNAMEDHANDLE
#define WAITABLEIOEVENTHANDLE "WaitableIoEventHandle"
#else
#define WAITABLEIOEVENTHANDLE NULL
#endif

#define MAXHANDLES 3
HANDLE WaitHandles[MAXHANDLES] = { NULL, NULL, NULL };

IoCompletionInfo *
GetHeapAlloc(char *fromfunc)
{
	IoCompletionInfo *lpo;

#ifdef USE_HEAP
	lpo = HeapAlloc(hHeapHandle,
			HEAP_ZERO_MEMORY,
			sizeof(IoCompletionInfo));
#else
	lpo = emalloc(sizeof(*lpo));
	memset(lpo, 0, sizeof(*lpo));
#endif
	DPRINTF(3, ("Allocation %d memory for %s, ptr %x\n", sizeof(IoCompletionInfo), fromfunc, lpo));

#ifdef DEBUG
	LOCK_COMPL();
	LINK_SLIST(compl_info_list, lpo, link);
	UNLOCK_COMPL();
#endif

	return (lpo);
}

void
FreeHeap(IoCompletionInfo *lpo, char *fromfunc)
{
#ifdef DEBUG
	IoCompletionInfo *unlinked;

	DPRINTF(3, ("Freeing memory for %s, ptr %x\n", fromfunc, lpo));

	LOCK_COMPL();
	UNLINK_SLIST(unlinked, compl_info_list, lpo, link,
	    IoCompletionInfo);
	UNLOCK_COMPL();
#endif

#ifdef USE_HEAP
	HeapFree(hHeapHandle, 0, lpo);
#else
	free(lpo);
#endif
}

transmitbuf_t *
get_trans_buf()
{
	transmitbuf_t *tb  = emalloc(sizeof(*tb));
	return (tb);
}

void
free_trans_buf(transmitbuf_t *tb)
{
	free(tb);
}

HANDLE
get_io_event()
{
	return( WaitableIoEventHandle );
}
HANDLE
get_exit_event()
{
	return( WaitableExitEventHandle );
}

/*  This function will add an entry to the I/O completion port
 *  that will signal the I/O thread to exit (gracefully)
 */
static void
signal_io_completion_port_exit()
{
	if (!PostQueuedCompletionStatus(hIoCompletionPort, 0, 0, 0)) {
		msyslog(LOG_ERR, "Can't request service thread to exit: %m");
		exit(1);
	}
}

static unsigned WINAPI
iocompletionthread(void *NotUsed)
{
	BOOL bSuccess = FALSE;
	int errstatus = 0;
	DWORD BytesTransferred = 0;
	ULONG_PTR Key = 0;
	IoCompletionInfo * lpo = NULL;
	u_long time_next_ifscan_after_error = 0;

	UNUSED_ARG(NotUsed);

	/*
	 *	socket and refclock receive call gettimeofday()
	 *	so the I/O thread needs to be on the same 
	 *	processor as the main and timing threads
	 *	to ensure consistent QueryPerformanceCounter()
	 *	results.
	 */
	lock_thread_to_processor(GetCurrentThread());

	/*	Set the thread priority high enough so I/O will
	 *	preempt normal recv packet processing, but not
	 * 	higher than the timer sync thread.
	 */
	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL)) {
		msyslog(LOG_ERR, "Can't set thread priority: %m");
	}

	while (TRUE) {
		bSuccess = GetQueuedCompletionStatus(
					hIoCompletionPort, 
					&BytesTransferred, 
					&Key, 
					(LPOVERLAPPED *) &lpo, 
					INFINITE);
		if (lpo == NULL)
		{
			DPRINTF(2, ("Overlapped IO Thread Exiting\n"));
			break; /* fail */
		}
		
		/*
		 * Deal with errors
		 */
		if (bSuccess)
			errstatus = 0;
		else
		{
			errstatus = GetLastError();
			if (BytesTransferred == 0)
			{
				if (WSA_OPERATION_ABORTED == errstatus) {
					DPRINTF(4, ("Transfer Operation aborted\n"));
				} else if (ERROR_UNEXP_NET_ERR == errstatus) {
					/*
					 * We get this error when trying to send an the network
					 * interface is gone or has lost link.  Rescan interfaces
					 * to catch on sooner, but no more than once per minute.
					 * Once ntp is able to detect changes without polling
					 * this should be unneccessary
					 */
					if (time_next_ifscan_after_error < current_time) {
						time_next_ifscan_after_error = current_time + 60;
						timer_interfacetimeout(current_time);
					}
					DPRINTF(4, ("sendto unexpected network error, interface may be down\n"));
				}
			}
			else
			{
				msyslog(LOG_ERR, "sendto error after %d bytes: %m", BytesTransferred);
			}
		}

		/*
		 * Invoke the appropriate function based on
		 * the value of the request_type
		 */
		switch(lpo->request_type)
		{
		case SERIAL_WAIT:
			OnSerialWaitComplete(Key, lpo, BytesTransferred, errstatus);
			break;
		case SERIAL_READ:
			OnSerialReadComplete(Key, lpo, BytesTransferred, errstatus);
			break;
		case SOCK_RECV:
			OnSocketRecv(Key, lpo, BytesTransferred, errstatus);
			break;
		case SOCK_SEND:
		case SERIAL_WRITE:
			OnWriteComplete(Key, lpo, BytesTransferred, errstatus);
			break;
		default:
			DPRINTF(1, ("Unknown request type %d found in completion port\n",
				    lpo->request_type));
			break;
		}
	}

	return 0;
}

/*  Create/initialise the I/O creation port
 */
void
init_io_completion_port(
	void
	)
{
	unsigned tid;
	HANDLE thread;

#ifdef DEBUG
	InitializeCriticalSection(&compl_info_lock);
	atexit(&free_io_completion_port_mem);
#endif

#ifdef USE_HEAP
	/*
	 * Create a handle to the Heap
	 */
	hHeapHandle = HeapCreate(0, 20*sizeof(IoCompletionInfo), 0);
	if (hHeapHandle == NULL)
	{
		msyslog(LOG_ERR, "Can't initialize Heap: %m");
		exit(1);
	}
#endif

#if 0	/* transmitbuff.c unused, no need to initialize it */
	init_transmitbuff();
#endif

	/* Create the event used to signal an IO event
	 */
	WaitableIoEventHandle = CreateEvent(NULL, FALSE, FALSE, WAITABLEIOEVENTHANDLE);
	if (WaitableIoEventHandle == NULL) {
		msyslog(LOG_ERR,
		"Can't create I/O event handle: %m - another process may be running - EXITING");
		exit(1);
	}
	/* Create the event used to signal an exit event
	 */
	WaitableExitEventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (WaitableExitEventHandle == NULL) {
		msyslog(LOG_ERR,
		"Can't create exit event handle: %m - EXITING");
		exit(1);
	}

	/* Create the IO completion port
	 */
	hIoCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hIoCompletionPort == NULL) {
		msyslog(LOG_ERR, "Can't create I/O completion port: %m");
		exit(1);
	}

	/*
	 * Initialize the Wait Handles
	 */
	WaitHandles[0] = WaitableIoEventHandle;
	WaitHandles[1] = WaitableExitEventHandle; /* exit request */
	WaitHandles[2] = get_timer_handle();

	/* Have one thread servicing I/O - there were 4, but this would 
	 * somehow cause NTP to stop replying to ntpq requests; TODO
 	 */
	thread = (HANDLE)_beginthreadex(
		NULL, 
		0, 
		iocompletionthread, 
		NULL, 
		CREATE_SUSPENDED, 
		&tid);
	ResumeThread(thread);
	CloseHandle(thread);
}
	

#ifdef DEBUG
static void
free_io_completion_port_mem(
	void
	)
{
	IoCompletionInfo *	pci;

#if defined(_MSC_VER) && defined (_DEBUG)
	_CrtCheckMemory();
#endif
	LOCK_COMPL();
	while ((pci = compl_info_list) != NULL) {

#if 0	/* sockaddr with received-from address in recvbuf */
	/* is sometimes modified by system after we free it  */
	/* triggering heap corruption warning -- find a */
	/* better way to free it after I/O is surely done */
		/* this handles both xmit and recv buffs */
		if (pci->recv_buf != NULL) {
			DPRINTF(1, ("freeing xmit/recv buff %p\n", pci->recv_buf));
			free(pci->recv_buf);
		}
#endif

		FreeHeap(pci, "free_io_completion_port_mem");
		/* FreeHeap() removed this item from compl_info_list */
	}
	UNLOCK_COMPL()

#if defined(_MSC_VER) && defined (_DEBUG)
	_CrtCheckMemory();
#endif
}
#endif	/* DEBUG */


void
uninit_io_completion_port(
	void
	)
{
	if (hIoCompletionPort != NULL) {
		/*  Get each of the service threads to exit
		*/
		signal_io_completion_port_exit();
	}
}


static int
QueueSerialWait(
	struct refclockio *	rio,
	recvbuf_t *		buff,
	IoCompletionInfo *	lpo,
	BOOL			clear_timestamp
	)
{
	lpo->request_type = SERIAL_WAIT;
	lpo->recv_buf = buff;

	if (clear_timestamp)
		memset(&buff->recv_time, 0, sizeof(buff->recv_time));

	buff->fd = _get_osfhandle(rio->fd);
	if (!WaitCommEvent((HANDLE) buff->fd, (DWORD *)&buff->recv_buffer, (LPOVERLAPPED) lpo)) {
		if (ERROR_IO_PENDING != GetLastError()) {
			msyslog(LOG_ERR, "Can't wait on Refclock: %m");
			freerecvbuf(buff);
			return 0;
		}
	}
	return 1;
}


static int 
OnSerialWaitComplete(ULONG_PTR i, IoCompletionInfo *lpo, DWORD Bytes, int errstatus)
{
	recvbuf_t *buff;
	struct refclockio * rio = (struct refclockio *) i;
	struct peer *pp;
	l_fp arrival_time;
	DWORD comm_mask;
	DWORD modem_status;
	static const l_fp zero_time = { 0 };
	BOOL rc;

	get_systime(&arrival_time);

	/*
	 * Get the recvbuf pointer from the overlapped buffer.
	 */
	buff = lpo->recv_buf;
	comm_mask = (*(DWORD *)&buff->recv_buffer);
#ifdef DEBUG
		if (errstatus || comm_mask & ~(EV_RXFLAG | EV_RLSD)) {
			msyslog(LOG_ERR, "WaitCommEvent returned unexpected mask %x errstatus %d",
				comm_mask, errstatus);
			exit(-1);
		}
#endif
		if (comm_mask & EV_RLSD) { 
			modem_status = 0;
			GetCommModemStatus((HANDLE)buff->fd, &modem_status);
			if (modem_status & MS_RLSD_ON) {
				/*
				 * Use the timestamp from this PPS CD not
				 * the later end of line.
				 */
				buff->recv_time = arrival_time;
			}

			if (!(comm_mask & EV_RXFLAG)) {
				/*
				 * if we didn't see an end of line yet
				 * issue another wait for it.
				 */
				QueueSerialWait(rio, buff, lpo, FALSE);
				return 1;
			}
		}

		/*
		 * We've detected the end of line of serial input.
		 * Use this timestamp unless we already have a CD PPS
		 * timestamp in buff->recv_time.
		 */
		if (memcmp(&buff->recv_time, &zero_time, sizeof buff->recv_time)) {
			/*
			 * We will first see a user PPS timestamp here on either
			 * the first or second line of text.  Log a one-time
			 * message while processing the second line.
			 */
			if (1 == rio->recvcount) {
				pp = (struct peer *)rio->srcclock;
				msyslog(LOG_NOTICE, "Using user-mode PPS timestamp for %s",
					refnumtoa(&pp->srcadr));
			}
		} else {
			buff->recv_time = arrival_time;
		}

		/*
		 * Now that we have a complete line waiting, read it.
		 * There is still a race here, but we're likely to win.
		 */

		lpo->request_type = SERIAL_READ;

		rc = ReadFile(
			(HANDLE)buff->fd,
			buff->recv_buffer,
			sizeof(buff->recv_buffer),
			NULL,
			(LPOVERLAPPED)lpo);

		if (!rc && ERROR_IO_PENDING != GetLastError()) {
			msyslog(LOG_ERR, "Can't read from Refclock: %m");
			freerecvbuf(buff);
			return 0;
		}

	return 1;
}

/* Return 1 on Successful Read */
static int 
OnSerialReadComplete(ULONG_PTR i, IoCompletionInfo *lpo, DWORD Bytes, int errstatus)
{
	recvbuf_t *		buff;
	l_fp			cr_time;
	struct refclockio *	rio;

	rio = (struct refclockio *)i;
	/*
	 * Get the recvbuf pointer from the overlapped buffer.
	 */
	buff = lpo->recv_buf;

	/*
	 * ignore 0 bytes read due to timeout's and closure on fd
	 */
	if (!errstatus && Bytes) {
		buff->recv_length = (int) Bytes;
		buff->receiver = rio->clock_recv;
		buff->dstadr = NULL;
		buff->recv_srcclock = rio->srcclock;
		packets_received++;
		/*
		 * Eat the first line of input as it's possibly
		 * partial and if a PPS is present, it may not 
		 * have fired since the port was opened.
		 */
		if (rio->recvcount++) {
			cr_time = buff->recv_time;
			add_full_recv_buffer(buff);
			/*
			 * Mimic Unix line discipline and assume CR/LF
			 * line termination.  On Unix the CR terminates
			 * the line containing the timecode, and
			 * immediately after the LF terminates an empty
			 * line.  So synthesize the empty LF-terminated
			 * line using the same CR timestamp.  Both CR
			 * and LF are stripped by refclock_gtlin().
			 */
			buff = get_free_recv_buffer_alloc();
			buff->recv_time = cr_time;
			buff->recv_length = 0;
			buff->fd = _get_osfhandle(rio->fd);
			buff->receiver = rio->clock_recv;
			buff->dstadr = NULL;
			buff->recv_srcclock = rio->srcclock;
			add_full_recv_buffer(buff);
			/*
			 * Now signal we have something to process
			 */
			SetEvent(WaitableIoEventHandle);
			buff = get_free_recv_buffer_alloc();
		}
	}

	QueueSerialWait(rio, buff, lpo, TRUE);

	return 1;
}

/*  Add a reference clock data structures I/O handles to
 *  the I/O completion port. Return 1 if any error.
 */  
int
io_completion_port_add_clock_io(
	struct refclockio *rio
	)
{
	IoCompletionInfo *lpo;
	recvbuf_t *buff;

	if (NULL == CreateIoCompletionPort(
			(HANDLE)_get_osfhandle(rio->fd), 
			hIoCompletionPort, 
			(ULONG_PTR)rio,
			0)) {
		msyslog(LOG_ERR, "Can't add COM port to i/o completion port: %m");
		return 1;
	}

	lpo = GetHeapAlloc("io_completion_port_add_clock_io");
	if (NULL == lpo) {
		msyslog(LOG_ERR, "Can't allocate heap for completion port: %m");
		return 1;
	}

	buff = get_free_recv_buffer_alloc();
	QueueSerialWait(rio, buff, lpo, TRUE);
	return 0;
}

/*
 * Queue a receiver on a socket. Returns 0 if no buffer can be queued 
 *
 *  Note: As per the winsock documentation, we use WSARecvFrom. Using
 *	  ReadFile() is less efficient.
 */
static unsigned long 
QueueSocketRecv(
	SOCKET s,
	recvbuf_t *buff,
	IoCompletionInfo *lpo
	)
{
	WSABUF wsabuf;
	DWORD Flags;
	DWORD Result;

	lpo->request_type = SOCK_RECV;
	lpo->recv_buf = buff;

	if (buff != NULL) {
		Flags = 0;
		buff->fd = s;
		buff->recv_srcadr_len = sizeof(buff->recv_srcadr);
		wsabuf.buf = (char *)buff->recv_buffer;
		wsabuf.len = sizeof(buff->recv_buffer);

		if (SOCKET_ERROR == WSARecvFrom(buff->fd, &wsabuf, 1, 
						NULL, &Flags, 
						&buff->recv_srcadr.sa, 
						&buff->recv_srcadr_len, 
						(LPOVERLAPPED)lpo, NULL)) {
			Result = GetLastError();
			switch (Result) {
				case NO_ERROR :
				case WSA_IO_PENDING :
					break ;

				case WSAENOTSOCK :
					msyslog(LOG_ERR, "Can't read from non-socket fd %d: %m", (int)buff->fd);
					/* return the buffer */
					freerecvbuf(buff);
					return 0;
					break;

				case WSAEFAULT :
					msyslog(LOG_ERR, "The buffers parameter is incorrect: %m");
					/* return the buffer */
					freerecvbuf(buff);
					return 0;
				break;

				default :
				  /* nop */ ;
			}
		}
	}
	else 
		return 0;
	return 1;
}


/* Returns 0 if any Error */
static int 
OnSocketRecv(ULONG_PTR i, IoCompletionInfo *lpo, DWORD Bytes, int errstatus)
{
	struct recvbuf *buff = NULL;
	recvbuf_t *newbuff;
	l_fp arrival_time;
	struct interface * inter = (struct interface *) i;
	
	get_systime(&arrival_time);

	NTP_REQUIRE(NULL != lpo);
	NTP_REQUIRE(NULL != lpo->recv_buf);

	/*
	 * Convert the overlapped pointer back to a recvbuf pointer.
	 */
	buff = lpo->recv_buf;

	/*
	 * If the socket is closed we get an Operation Aborted error
	 * Just clean up
	 */
	if (errstatus == WSA_OPERATION_ABORTED)
	{
		freerecvbuf(buff);
		lpo->recv_buf = NULL;
		FreeHeap(lpo, "OnSocketRecv: Socket Closed");
		return (1);
	}

	/*
	 * Get a new recv buffer for the replacement socket receive
	 */
	newbuff = get_free_recv_buffer_alloc();
	QueueSocketRecv(inter->fd, newbuff, lpo);

	DPRINTF(4, ("%sfd %d %s recv packet mode is %d\n", 
		    (MODE_BROADCAST == get_packet_mode(buff))
			? " **** Broadcast "
			: "",
		    (int)buff->fd, stoa(&buff->recv_srcadr),
		    get_packet_mode(buff)));

	/*
	 * If we keep it add some info to the structure
	 */
	if (Bytes && !inter->ignore_packets) {
		memcpy(&buff->recv_time, &arrival_time, sizeof buff->recv_time);	
		buff->recv_length = (int) Bytes;
		buff->receiver = receive; 
		buff->dstadr = inter;

		DPRINTF(2, ("Received %d bytes fd %d in buffer %p from %s\n", 
			    Bytes, (int)buff->fd, buff, stoa(&buff->recv_srcadr)));

		packets_received++;
		inter->received++;
		add_full_recv_buffer(buff);
		/*
		 * Now signal we have something to process
		 */
		SetEvent(WaitableIoEventHandle);
	} else
		freerecvbuf(buff);

	return 1;
}


/*  Add a socket handle to the I/O completion port, and send 
 *  NTP_RECVS_PER_SOCKET recv requests to the kernel.
 */
extern int
io_completion_port_add_socket(SOCKET fd, struct interface *inter)
{
	IoCompletionInfo *lpo;
	recvbuf_t *buff;
	int n;

	if (fd != INVALID_SOCKET) {
		if (NULL == CreateIoCompletionPort((HANDLE)fd, 
		    hIoCompletionPort, (ULONG_PTR)inter, 0)) {
			msyslog(LOG_ERR, "Can't add socket to i/o completion port: %m");
			return 1;
		}
	}

	/*
	 * Windows 2000 bluescreens with bugcheck 0x76
	 * PROCESS_HAS_LOCKED_PAGES at ntpd process
	 * termination when using more than one pending
	 * receive per socket.  A runtime version test
	 * would allow using more on newer versions
	 * of Windows.
	 */

#define WINDOWS_RECVS_PER_SOCKET 1

	for (n = 0; n < WINDOWS_RECVS_PER_SOCKET; n++) {

		buff = get_free_recv_buffer_alloc();
		lpo = (IoCompletionInfo *) GetHeapAlloc("io_completion_port_add_socket");
		if (lpo == NULL)
		{
			msyslog(LOG_ERR, "Can't allocate heap for completion port: %m");
			return 1;
		}

		QueueSocketRecv(fd, buff, lpo);

	}
	return 0;
}

static int 
OnWriteComplete(ULONG_PTR i, IoCompletionInfo *lpo, DWORD Bytes, int errstatus)
{
	transmitbuf_t *buff;
	struct interface *inter;

	UNUSED_ARG(Bytes);

	buff = lpo->trans_buf;

	free_trans_buf(buff);
	lpo->trans_buf = NULL;

	if (SOCK_SEND == lpo->request_type) {
		switch (errstatus) {
		case WSA_OPERATION_ABORTED:
		case NO_ERROR:
			break;

		default:
			inter = (struct interface *)i;
			packets_notsent++;
			inter->notsent++;
			break;
		}
	}

	if (errstatus == WSA_OPERATION_ABORTED)
		FreeHeap(lpo, "OnWriteComplete: Socket Closed");
	else
		FreeHeap(lpo, "OnWriteComplete");
	return 1;
}


/*
 * Return value is really GetLastError-style error code
 * which is a DWORD but using int, which is large enough,
 * decreases #ifdef forest in ntp_io.c harmlessly.
 */
int	
io_completion_port_sendto(
	struct interface *inter,	
	struct pkt *pkt,	
	int len, 
	sockaddr_u* dest
	)
{
	WSABUF wsabuf;
	transmitbuf_t *buff;
	DWORD Result = ERROR_SUCCESS;
	int errval;
	int AddrLen;
	IoCompletionInfo *lpo;
	DWORD Flags;

	lpo = (IoCompletionInfo *) GetHeapAlloc("io_completion_port_sendto");

	if (lpo == NULL)
		return ERROR_OUTOFMEMORY;

	if (len <= sizeof(buff->pkt)) {
		buff = get_trans_buf();

		if (buff == NULL) {
			msyslog(LOG_ERR, "No more transmit buffers left - data discarded");
			FreeHeap(lpo, "io_completion_port_sendto");
			return ERROR_OUTOFMEMORY;
		}


		memcpy(&buff->pkt, pkt, len);
		wsabuf.buf = buff->pkt;
		wsabuf.len = len;

		AddrLen = SOCKLEN(dest);
		lpo->request_type = SOCK_SEND;
		lpo->trans_buf = buff;
		Flags = 0;

		Result = WSASendTo(inter->fd, &wsabuf, 1, NULL, Flags,
				   &dest->sa, AddrLen, 
				   (LPOVERLAPPED)lpo, NULL);

		if(Result == SOCKET_ERROR)
		{
			errval = WSAGetLastError();
			switch (errval) {

			case NO_ERROR :
			case WSA_IO_PENDING :
				Result = ERROR_SUCCESS;
				break ;

			/*
			 * Something bad happened
			 */
			default :
				msyslog(LOG_ERR,
					"WSASendTo(%s) error %d: %s",
					stoa(dest), errval, strerror(errval));
				free_trans_buf(buff);
				lpo->trans_buf = NULL;
				FreeHeap(lpo, "io_completion_port_sendto");
				break;
			}
		}
#ifdef DEBUG
		if (debug > 3)
			printf("WSASendTo - %d bytes to %s : %d\n", len, stoa(dest), Result);
#endif
		return (Result);
	}
	else {
#ifdef DEBUG
		if (debug) printf("Packet too large: %d Bytes\n", len);
#endif
		return ERROR_INSUFFICIENT_BUFFER;
	}
}


/*
 * async_write, clone of write(), used by some reflock drivers
 */
int	
async_write(
	int fd,
	const void *data,
	unsigned int count)
{
	transmitbuf_t *buff;
	IoCompletionInfo *lpo;
	DWORD BytesWritten;

	if (count > sizeof buff->pkt) {
#ifdef DEBUG
		if (debug) {
			printf("async_write: %d bytes too large, limit is %d\n",
				count, sizeof buff->pkt);
			exit(-1);
		}
#endif
		errno = ENOMEM;
		return -1;
	}

	buff = get_trans_buf();
	lpo = (IoCompletionInfo *) GetHeapAlloc("async_write");

	if (! buff || ! lpo) {
		if (buff) {
			free_trans_buf(buff);
			DPRINTF(1, ("async_write: out of memory\n"));
		} else
			msyslog(LOG_ERR, "No more transmit buffers left - data discarded");

		errno = ENOMEM;
		return -1;
	}

	lpo->request_type = SERIAL_WRITE;
	lpo->trans_buf = buff;
	memcpy(&buff->pkt, data, count);

	if (!WriteFile((HANDLE)_get_osfhandle(fd), buff->pkt, count,
		&BytesWritten, (LPOVERLAPPED)lpo)
		&& ERROR_IO_PENDING != GetLastError()) {

		msyslog(LOG_ERR, "async_write - error %m");
		free_trans_buf(buff);
		lpo->trans_buf = NULL;
		FreeHeap(lpo, "async_write");
		errno = EBADF;
		return -1;
	}

	return count;
}


/*
 * GetReceivedBuffers
 * Note that this is in effect the main loop for processing requests
 * both send and receive. This should be reimplemented
 */
int GetReceivedBuffers()
{
	isc_boolean_t have_packet = ISC_FALSE;
	while (!have_packet) {
		DWORD Index = WaitForMultipleObjects(MAXHANDLES, WaitHandles, FALSE, INFINITE);
		switch (Index) {
		case WAIT_OBJECT_0 + 0 : /* Io event */
# ifdef DEBUG
			if ( debug > 3 )
			{
				printf( "IoEvent occurred\n" );
			}
# endif
			have_packet = ISC_TRUE;
			break;
		case WAIT_OBJECT_0 + 1 : /* exit request */
			exit(0);
			break;
		case WAIT_OBJECT_0 + 2 : /* timer */
			timer();
			break;
		case WAIT_IO_COMPLETION : /* loop */
		case WAIT_TIMEOUT :
			break;
		case WAIT_FAILED:
			msyslog(LOG_ERR, "ntpd: WaitForMultipleObjects Failed: Error: %m");
			break;

			/* For now do nothing if not expected */
		default:
			break;		
				
		} /* switch */
	}

	return (full_recvbuffs());	/* get received buffers */
}

#else
  static int NonEmptyCompilationUnit;
#endif

