/*
 * Copyright (c) 2003, Oleg Safiullin <form@pdp11.org.ru>
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/lkm.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>

#define KILL_SYSCALL_OFFSET		37
#define SYSCTL_SYSCALL_OFFSET		202

static int hidep_handle(struct lkm_table *, int);
int hidep(struct lkm_table *, int, int);
int lkmexists(struct lkm_table *);
static int hidep_sysctl(struct proc *, void *, register_t *);
static int hidep_kill(struct proc *, void *, register_t *);

sy_call_t *system_kill;
sy_call_t *system_sysctl;

MOD_MISC("hidep")

static int
hidep_handle(struct lkm_table *lkmtp, int cmd)
{
	switch (cmd) {
	case LKM_E_LOAD:
		if (lkmexists(lkmtp))
			return (EEXIST);
		system_kill = sysent[KILL_SYSCALL_OFFSET].sy_call;
		system_sysctl = sysent[SYSCTL_SYSCALL_OFFSET].sy_call;
		sysent[KILL_SYSCALL_OFFSET].sy_call = hidep_kill;
		sysent[SYSCTL_SYSCALL_OFFSET].sy_call = hidep_sysctl;
		break;
	case LKM_E_UNLOAD:
		sysent[KILL_SYSCALL_OFFSET].sy_call = system_kill;
		sysent[SYSCTL_SYSCALL_OFFSET].sy_call = system_sysctl;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

int
hidep(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, hidep_handle, hidep_handle, lkm_nofunc)
}

int
hidep_sysctl(struct proc *p, void *v, register_t *retval)
{
	struct sys___sysctl_args *uap = v;
	struct kinfo_proc *cp, *ep, *kp = SCARG(uap, old);
	size_t *size = SCARG(uap, oldlenp);
	int *mib = SCARG(uap, name);
	int error, nentries;

	error = system_sysctl(p, v, retval);
	if (error)
		return (error);

	if (mib[0] != CTL_KERN || mib[1] != KERN_PROC || !p->p_ucred->cr_uid ||
	    kp == NULL || (nentries = *size / sizeof(struct kinfo_proc)) == 0)
		return (0);

	for (ep = kp, cp = kp; --nentries >= 0; cp++) {
		if (cp->kp_eproc.e_pcred.p_ruid == p->p_ucred->cr_uid) {
			if (cp != ep)
				bcopy(cp, ep, sizeof(struct kinfo_proc));
			ep++;
		} else
			bzero(cp, sizeof(struct kinfo_proc));

	}
	*size = (u_int32_t)ep - (u_int32_t)kp;

	return (0);
}

int
hidep_kill(struct proc *p, void *v, register_t *retval)
{
	int error;

	error = system_kill(p, v, retval);

	return (error == EPERM ? ESRCH: error);
}
