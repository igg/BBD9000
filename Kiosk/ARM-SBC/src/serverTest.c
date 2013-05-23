#include <stdio.h>
#include <assert.h>
#define BBD9000srv        "/var/ramfs/BBD9000srv"
FILE *srv_fp;


int main () {
	/* Open the server FIFO */
	srv_fp = fopen (BBD9000srv, "w");
	assert(srv_fp != NULL);
	
	fprintf (srv_fp,"new_memb\t new member bla bla bla single\n");
	fflush (srv_fp);
	sleep (1);

	fprintf (srv_fp,"start\t a idle message here\n");
	fprintf (srv_fp,"foo\t some foo bla bla bla\n");
	fprintf (srv_fp,"status\t a status message\n");
	fprintf (srv_fp,"foo\t between status and auth\n");
	fprintf (srv_fp,"auth\t an auth message\n");
	fprintf (srv_fp,"new_memb\t new member bla bla bla - flushing\n");
	fflush (srv_fp);
	sleep (1);

	fprintf (srv_fp,"status\t a status message first\n");
	fprintf (srv_fp,"auth\t an auth message second\n");
	fprintf (srv_fp,"foo\t some foo bla bla bla\n");
	fprintf (srv_fp,"new_memb\t new member bla bla bla - flushing\n");
	fprintf (srv_fp,"idle\t a idle message here\n");
	fflush (srv_fp);
	sleep (1);

	fprintf (srv_fp,"foo\t some foo bla bla bla\n");
	fprintf (srv_fp,"new_memb\t new member bla bla bla\n");
	fprintf (srv_fp,"status\t a status message last\n");
	fprintf (srv_fp,"start\t a start message here\n");
	fprintf (srv_fp,"auth\t an auth message lastest\n");
	fflush (srv_fp);
	sleep (1);

	fprintf (srv_fp,"new_memb\t new member bla bla bla\n");
	fprintf (srv_fp,"idle\t a idle message here\n");
	fprintf (srv_fp,"status\t a status message first - no NL");
	fflush (srv_fp);
	sleep (1);
	fprintf (srv_fp,"\nauth\t an auth message second starting with NL\n");
	fprintf (srv_fp,"new_memb\t new member bla bla bla\n");
	fclose (srv_fp);
}
