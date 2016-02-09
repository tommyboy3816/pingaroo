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

#define RETRY_TMR_SEC       (4)
#define RETRY_TMR_USEC (000000)

class pingit {
public:
	pingit( std::string dev, std::string ip_addr_str );
	virtual ~pingit();

	bool init( bool periodic, struct timeval *retry );
	int start();
	void stop();
	bool set_retry( struct timeval *new_retry );
	void display_arp_table( void );
	std::string get_mac_addr_str();
	void clear_arp_table( void );

private:
	std::string m_dev;
	struct in_addr *m_dest_ip;
	bool m_initialized;

	char m_mac_addr[ETH_ALEN];
	uint32_t m_icmp_seq_num;
	uint8_t m_raw_data[2048];
	struct sockaddr_in m_addr;

	bool m_periodic;
	struct timeval m_retry_tmr;

	pthread_mutex_t m_retry_tmr_lock;
	pthread_mutex_t m_exit_lock;

	bool m_exit_request;


	bool send_ping( void );
	int get_mac_addr( std::string host_str );
	void get_retry( struct timeval *a );
};

#endif /* PINGIT_H_ */
