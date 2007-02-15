/*	$RuOBSD: hproc.c,v 1.3 2004/04/13 14:46:09 form Exp $	*/

/*
 * Copyright (c) 2004 Oleg Safiullin <form@pdp-11.org.ru>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/systm.h>
#include <sys/lkm.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/kthread.h>
#include <sys/stat.h>
#include <sys/namei.h>
#include <sys/vnode.h>

#include "hproc.h"


extern int lkmexists(struct lkm_table *);
static int hproc_load(struct lkm_table *, int);
int hproc_lkmentry(struct lkm_table *, int, int);
static int hproc_kill(struct proc *, void *, register_t *);
static int hproc_sysctl(struct proc *, void *, register_t *);
static int hproc_nprocs(uid_t);
static int hproc_doproc(uid_t, int *, u_int, char *, size_t *);
static int hproc_trusted(struct proc *p);
static int hproc_unlink(const char *);
static int hproc_mknod(const char *, mode_t, dev_t);
static int hproc_openclose(dev_t, int, int, struct proc *);
static int hproc_ioctl(dev_t, u_long, caddr_t, int, struct proc *);
static void hproc_kthread(void *);


static sy_call_t *system_kill;
static sy_call_t *system_sysctl;
static gid_t hproc_gid;
static u_int32_t hproc_flags = HPF_ENABLED;

#ifndef dev_type_poll
#define dev_type_poll(n)		int n(dev_t, int, struct proc *)
#endif

static struct cdevsw hproc_cdevsw = {
	hproc_openclose,		/* open */
	hproc_openclose,		/* close */
	(dev_type_read((*))) enodev,	/* read */
	(dev_type_write((*))) enodev,	/* write */
	hproc_ioctl,			/* ioctl */
	(dev_type_stop((*))) enodev,	/* stop */
	NULL,				/* tty */
	(dev_type_poll((*))) enodev,	/* poll */
	(dev_type_mmap((*))) enodev	/* mmap */
};

MOD_DEV("hproc", LM_DT_CHAR, -1, &hproc_cdevsw)


int
hproc_lkmentry(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, hproc_load, hproc_load, lkm_nofunc)
}

