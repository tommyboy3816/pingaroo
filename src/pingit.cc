/*
 * pingit.cpp
 *
 *  Created on: Jan 18, 2016
 *      Author: luke
 */
#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <linux/if_ether.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <sys/time.h>
#include <pthread.h>

#include "pingit.h"


/**
 * command = /usr/bin/arping -f -w 5 -I eth0 -c 4 10.8.129.254
 * ----------------------------------
 * WARNING: interface is ignored: Operation not permitted
 *   0: ARPING 10.8.129.254 from 10.8.128.65 eth0
 *   1: Sent 4 probes (4 broadcast(s))
 *   2: Received 0 response(s)
 * ----------------------------------
 *
 * ----------------------------------
 * WARNING: interface is ignored: Operation not permitted
 *   0: ARPING 10.8.129.53 from 10.8.128.65 eth0
 *   1: Unicast reply from 10.8.129.53 [88:51:FB:56:CE:6B]  0.870ms
 *   2: Sent 1 probes (1 broadcast(s))
 *   3: Received 1 response(s)
 * ----------------------------------
 *
 */
/**
 *
 */
std::string mac_ntoa( char *p )
{
	std::string mac_addr_str;
	static char buffer[30];


	snprintf(buffer, 30, "%02X:%02X:%02X:%02X:%02X:%02X",
			0xFF & p[0], 0xFF & p[1], 0xFF & p[2],
			0xFF & p[3], 0xFF & p[4], 0xFF & p[5]);

	mac_addr_str = buffer;

	return (mac_addr_str);
}


pingit::pingit( std::string dev, std::string ip_addr )
{
	int retval = -1;


	m_dev = dev;
	m_dest_ip = new struct in_addr();

	retval = inet_aton( ip_addr.c_str(), m_dest_ip );
	if( retval < 0 ) {
		fprintf(stderr, "FAILED inet_ntoa: %s(%d)\n", strerror(errno), errno);
	}

	fprintf(stderr, "%s:%d: %s: new with device %s, IP %s (0x%08x)\n",
			DBGHDR, m_dev.c_str(), ip_addr.c_str(), ntohl(m_dest_ip->s_addr));

	m_initialized = false;
	m_periodic = false;
	m_exit_request = false;

	m_icmp_seq_num = 1;
}

pingit::~pingit() {
	// TODO Auto-generated destructor stub

	delete m_dest_ip;

	pthread_mutex_destroy( &m_retry_tmr_lock );
	pthread_mutex_destroy( &m_exit_lock );
}

bool pingit::init( bool periodic, struct timeval *retry )
{
	bool retval = false;
	struct timeval tmp;


	if( pthread_mutex_init( &m_retry_tmr_lock, NULL ) != 0 )
	{
		printf("\n mutex init failed\n");
		return (false);
	}

	if( pthread_mutex_init( &m_exit_lock, NULL ) != 0 )
	{
		printf("\nm_exit_lock failed\n");
		return (false);
	}

	clear_arp_table();

	m_periodic = periodic;

	if( periodic == true ) {
		if( retry == NULL ) {
		/** use default time */
		tmp.tv_sec = RETRY_TMR_SEC;
		tmp.tv_usec = RETRY_TMR_USEC;
		}
		else {
			tmp.tv_sec = retry->tv_sec;
			tmp.tv_usec = retry->tv_usec;
		}
		set_retry( &tmp );
	}
	else {
		timerclear( &m_retry_tmr );
	}

	printf("%s:%d: %s: retries %s, every %ld.%03ld seconds\n",
			DBGHDR, (periodic)?"true":"false",
			m_retry_tmr.tv_sec, m_retry_tmr.tv_usec);

	memset( &m_addr, 0, sizeof(struct sockaddr_in) );
	m_addr.sin_family = AF_INET;
	m_addr.sin_addr = *m_dest_ip;

	m_initialized = true;

	return (retval);
}


/**
 * @fn bool pingit::set_retry( struct timeval *new_retry )
 * @brief
 *
 * Make sure this is thread safe, timer could be changed on the fly
 */
bool pingit::set_retry( struct timeval *new_retry )
{
	pthread_mutex_lock( &m_retry_tmr_lock);

	m_retry_tmr.tv_sec = new_retry->tv_sec;
	m_retry_tmr.tv_usec = new_retry->tv_usec;

	pthread_mutex_unlock( &m_retry_tmr_lock);
}


void pingit::get_retry( struct timeval *tmr )
{
	pthread_mutex_lock( &m_retry_tmr_lock);

	tmr->tv_sec = m_retry_tmr.tv_sec;
	tmr->tv_usec = m_retry_tmr.tv_usec;

	pthread_mutex_unlock( &m_retry_tmr_lock);
}


int pingit::start()
{
	int retval = -1;
	uint32_t ii = 0;
	fd_set rfds;
	struct timeval sleep;


	/* Watch stdin (fd 0) to see when it has input. */
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);

	get_retry( &sleep );


	while( !m_exit_request )
	{
		retval = select( 1, &rfds, NULL, NULL, &sleep );
		if( retval == -1 ) {
			perror ("select");
			return (1);
		}
		else if (m_exit_request) {
			fprintf(stderr, "%s:%d: %s: EXITING...\n", DBGHDR);
			break;
		}
		else {
			printf("%d...m_exit_request %d\n", ii++, m_exit_request);
			retval = send_ping();
			get_mac_addr( inet_ntoa(*m_dest_ip) );
		}

		/** Check to see if the timer changed */
		get_retry( &sleep );
	}


	printf("%s:%d: %s: m_exit_request %u\n", DBGHDR, m_exit_request);

	return (retval);
}


