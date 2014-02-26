#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

void encrypt_SPN (char *crypt_str, const char *plain_str) {
int dev_rand_fp;
int i;
unsigned long seed[2];
char salt[] = "$1$........";
const char *const seedchars =
 "./0123456789ABCDEFGHIJKLMNOPQRST"
 "UVWXYZabcdefghijklmnopqrstuvwxyz";
	
	// Get two longs worth of noise
	dev_rand_fp = open ("/dev/urandom", O_RDONLY);
	if (dev_rand_fp > -1 ) {
		read (dev_rand_fp, seed, 8);
		close (dev_rand_fp);
	} else {
		seed[0] = time(NULL);
		seed[1] = getpid() ^ (seed[0] >> 14 & 0x30000);
	}
	
	/* Turn it into printable characters from `seedchars'. */
	for (i = 0; i < 8; i++)
		salt[3+i] = seedchars[(seed[i/5] >> (i%5)*6) & 0x3f];

	strcpy (crypt_str, crypt(plain_str, salt));	
}

int main (int argc,char **argv) {
	char out_str[1024];

	if (argc != 2) {
		fprintf (stderr,"Encypt an input string using crypt() with random salt.\n");
		fprintf (stderr,"Can be used as an encrypted SPN entry (e.g. operator_code in BBD9000-cal.conf.\n");
		fprintf (stderr,"Usage:\n");
		fprintf (stderr,"  %s plaintext\n",argv[0]);
		exit (-1);
	}
	if (strlen(argv[1]) > 255) {
		fprintf (stderr,"Input string is way too long\n");
		exit (-1);
	}
	
	encrypt_SPN (out_str, argv[1]);
//  A hash for 123456.  Does not seem to work on Mac OS
//	strcpy (out_str,"$1$vVxA4ca9$en0kUF4oGO5KiV0TcPF7f1");
	if (! strcmp (crypt(argv[1], out_str), out_str) ) {
		printf ("%s\n",out_str);
		exit (0);
	} else {
		printf ("Can't confirm match with crypt()!!\n");
		exit (-1);
	}	

	
}