static int
hproc_load(struct lkm_table *lkmtp, int cmd)
{
	int error;

	switch (cmd) {
	case LKM_E_LOAD:
		if (lkmexists(lkmtp))
			return (EEXIST);
		error = kthread_create(hproc_kthread, &_module, NULL, "hprocd");
		if (error != 0)
			return (error);
		system_kill = sysent[SYS_kill].sy_call;
		system_sysctl = sysent[SYS___sysctl].sy_call;
		sysent[SYS_kill].sy_call = hproc_kill;
		sysent[SYS___sysctl].sy_call = hproc_sysctl;
		break;
	case LKM_E_UNLOAD:
		sysent[SYS___sysctl].sy_call = system_sysctl;
		sysent[SYS_kill].sy_call = system_kill;
		wakeup(&hproc_cdevsw);
		(void)tsleep(&_module, PVM, "exit", 0);
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
hproc_kill(struct proc *p, void *v, register_t *retval)
{
	int error;

	error = system_kill(p, v, retval);
	if (error == EPERM && !hproc_trusted(p))
		return (ESRCH);
	return (error);
}

static int
hproc_sysctl(struct proc *p, void *v, register_t *retval)
{
	struct sys___sysctl_args *uap = v;
	struct proc *vp;
	int name[CTL_MAXNAME];
	int error;

	if (SCARG(uap, namelen) < 2 || SCARG(uap, namelen) > CTL_MAXNAME)
		return (EINVAL);
	if ((error = copyin(SCARG(uap, name), name,
	    SCARG(uap, namelen) * sizeof(int))) != 0)
		return (error);
	if (name[0] != CTL_KERN || hproc_trusted(p))
		return (system_sysctl(p, v, retval));

	switch (name[1]) {
	case KERN_NPROCS:
		return (sysctl_rdint(SCARG(uap, old), SCARG(uap, oldlenp),
		    SCARG(uap, new), hproc_nprocs(p->p_ucred->cr_uid)));
	case KERN_PROC:
#ifdef KERN_PROC2
	case KERN_PROC2:
#endif
		return (hproc_doproc(p->p_ucred->cr_uid, SCARG(uap, name) + 1,
		    SCARG(uap, namelen) - 1, SCARG(uap, old),
		    SCARG(uap, oldlenp)));
	case KERN_PROC_ARGS:
		if (SCARG(uap, namelen) < 4)
			return (EINVAL);
		if ((vp = pfind((pid_t)name[2])) == NULL ||
		    vp->p_cred->p_ruid != p->p_ucred->cr_uid)
			return (ESRCH);
		break;
	}

	if ((error = system_sysctl(p, v, retval)) != 0)
		return (error);

	return (error);
}

static int
hproc_nprocs(uid_t euid)
{
	struct proc *p;
	int n;

	n = 0;
	for (p = LIST_FIRST(&allproc); p != NULL; p = LIST_NEXT(p, p_list))
		if (p->p_cred->p_ruid == euid)
			n++;
	return (n);
}


#define KERN_PROCSLOP	(5 * sizeof (struct kinfo_proc))

static int
hproc_doproc(uid_t euid, int *name, u_int namelen, char *where, size_t *sizep)
{
#ifdef KERN_PROC2
	struct kinfo_proc2 kproc2;
#endif
	struct eproc eproc;
	struct proc *p;
	char *dp;
	int arg, buflen, doingzomb, elem_size, elem_count;
	int error, needed, type, op;

	dp = where;
	buflen = where != NULL ? *sizep : 0;
	needed = error = 0;
	type = name[0];

#ifdef KERN_PROC2
	if (type == KERN_PROC) {
#endif
		if (namelen != 3 && !(namelen == 2 &&
		    (name[1] == KERN_PROC_ALL || name[1] == KERN_PROC_KTHREAD)))
			return (EINVAL);
		op = name[1];
		arg = op == KERN_PROC_ALL ? 0 : name[2];
		elem_size = elem_count = 0;
#ifdef KERN_PROC2
	}
	 else /* if (type == KERN_PROC2) */ {
		if (namelen != 5 || name[3] < 0 || name[4] < 0)
			return (EINVAL);
		op = name[1];
		arg = name[2];
		elem_size = name[3];
		elem_count = name[4];
	}
#endif	/* KERN_PROC2 */
	p = LIST_FIRST(&allproc);
	doingzomb = 0;
again:
	for (; p != 0; p = LIST_NEXT(p, p_list)) {
		/*
		 * Skip embryonic processes.
		 */
		if (p->p_stat == SIDL || p->p_cred->p_ruid != euid)
			continue;
		/*
		 * TODO - make more efficient (see notes below).
		 */
		switch (op) {
		case KERN_PROC_PID:
			/* could do this with just a lookup */
			if (p->p_pid != (pid_t)arg)
				continue;
			break;
		case KERN_PROC_PGRP:
			/* could do this by traversing pgrp */
			if (p->p_pgrp->pg_id != (pid_t)arg)
				continue;
			break;
		case KERN_PROC_SESSION:
			if (p->p_session->s_leader == NULL ||
			    p->p_session->s_leader->p_pid != (pid_t)arg)
				continue;
			break;
		case KERN_PROC_TTY:
			if ((p->p_flag & P_CONTROLT) == 0 ||
			    p->p_session->s_ttyp == NULL ||
			    p->p_session->s_ttyp->t_dev != (dev_t)arg)
				continue;
			break;
		case KERN_PROC_UID:
			if (p->p_ucred->cr_uid != (uid_t)arg)
				continue;
			break;
		case KERN_PROC_RUID:
			if (p->p_cred->p_ruid != (uid_t)arg)
				continue;
			break;
		case KERN_PROC_ALL:
			if (p->p_flag & P_SYSTEM)
				continue;
			break;
		case KERN_PROC_KTHREAD:
			/* no filtering */
			break;
		default:
			return (EINVAL);
		}
#ifdef KERN_PROC2
		if (type == KERN_PROC) {
#endif
			if (buflen >= sizeof(struct kinfo_proc)) {
				fill_eproc(p, &eproc);
				error = copyout((caddr_t)p,
				    &((struct kinfo_proc *)dp)->kp_proc,
				    sizeof(struct proc));
				if (error)
					return (error);
				error = copyout((caddr_t)&eproc,
				    &((struct kinfo_proc *)dp)->kp_eproc,
				    sizeof(eproc));
				if (error)
					return (error);
				dp += sizeof(struct kinfo_proc);
				buflen -= sizeof(struct kinfo_proc);
			}
			needed += sizeof(struct kinfo_proc);
#ifdef KERN_PROC2
		}
		 else /* if (type == KERN_PROC2) */ {
			if (buflen >= elem_size && elem_count > 0) {
				fill_kproc2(p, &kproc2);
				/*
				 * Copy out elem_size, but not larger than
				 * the size of a struct kinfo_proc2.
				 */
				error = copyout(&kproc2, dp,
				    min(sizeof(kproc2), elem_size));
				if (error)
					return (error);
				dp += elem_size;
				buflen -= elem_size;
				elem_count--;
			}
			needed += elem_size;
		}
#endif	/* KERN_PROC2 */
	}
	if (doingzomb == 0) {
		p = LIST_FIRST(&zombproc);
		doingzomb++;
		goto again;
	}
	if (where != NULL) {
		*sizep = dp - where;
		if (needed > *sizep)
			return (ENOMEM);
	} else {
		needed += KERN_PROCSLOP;
		*sizep = needed;
	}
	return (0);
}

static int
hproc_trusted(struct proc *p)
{
	int trust;

	if (!suser(p, 0))
		return (1);
	trust = (!p->p_ucred->cr_uid || groupmember(hproc_gid, p->p_ucred));
	return (hproc_flags & HPF_DENYGID ? !trust : trust);
}

static int
hproc_unlink(const char *path)
{
	struct proc *p = curproc;
	struct vnode *vp;
	struct nameidata nd;
	int error;

	NDINIT(&nd, DELETE, LOCKPARENT | LOCKLEAF, UIO_SYSSPACE, path, p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	(void)uvm_vnp_uncache(vp);

	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);

	return (VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd));
}

static int
hproc_mknod(const char *path, mode_t mode, dev_t dev)
{
	struct proc *p = curproc;
	struct vattr vattr;
	struct nameidata nd;
	struct vnode *vp;
	int error;

	NDINIT(&nd, CREATE, LOCKPARENT, UIO_SYSSPACE, path, p);
	if ((error = namei(&nd)) != 0)
		return (error);
	if ((vp = nd.ni_vp) != NULL)
		return (EEXIST);
	VATTR_NULL(&vattr);
	vattr.va_mode = mode & ALLPERMS;
	vattr.va_type = VCHR;
	vattr.va_rdev = dev;
	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);

	return (VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr));
}

