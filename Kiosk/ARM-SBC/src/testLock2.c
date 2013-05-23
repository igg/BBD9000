#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/file.h> 


main() {
int fdlock;
int lock_PID=-1;
struct flock lock_s;
char tmpstr[256];
time_t t_now;


	fdlock=open("/var/ramfs/BBD9000netlock", O_RDWR );
	
//	while (1) {
		t_now = time(NULL);

		// wait for a write lock
		if(flock(fdlock, LOCK_EX) == -1) {
			close (fdlock);
			fprintf (stderr,"no lock\n");
			return (-3);
		}
		fprintf (stderr,"got lock\n");

		sleep (1000);
		
//	}

}
