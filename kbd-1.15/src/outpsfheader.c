/* outpsfheader - auxiliary fn - not to be installed */
/* assumes a little-endian machine */
#include <stdio.h>
#include <stdlib.h>	/* exit */
#include "psf.h"

static void
usage(void) {
	fprintf(stderr, "call: outpsfheader psftype fontsize charsize hastable\n");
	exit(1);
}

int
main(int argc, char **argv){
	int psftype, fontsize, charsize, hastable;

	if (argc != 5)
		usage();
	psftype = atoi(argv[1]);
	fontsize = atoi(argv[2]);
	charsize = atoi(argv[3]);
	hastable = atoi(argv[4]);

	if (psftype == 1) {
		struct psf1_header h1;

		if (fontsize != 256 && fontsize != 512)
			usage();
		h1.magic[0] = PSF1_MAGIC0;
		h1.magic[1] = PSF1_MAGIC1;
		h1.mode = (fontsize == 256) ? 0 : PSF1_MODE512;
		if (hastable)
			h1.mode |= PSF1_MODEHASTAB;
		h1.charsize = charsize;
		if (fwrite(&h1, sizeof(h1), 1, stdout) != 1) {
			fprintf(stderr, "write error\n");
			exit(1);
		}
	} else if (psftype == 2) {
		struct psf2_header h2;

		h2.magic[0] = PSF2_MAGIC0;
		h2.magic[1] = PSF2_MAGIC1;
		h2.magic[2] = PSF2_MAGIC2;
		h2.magic[3] = PSF2_MAGIC3;
		h2.version = 0;
		h2.headersize = sizeof(h2);
		h2.flags = (hastable ? PSF2_HAS_UNICODE_TABLE : 0);
		h2.length = fontsize;
		h2.charsize = charsize;
		h2.width = 8;
		h2.height = charsize;
		if (fwrite(&h2, sizeof(h2), 1, stdout) != 1) {
			fprintf(stderr, "write error\n");
			exit(1);
		}
	} else
		usage();
	return 0;
}