static int
hproc_openclose(dev_t dev, int oflags, int devtype, struct proc *p)
{
	return (minor(dev) ? ENXIO : 0);
}

static int
hproc_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct proc *p)
{
	u_int32_t *flags;
	gid_t *gid;

	switch (cmd) {
	case HIOCGFLAGS:
		flags = (u_int32_t *)data;
		*flags = hproc_flags;
		break;
	case HIOCSFLAGS:
		if (suser(p, 0))
			return (EPERM);
		flags = (u_int32_t *)data;
		hproc_flags = *flags;
		if (hproc_flags & HPF_ENABLED) {
			sysent[SYS_kill].sy_call = hproc_kill;
			sysent[SYS___sysctl].sy_call = hproc_sysctl;
		} else {
			sysent[SYS_kill].sy_call = system_kill;
			sysent[SYS___sysctl].sy_call = system_sysctl;
		}
		break;
	case HIOCGGID:
		gid = (gid_t *)data;
		*gid = hproc_gid;
		break;
	case HIOCSGID:
		if (suser(p, 0))
			return (EPERM);
		gid = (gid_t *)data;
		hproc_gid = *gid;
		break;
	default:
		return (ENODEV);
	}
	return (0);
}

static void
hproc_kthread(void *arg)
{
	struct lkm_dev *lkm = arg;

	(void)hproc_unlink(HPROC_DEV);
	(void)hproc_mknod(HPROC_DEV, HPROC_MODE, makedev(lkm->lkm_offset, 0));
	(void)tsleep(&hproc_cdevsw, PVM, "hproc", 0);
	(void)hproc_unlink(HPROC_DEV);
	wakeup(&_module);
	kthread_exit(0);
}
