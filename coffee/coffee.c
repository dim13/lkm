/*
 * Copyright (c) 2001 Thomas Coffy.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TERRENCE R. LAMBERT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Basic lkm driver for making coffee from a shell.
 * 0x3bc for /dev/lp0, 0x378 for /dev/lp1, and 0x278 for /dev/lp2
 */ 

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/lkm.h>

#define PORT_ADDR 0x378		/* lpt1 */

int 	coffee_load	__P((struct lkm_table *, int));
int	coffee		__P((struct lkm_table *, int, int));
int	coffee_open	__P((dev_t, int, int, struct proc *));
int	coffee_read	__P((dev_t, struct uio *, int));

static struct cdevsw coffee_cdevsw = {
	coffee_open, (dev_type_close((*))) enodev,
	coffee_read, (dev_type_write((*))) enodev,
	(dev_type_ioctl((*))) enodev,
	(dev_type_stop((*))) enodev,
	0, seltrue, (dev_type_mmap((*))) enodev, 0
};

MOD_DEV("coffee", LM_DT_CHAR, -1, &coffee_cdevsw );

int
coffee(lkmtp, cmd, ver)
        struct lkm_table *lkmtp;        
        int cmd;
        int ver;
{
	DISPATCH(lkmtp, cmd, ver, coffee_load, lkm_nofunc, lkm_nofunc);	
        return 0;
}

int
coffee_load(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	if (cmd==LKM_E_LOAD) 
		printf("Coffee driver loaded.\n");
	if (cmd==LKM_E_UNLOAD)
                printf("Coffee driver unloaded.\n");
	return 0;
}

/*
 * Open the device
 */
int
coffee_open(dev, oflags, devtype, p)
	dev_t dev;
	int oflags;
	int devtype;
	struct proc *p;
{
        int s;

        /* set power everywhere on the // port */
        s = splhigh();
        outb(PORT_ADDR, 255);
        splx(s);

	return 0;
}	

int
coffee_read(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	int s;

        /* set power everywhere on the // port */
        s = splhigh();
        outb(PORT_ADDR, 0);
        splx(s);

        return 0;
}
