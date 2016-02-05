/*
 * pingit.h
 *
 *  Created on: Jan 18, 2016
 *      Author: luke
 */

#ifndef PINGIT_H_
#define PINGIT_H_

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DBGHDR  __FILE__, __LINE__, __FUNCTION__

class pingit {
public:
	pingit( uint32_t ip_addr );
	pingit( std::string ip_addr_str );
	virtual ~pingit();

	int init( bool periodic );
	int start();
	int stop();
	void display_arp_table( void );
	std::string get_mac_addr_str();
	void clear_arp_table( void );

private:
	struct in_addr *m_dest_ip;
	char m_mac_addr[ETH_ALEN];
	int m_sock;
	struct icmphdr *m_icmp_hdr;
	uint32_t m_icmp_seq_num;
	uint8_t m_raw_data[2048];
	struct sockaddr_in m_addr;
	bool m_periodic;

	int send_ping( void );
	int get_mac_addr( std::string host_str );
};

#endif /* PINGIT_H_ */
