/* $Id$ */
/*
 * Copyright (c) 2005 Dimitri Sokolyuk <demon@dim13.org>
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

MOD_MISC("gsrlimit");

char *gsrlimit_name[] = {
	"cpu time in milliseconds",
	"maximum file size",
	"data size",
	"stack size",
	"core file size",
	"resident set size",
	"locked-in-memory address space",
	"number of processes",
	"number of open files",
	NULL
};

int
my_getrlimit(struct proc *p, void *v, register_t *retval)
{
	register struct sys_getrlimit_args *uap = v;
	int ret;

	ret = sys_getrlimit(p, v, retval);

	printf("getrlimit: %s (%llu/%llu)\n",
		SCARG(uap, which) >= RLIM_NLIMITS ? "unknown" : gsrlimit_name[SCARG(uap, which)],
		SCARG(uap, rlp)->rlim_cur, SCARG(uap, rlp)->rlim_max);

	return (ret);
}

int
my_setrlimit(struct proc *p, void *v, register_t *retval)
{
	register struct sys_getrlimit_args *uap = v;
	int ret;

	ret = sys_setrlimit(p, v, retval);

	printf("setrlimit: %s (%llu/%llu)\n",
		SCARG(uap, which) >= RLIM_NLIMITS ? "unknown" : gsrlimit_name[SCARG(uap, which)],
		SCARG(uap, rlp)->rlim_cur, SCARG(uap, rlp)->rlim_max);

	return (ret);
}

int
gsrlimit_handler(struct lkm_table *lkmtp, int cmd)
{
	switch (cmd) {
	case LKM_E_LOAD:
		printf("fake getrlimit/setrlimit\n");
		sysent[SYS_getrlimit].sy_call = my_getrlimit;
		sysent[SYS_setrlimit].sy_call = my_setrlimit;
		break;

	case LKM_E_UNLOAD:
		printf("restore getrlimit/setrlimit\n");
		sysent[SYS_getrlimit].sy_call = sys_getrlimit;
		sysent[SYS_setrlimit].sy_call = sys_setrlimit;
		break;
	}

	return (0);
}

int
gsrlimit(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, gsrlimit_handler, gsrlimit_handler, lkm_nofunc);
}
