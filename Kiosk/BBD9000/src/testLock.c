#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

main() {
int fd;
int lock_PID=-1;
struct flock lock_s;
char tmpstr[256];
time_t t_now;


	fd=open("/var/ramfs/BBD9000netlock", O_RDONLY );
	
	while (1) {
		t_now = time(NULL);
		memset (&lock_s, 0, sizeof (lock_s));
		lock_s.l_type=F_WRLCK;
		lock_s.l_whence = SEEK_SET;
		fcntl(fd,F_GETLK,&lock_s);
		if (lock_s.l_type == F_UNLCK && lock_PID) {
			lock_PID = 0;
			strftime (tmpstr, 256, "%Y-%m-%d %H:%M:%S", localtime (&t_now));
			fprintf (stdout,"%s: UNLOCKED\n",tmpstr);
		} else if (lock_s.l_pid != lock_PID) {
			lock_PID = lock_s.l_pid;
			strftime (tmpstr, 256, "%Y-%m-%d %H:%M:%S", localtime (&t_now));
			fprintf (stdout,"%s: %d\n",tmpstr, lock_PID);
		}
		
		usleep (100000);
	}

}
