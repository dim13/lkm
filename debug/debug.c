#include <sys/param.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/systm.h>
#include <sys/lkm.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/filedesc.h>
#include <sys/sysctl.h>


int debug_lkmentry(struct lkm_table *, int, int);
static int debug_sysctl(struct proc *, void *, register_t *);


struct sysent debug_sysent = {
	6,
	sizeof(struct sys___sysctl_args),
	0,
	debug_sysctl
};


MOD_SYSCALL("debug", SYS___sysctl, &debug_sysent)


int
debug_lkmentry(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, lkm_nofunc, lkm_nofunc, lkm_nofunc);
}

static int
debug_sysctl(struct proc *p, void *v, register_t *retval)
{
	struct sys___sysctl_args /* {
		syscallarg(int *) name;
		syscallarg(u_int) namelen;
		syscallarg(void *) old;
		syscallarg(size_t *) oldlenp;
		syscallarg(void *) new;
		syscallarg(size_t) newlen;
	} */ *uap = v;
	int error, level, name[CTL_MAXNAME];

	if (suser(p, 0) != 0)
		return (_module.lkm_oldent.sy_call(p, v, retval));

	if (SCARG(uap, namelen) > CTL_MAXNAME || SCARG(uap, namelen) < 2)
		return (EINVAL);
	if ((error = copyin(SCARG(uap, name), name,
	    SCARG(uap, namelen) * sizeof(int))) != 0)
		return (error);

	switch (name[0]) {
	case CTL_KERN:
		break;
	default:
		return (_module.lkm_oldent.sy_call(p, v, retval));
	}

	switch (name[1]) {
	case KERN_SECURELVL:
		break;
	default:
		return (_module.lkm_oldent.sy_call(p, v, retval));
	}

	level = securelevel;
	if ((error = sysctl_int(SCARG(uap, old), SCARG(uap, oldlenp),
	    SCARG(uap, new), SCARG(uap, newlen), &level)) != 0)
		return (error);
	securelevel = level;
	return (0);
}
