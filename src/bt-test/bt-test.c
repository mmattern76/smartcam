#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>


/* Unofficial value, might still change */
#define LE_LINK		0x03

#define for_each_opt(opt, long, short) while ((opt=getopt_long(argc, argv, short ? short:"+", long, NULL)) != -1)


/* Inquiry */

static struct option inq_options[] = {
	{ "help",	0, 0, 'h' },
	{ "length",	1, 0, 'l' },
	{ "numrsp",	1, 0, 'n' },
	{ "iac",	1, 0, 'i' },
	{ "flush",	0, 0, 'f' },
	{ 0, 0, 0, 0 }
};

static const char *inq_help =
	"Usage:\n"
	"\tinq [--length=N] maximum inquiry duration in 1.28 s units\n"
	"\t    [--numrsp=N] specify maximum number of inquiry responses\n"
	"\t    [--iac=lap]  specify the inquiry access code\n"
	"\t    [--flush]    flush the inquiry cache\n";

int main(int argc, char **argv)
{

	int dev_id = 0;
	inquiry_info *info = NULL;
	uint8_t lap[3] = { 0x33, 0x8b, 0x9e };
	int num_rsp, length, flags;
	char addr[18];
	int i, l, opt;

	length  = 8;	/* ~10 seconds */
	num_rsp = 0;
	flags   = 0;

	for_each_opt(opt, inq_options, NULL) {
		switch (opt) {
		case 'l':
			length = atoi(optarg);
			break;

		case 'n':
			num_rsp = atoi(optarg);
			break;

		case 'i':
			l = strtoul(optarg, 0, 16);
			if (!strcasecmp(optarg, "giac")) {
				l = 0x9e8b33;
			} else if (!strcasecmp(optarg, "liac")) {
				l = 0x9e8b00;
			} if (l < 0x9e8b00 || l > 0x9e8b3f) {
				printf("Invalid access code 0x%x\n", l);
				exit(1);
			}
			lap[0] = (l & 0xff);
			lap[1] = (l >> 8) & 0xff;
			lap[2] = (l >> 16) & 0xff;
			break;

		case 'f':
			flags |= IREQ_CACHE_FLUSH;
			break;

		default:
			printf("%s", inq_help);
			return 1;
		}
	}

	printf("Inquiring ...\n");

	num_rsp = hci_inquiry(dev_id, length, num_rsp, lap, &info, flags);
	if (num_rsp < 0) {
		perror("Inquiry failed.");
		exit(1);
	}

	for (i = 0; i < num_rsp; i++) {
		ba2str(&(info+i)->bdaddr, addr);
		printf("\t%s\tclock offset: 0x%4.4x\tclass: 0x%2.2x%2.2x%2.2x\n",
			addr, btohs((info+i)->clock_offset),
			(info+i)->dev_class[2],
			(info+i)->dev_class[1],
			(info+i)->dev_class[0]);
	}

	bt_free(info);
	
	return 1;
}

