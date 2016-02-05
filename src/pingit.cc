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

#include "pingit.h"


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


pingit::pingit( uint32_t ip_addr )
{
	// TODO Auto-generated constructor stub

	m_dest_ip = new struct in_addr();
	m_dest_ip->s_addr = ip_addr;

	printf("%s:%d: %s: ip_addr %s (0x%08x)\n",
			DBGHDR, inet_ntoa(*m_dest_ip), ip_addr);

	m_icmp_hdr = new struct icmphdr();
	memset(m_icmp_hdr, 0, sizeof(struct icmphdr));

	m_periodic = false;
	m_sock = -1;
	m_icmp_seq_num = 1;
}


pingit::pingit( std::string ip_addr )
{
	int retval = -1;


	m_dest_ip = new struct in_addr();

	retval = inet_aton( ip_addr.c_str(), m_dest_ip );
	if( retval < 0 ) {
		fprintf(stderr, "FAILED inet_ntoa: %s(%d)\n", strerror(errno), errno);
	}

	fprintf(stderr, "%s:%d: %s: new with %s (0x%08x)\n",
			DBGHDR, ip_addr.c_str(), ntohl(m_dest_ip->s_addr));

	m_icmp_hdr = new struct icmphdr();
	memset(m_icmp_hdr, 0, sizeof(struct icmphdr));

	m_periodic = false;
	m_sock = -1;
	m_icmp_seq_num = 1;
}

pingit::~pingit() {
	// TODO Auto-generated destructor stub

	close(m_sock);

	delete m_dest_ip;
	delete m_icmp_hdr;
}

int pingit::init( bool periodic )
{
	//unsigned char data[2048];
	//int rc;
	int retval = 0;

	clear_arp_table();

	m_periodic = periodic;

	m_sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_ICMP );
	if( m_sock < 0 ) {
		fprintf(stderr, "%s:%d: %s: FAILED to open socket (%d): %s(%d)\n",
				DBGHDR, m_sock, strerror(errno), errno);
		return (m_sock);
	}

	fprintf(stderr, "%s:%d: %s: opened socket %d\n",
			DBGHDR, m_sock);

	memset( &m_addr, 0, sizeof(struct sockaddr_in) );
	m_addr.sin_family = AF_INET;
	m_addr.sin_addr = *m_dest_ip;

	memset( m_icmp_hdr, 0, sizeof(struct icmphdr) );
	m_icmp_hdr->type = ICMP_ECHO;
	m_icmp_hdr->un.echo.id = 1234;//arbitrary id

	return (retval);
}


int pingit::start()
{
	int retval = -1;


	fprintf(stderr, "%s:%d: %s: pinging...\n", DBGHDR);

	retval = send_ping();

	get_mac_addr( inet_ntoa(*m_dest_ip) );

	return (retval);
}


int pingit::stop()
{
	int retval = -1;


	return (retval);

}


int pingit::send_ping( void )
{
	int retval = -1;
	std::string payload("hello");
	struct timeval timeout = {3, 0}; //wait max 3 seconds for a reply
	struct sockaddr_in src_addr;
	fd_set read_set;
	socklen_t slen;
	struct icmphdr rcv_hdr;
	struct timeval sent_time, recv_time, delta_time;

	m_icmp_hdr->un.echo.sequence = m_icmp_seq_num++;

	memset( &sent_time, 0, sizeof(struct timeval) );
	memset( &recv_time, 0, sizeof(struct timeval) );

	memcpy( m_raw_data, m_icmp_hdr, sizeof(struct icmphdr) );
	memcpy( m_raw_data + sizeof( struct icmphdr ), payload.c_str(), payload.length()); //icmp payload
	retval = sendto( m_sock, m_raw_data, sizeof(struct icmphdr) + payload.length(), 0,
			(struct sockaddr*)&m_addr, sizeof(struct sockaddr_in));
	if (retval <= 0) {
		fprintf(stderr, "%s:%d: %s: FAILED sendto to %s: %s(%d)\n",
				DBGHDR, inet_ntoa(*m_dest_ip), strerror(errno), errno);
		return (retval);
	}

	gettimeofday( &sent_time, NULL );

	fprintf(stderr, "Sent ICMP to %s\n", inet_ntoa(*m_dest_ip));

	for( int ii = 0; ii < 3; ii++ )
	{
		memset(&read_set, 0, sizeof read_set);
		FD_SET(m_sock, &read_set);

		//wait for a reply with a timeout
		retval = select(m_sock + 1, &read_set, NULL, NULL, &timeout);
		if( retval == 0 ) {
			fprintf(stderr, "TIMEDOUT waiting response from %s\n", inet_ntoa(*m_dest_ip));
			continue;
		} else if( retval < 0 ) {
			fprintf(stderr, "%s:%d: %s: FAILED select(%d): %s:%d\n",
				DBGHDR, retval, strerror(errno), errno);
			break;
		}

		//we don't care about the sender address in this example..
		slen = sizeof(struct sockaddr);

		retval = recvfrom( m_sock, m_raw_data, sizeof(m_raw_data), 0,
				(struct sockaddr *)&src_addr, &slen );
		if( retval <= 0 ) {
			fprintf(stderr, "%s:%d: %s: FAILED recvfrom(%d): %s:%d\n",
				DBGHDR, retval, strerror(errno), errno);
			break;
		}
		else if( retval < (int)sizeof(rcv_hdr) ) {
			fprintf(stderr, "%s:%d: %s: short ICMP pkt(%d bytes): %s:%d\n",
				DBGHDR, retval, strerror(errno), errno);
			break;
		}

		gettimeofday( &recv_time, NULL );
		timersub( &recv_time, &sent_time, &delta_time );

		memcpy( &rcv_hdr, m_raw_data, sizeof(rcv_hdr) );

		if( rcv_hdr.type == ICMP_ECHOREPLY ) {
			fprintf(stderr, "ICMP Reply (%d), src %s, id=0x%x, sequence =  0x%x (%ld.%06ld sec)\n",
					rcv_hdr.type, inet_ntoa(src_addr.sin_addr),
					rcv_hdr.un.echo.id, rcv_hdr.un.echo.sequence,
					delta_time.tv_sec, delta_time.tv_usec);
			break;
		}
		else {
			fprintf(stderr, "%s:%d: %s: Got ICMP packet with type 0x%x ?!?\n",
					DBGHDR, rcv_hdr.type);
		}
	}

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

	strcpy(req.arp_dev, "wlan0");

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