void pingit::stop()
{
	pthread_mutex_lock( &m_exit_lock);

	m_exit_request = true;
	printf("%s:%d: %s: m_exit_request %u\n", DBGHDR, m_exit_request);

	pthread_mutex_unlock( &m_exit_lock);
}


bool pingit::send_ping( void )
{
	bool retval = false;
	static char tmp[256];
	std::string cmd;
	FILE *fp = NULL;
	uint32_t ii = 0;

	memset( tmp, 256, 0 );

	snprintf(tmp, 256, "/usr/bin/arping -f -w 5 -I %s -c 4 %s",
			m_dev.c_str(), inet_ntoa(*m_dest_ip));

	cmd = tmp;

	printf("command = %s\n", cmd.c_str());
	printf("----------------------------------\n");

	/* Open the command for reading. */
	fp = popen(cmd.c_str(), "r");
	if (fp == NULL) {
		printf("Failed to run command\n" );
		exit(1);
	}

	/* Read the output a line at a time - output it. */
	while (fgets(tmp, sizeof(tmp)-1, fp) != NULL) {
		printf("%3d: %s", ii, tmp);
		ii++;
	}

	printf("----------------------------------\n");

	/* close */
	pclose(fp);

	return (retval);
}


/**
 * Get the MAC address of a given IPv4 Address provided its in the ARP table.
 *  Needs to have a ping or some other data packet go to the network device
 *  first.
 */
int pingit::get_mac_addr( std::string host_str )
{
	struct arpreq req;
	struct hostent *hp;
	struct sockaddr_in *sin;
	int retval = -1, tmp_socket = -1;


	bzero( (caddr_t)&req, sizeof(struct arpreq) );

	sin = (struct sockaddr_in *)&req.arp_pa;
	sin->sin_family = AF_INET; /* Address Family: Internet */
	sin->sin_addr.s_addr = inet_addr(host_str.c_str());

	if(sin->sin_addr.s_addr == 0xFFFFFFFF)
	{
		if(!(hp = gethostbyname(host_str.c_str()))){
			fprintf(stderr, "arp: %s ", host_str.c_str());
			herror((char *)NULL);
			return(-1);
		}

		bcopy((char *)hp->h_addr, (char *)&sin->sin_addr, sizeof(sin->sin_addr));
	}

	tmp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if( tmp_socket < 0 ) {
		perror("socket() failed.");
		fprintf(stderr, "%s:%d: %s: FAILED to open socket(%d) %s(%d)\n",
				DBGHDR, tmp_socket, strerror(errno), errno);
		return (-1);
	} /* Socket is opened.*/

	//strcpy(req.arp_dev, "wlan0");
	strcpy(req.arp_dev, m_dev.c_str());

	if( ioctl(tmp_socket, SIOCGARP, (caddr_t)&req) < 0 ) {
		if( errno == ENXIO ) {
			fprintf(stderr, "%s:%d: %s: %s (%s) -- no entry.\n",
					DBGHDR, host_str.c_str(), inet_ntoa(sin->sin_addr));
			return( -2 );
		}
		else {
			fprintf(stderr, "%s:%d: %s: SIOCGARP %s(%d)\n",
					DBGHDR, strerror(errno), errno);
			return (-3);
		}
	}

	close(tmp_socket); /* Close the socket, we don't need it anymore. */

	fprintf(stderr, "%s (%s) at ", host_str.c_str(), inet_ntoa(sin->sin_addr));

	if( req.arp_flags & ATF_COM ) {
		memcpy( m_mac_addr, req.arp_ha.sa_data, ETH_ALEN );
		fprintf(stderr, "%s (%s), family %d\n\n",
				mac_ntoa(req.arp_ha.sa_data).c_str(), req.arp_dev,
				req.arp_ha.sa_family);
		return (0);
	}
	else {
		printf("incomplete");
		retval = -1;
	}

	if( req.arp_flags & ATF_PERM ) {
		printf("ATF_PERM");
	}

	if( req.arp_flags & ATF_PUBL ) {
		printf("ATF_PUBL");
	}

	if( req.arp_flags & ATF_USETRAILERS ) {
		printf("ATF_USETRAILERS");
	}

	printf("\n");

	return (retval);
}


std::string pingit::get_mac_addr_str( void )
{
	return mac_ntoa(m_mac_addr);
}

void pingit::clear_arp_table( void )
{
	fprintf(stderr, "%s:%d: %s: CLEARING ARP TABLE...\n", DBGHDR);
	fprintf(stderr, "------------------------------------------------------------\n");

	/** clear out the ARP cache first */
	system("ip -s -s neigh flush all");

	fprintf(stderr, "------------------------------------------------------------\n");
}


void pingit::display_arp_table( void )
{

}
