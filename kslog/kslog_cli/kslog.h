/*
 * kslog.h
 *
 * kslog defines, etc.
 *
 * mike@gravitino.net
 */
 
// keystroke circular buffer size
#define CBUF_SIZE 1024		

// ioctl cmd structure
typedef struct kslog_op
{
	// id if related to operation (PID/UID)
	int val;
} kslog_op;

#define KSLOG_SETPID	_IOW('O', 0, struct kslog_op)
#define KSLOG_SETUID	_IOW('O', 1, struct kslog_op)
#define KSLOG_START	_IOW('O', 2, struct kslog_op)
#define KSLOG_STOP	_IOW('O', 3, struct kslog_op)
#define KSLOG_STATUS	_IOR('O', 4, struct kslog_op)
