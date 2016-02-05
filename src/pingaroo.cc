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
#include <signal.h>

#include "pingit.h"

using namespace std;

std::string g_devname;
std::string g_destip;

volatile bool g_kill_switch = false;

void signal_sigint_handler( int signal ) {
	g_kill_switch = true;
	printf("caught signal %u\n", signal);

}


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
	bool retval_b = false;
	uint64_t ctr = 0;


	retval_b = get_cmd_line_options( argc, argv );
	if( !retval_b ) {
		fprintf(stderr, "bad command line args\n\n");
		return (-1);
	}

	signal( SIGINT, signal_sigint_handler );

	ne01 = new pingit( g_devname, g_destip );

	ne01->init( true, NULL );
	ne01->start();

	while( g_kill_switch == false )
	{
		usleep(100);
		ctr++;
	}

	printf("stopping...%lu, g_kill_switch %u\n", ctr, g_kill_switch);

	ne01->stop();

	cout << ne01->get_mac_addr_str() << "\n\n";

	delete ne01;

	return (0);
}
