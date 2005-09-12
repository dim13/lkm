/* $Id$ */
/*
 * Copyright (c) 2005 Dimitri Sokolyuk <demon@vhost.dyndns.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/lkm.h>
#include <sys/proc.h>
#include <sys/syscall.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <linux_syscall.h>
#include <linux_types.h>
#include <linux_resource.h>
#include <linux_signal.h>
#include <linux_syscallargs.h>

MOD_MISC("rlimit");

extern struct sysent linux_sysent[];

/* OpenBSD dosn't support RLIMIT_AS, so if a linux app tries to get
 * its value, return a value of RLIMIT_DATA instead
 */

int
my_getrlimit(struct proc *p, void *v, register_t *retval)
{
	register struct linux_sys_getrlimit_args *uap = v;

	if (SCARG(uap, which) == LINUX_RLIMIT_AS)
		SCARG(uap, which) = LINUX_RLIMIT_DATA;

	return (linux_sys_getrlimit(p, v, retval));
}

int
rlimit_handler(struct lkm_table *lkmtp, int cmd)
{
	switch (cmd) {
	case LKM_E_LOAD:
		linux_sysent[LINUX_SYS_getrlimit].sy_call = my_getrlimit;
		break;

	case LKM_E_UNLOAD:
		linux_sysent[LINUX_SYS_getrlimit].sy_call = linux_sys_getrlimit;
		break;
	}

	return (0);
}

int
rlimit(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, rlimit_handler, rlimit_handler, lkm_nofunc);
}
