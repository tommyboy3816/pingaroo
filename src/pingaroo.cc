//============================================================================
// Name        : pingaroo.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/if_ether.h>
#include <ctype.h>
#include <unistd.h>

#include "pingit.h"

using namespace std;

std::string g_devname;
std::string g_destip;


bool get_cmd_line_options( int argc, char **argv )
{
	int c;
	bool devname_b = false, destip_b = false;


	while ((c = getopt (argc, argv, "d:i:")) != -1)
	{
		//printf("%02d   c = %c, optarg = %s\n", ii, c, optarg);
		switch (c)
		{
		case 'd':
			g_devname = optarg;
			devname_b = true;
			break;
		case 'i':
			g_destip = optarg;
			destip_b = true;
			break;
		case '?':
			if (optopt == 'c')
				fprintf (stderr, "Option -%c requires an argument.\n", optopt);
			else if (isprint (optopt))
				fprintf (stderr, "Unknown option `-%c'.\n", optopt);
			else
				fprintf (stderr,
						"Unknown option character `\\x%x'.\n",
						optopt);
			return (false);
		default:
			abort ();
		}
	}

	//printf ("devname_b = %d, devname %s, destip_b %d, destip = %s\n",
	//		devname_b, g_devname.c_str(), destip_b, g_destip.c_str());

	if( !(devname_b) || !(destip_b) ) {
		return (false);
	}
	else {
		return (true);
	}
}

int main( int argc, char **argv ) {
	pingit *ne01;
	std::string hostname;
	bool retval_b = false;
	FILE *fp;
	char path[1035];
	char tmp[80];
	std::string cmd;


	retval_b = get_cmd_line_options( argc, argv );
	if( !retval_b ) {
		fprintf(stderr, "bad command line args\n\n");
		return (-1);
	}


	printf("device %s, dest IP %s\n", g_devname.c_str(), g_destip.c_str());

	snprintf(tmp, 80, "/usr/bin/arping -I %s -c 4 %s",
			g_devname.c_str(), g_destip.c_str());

	cmd = tmp;

	printf("command = %s\n", cmd.c_str());

	/* Open the command for reading. */
	fp = popen(cmd.c_str(), "r");
	if (fp == NULL) {
		printf("Failed to run command\n" );
		exit(1);
	}

	/* Read the output a line at a time - output it. */
	while (fgets(path, sizeof(path)-1, fp) != NULL) {
		printf("%s", path);
	}

	/* close */
	pclose(fp);

  return 0;



	printf("argc %d, argv %s\n", argc, argv[argc-1]);

	if( argc == 2 ) {
		hostname = argv[argc-1];
	}
	else {
		hostname = "192.168.1.100";
	}

	ne01 = new pingit( hostname );

	if( -1 == ne01->init(true) ) {
		return (-1);
	}

	ne01->start();

	cout << ne01->get_mac_addr_str() << "\n\n";

	delete ne01;

	return 0;
}
