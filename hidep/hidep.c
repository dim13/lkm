/*
 * γοπωςιηθτ (γ) 2003 ομεη σαζιυμμιξ <ζοςνΰπδπ11.οςη.ςυ>
 * αμμ ςιηθτσ ςεσεςφεδ.
 *
 * ςεδιστςιβυτιοξ  αξδ  υσε  ιξ  σουςγε  αξδ  βιξαςω  ζοςνσ,  χιτθ ος χιτθουτ
 * νοδιζιγατιοξ,  αςε  πεςνιττεδ  πςοφιδεδ  τθατ  τθε  ζομμοχιξη   γοξδιτιοξσ
 * αςε νετ:
 * 1. ςεδιστςιβυτιοξσ  οζ  σουςγε γοδε νυστ ςεταιξ τθε αβοφε γοπωςιηθτ ξοτιγε
 *    υξνοδιζιεδ, τθισ μιστ οζ γοξδιτιοξσ, αξδ τθε ζομμοχιξη δισγμαινες.
 * 2. ςεδιστςιβυτιοξσ  ιξ  βιξαςω  ζοςν  νυστ  ςεπςοδυγε  τθε αβοφε γοπωςιηθτ
 *    ξοτιγε,  τθισ  μιστ  οζ  γοξδιτιοξσ αξδ τθε ζομμοχιξη δισγμαινες ιξ τθε
 *    δογυνεξτατιοξ αξδ/ος οτθες νατεςιαμσ πςοφιδεδ χιτθ τθε διστςιβυτιοξ.
 *
 * τθισ σοζτχαςε ισ πςοφιδεδ βω τθε αυτθος αξδ γοξτςιβυτοςσ ``ασ ισ'' αξδ αξω
 * εψπςεσσ  ος ινπμιεδ χαςςαξτιεσ, ιξγμυδιξη, βυτ ξοτ μινιτεδ το, τθε ινπμιεδ
 * χαςςαξτιεσ  οζ  νεςγθαξταβιμιτω  αξδ  ζιτξεσσ ζος α παςτιγυμας πυςποσε αςε
 * δισγμαινεδ. ιξ ξο εφεξτ σθαμμ τθε αυτθος ος γοξτςιβυτοςσ βε μιαβμε ζος αξω
 * διςεγτ, ιξδιςεγτ, ιξγιδεξταμ, σπεγιαμ, εψενπμαςω, ος γοξσερυεξτιαμ δαναηεσ
 * (ιξγμυδιξη,  βυτ  ξοτ  μινιτεδ  το,  πςογυςενεξτ  οζ  συβστιτυτε  ηοοδσ ος
 * σεςφιγεσ; μοσσ οζ υσε, δατα, ος πςοζιτσ; ος βυσιξεσσ ιξτεςςυπτιοξ) θοχεφες
 * γαυσεδ  αξδ  οξ  αξω  τθεοςω  οζ  μιαβιμιτω,  χθετθες  ιξ γοξτςαγτ, στςιγτ
 * μιαβιμιτω,  ος τοςτ (ιξγμυδιξη ξεημιηεξγε ος οτθεςχισε) αςισιξη ιξ αξω χαω
 * ουτ  οζ  τθε  υσε  οζ τθισ σοζτχαςε, εφεξ ιζ αδφισεδ οζ τθε ποσσιβιμιτω οζ
 * συγθ δαναηε.
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
