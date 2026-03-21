#ifndef  _NVILIDAR_SOCKET_TCP_H
#define _NVILIDAR_SOCKET_TCP_H

#if defined(__linux__)
	#include "socket/nvilidar_socket_tcp_unix.h"
	#include "socket/nvilidar_socket_udp_unix.h"
#elif defined(_WIN32)
	#include "socket/nvilidar_socket_tcp_win.h"
	#include "socket/nvilidar_socket_udp_win.h"
#endif


#endif


