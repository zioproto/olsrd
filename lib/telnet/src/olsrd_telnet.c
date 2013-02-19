/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org)
 *                     includes code by Bruno Randolf
 *                     includes code by Andreas Lopatic
 *                     includes code by Sven-Ola Tuecke
 *                     includes code by Lorenz Schori
 *                     includes bugs by Markus Kittenberger
 *                     includes bugs by Hans-Christoph Steiner
 *                     includes bugs by Christian Pointner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

/*
 * Dynamic linked library for the olsr.org olsr daemon
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#ifdef __linux__
#include <fcntl.h>
#endif /* __linux__ */

#include "olsr.h"
#include "olsr_types.h"
#include "scheduler.h"

#include "olsrd_telnet.h"
#include "telnet_client.h"

#include "cmd_handler.h"
#include "cmd_hna.h"
#include "cmd_interface.h"
#include "cmd_terminate.h"


#ifdef _WIN32
#define close(x) closesocket(x)
#endif /* _WIN32 */



#define STR_CONCAT3(x, y, z) x ## y ## z
#define CHECK_ENABLE_COMMAND(COMMAND, CMD)                                     \
  do {                                                                         \
    if(!strcmp(COMMAND, STR_CONCAT3(cmd_, CMD, _get_command()) )) {            \
      if(! STR_CONCAT3(cmd_, CMD, _init()) )                                   \
        olsr_printf(1, "(TELNET) failed: enabling command '%s'\n", COMMAND);   \
      else                                                                     \
        olsr_printf(2, "(TELNET) command '%s' enabled\n", COMMAND);            \
    }                                                                          \
  } while(false)

static void enable_command(const char* command)
{
  CHECK_ENABLE_COMMAND(command, terminate);
  CHECK_ENABLE_COMMAND(command, interface);
  CHECK_ENABLE_COMMAND(command, hna);
}

static void telnet_enable_commands(void)
{
  struct string_list* s;

  if(!telnet_enabled_commands) {
    enable_command(cmd_terminate_get_command());
    enable_command(cmd_interface_get_command());
    enable_command(cmd_hna_get_command());
    return;
  }

  for(s = telnet_enabled_commands; s; s = s->next)
    enable_command(s->string);
}


static int telnet_socket;
static void telnet_action(int, void *, unsigned int);

int
olsrd_telnet_init(void)
{
  union olsr_sockaddr sst;
  uint32_t yes = 1;
  socklen_t addrlen;
  telnet_socket = -1;

  /* Init telnet socket */
  if ((telnet_socket = socket(olsr_cnf->ip_version, SOCK_STREAM, 0)) == -1) {
#ifndef NODEBUG
    olsr_printf(1, "(TELNET) socket()=%s\n", strerror(errno));
#endif /* NODEBUG */
    return 0;
  }

  if (setsockopt(telnet_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0) {
#ifndef NODEBUG
    olsr_printf(1, "(TELNET) setsockopt()=%s\n", strerror(errno));
#endif /* NODEBUG */
    return 0;
  }
#if (defined __FreeBSD__ || defined __FreeBSD_kernel__) && defined SO_NOSIGPIPE
  if (setsockopt(telnet_socket, SOL_SOCKET, SO_NOSIGPIPE, (char *)&yes, sizeof(yes)) < 0) {
    perror("SO_REUSEADDR failed");
    return 0;
  }
#endif /* (defined __FreeBSD__ || defined __FreeBSD_kernel__) && defined SO_NOSIGPIPE */
      /* Bind the socket */

      /* complete the socket structure */
  memset(&sst, 0, sizeof(sst));
  if (olsr_cnf->ip_version == AF_INET) {
    sst.in4.sin_family = AF_INET;
    addrlen = sizeof(struct sockaddr_in);
#ifdef SIN6_LEN
    sst.in4.sin_len = addrlen;
#endif /* SIN6_LEN */
    sst.in4.sin_addr.s_addr = telnet_listen_ip.v4.s_addr;
    sst.in4.sin_port = htons(telnet_port);
  } else {
    sst.in6.sin6_family = AF_INET6;
    addrlen = sizeof(struct sockaddr_in6);
#ifdef SIN6_LEN
    sst.in6.sin6_len = addrlen;
#endif /* SIN6_LEN */
    sst.in6.sin6_addr = telnet_listen_ip.v6;
    sst.in6.sin6_port = htons(telnet_port);
  }

      /* bind the socket to the port number */
  if (bind(telnet_socket, &sst.in, addrlen) == -1) {
#ifndef NODEBUG
    olsr_printf(1, "(TELNET) bind()=%s\n", strerror(errno));
#endif /* NODEBUG */
    return 0;
  }

      /* show that we are willing to listen */
  if (listen(telnet_socket, 1) == -1) {
#ifndef NODEBUG
    olsr_printf(1, "(TELNET) listen()=%s\n", strerror(errno));
#endif /* NODEBUG */
    return 0;
  }

      /* Register with olsrd */
  add_olsr_socket(telnet_socket, &telnet_action, NULL, NULL, SP_PR_READ);

#ifndef NODEBUG
  olsr_printf(2, "(TELNET) listening on port %d\n", telnet_port);
#endif /* NODEBUG */

#ifdef TELNET_FOREIGN_CMDS
  if(telnet_allow_foreign) {
    olsr_cnf->telnet_foreign_cmds.client_quit = &telnet_client_quit;
    olsr_cnf->telnet_foreign_cmds.client_printf = &telnet_client_printf;
  }
#endif /* TELNET_FOREIGN_CMDS */
  telnet_enable_commands();
  return telnet_client_init();
}

void
olsrd_telnet_exit(void)
{
  telnet_client_cleanup();

  if (telnet_socket != -1) {
    remove_olsr_socket(telnet_socket, &telnet_action, NULL);
    close(telnet_socket);
  }
}


static void
telnet_action(int fd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused)))
{
  union olsr_sockaddr pin;

  char addr[INET6_ADDRSTRLEN];
  int client_fd, c;

  socklen_t addrlen = sizeof(pin);

  if ((client_fd = accept(fd, &pin.in, &addrlen)) == -1) {
#ifndef NODEBUG
    olsr_printf(1, "(TELNET) accept()=%s\n", strerror(errno));
#endif /* NODEBUG */
    return;
  }

  if (olsr_cnf->ip_version == AF_INET) {
    if (inet_ntop(olsr_cnf->ip_version, &pin.in4.sin_addr, addr, INET6_ADDRSTRLEN) == NULL)
      addr[0] = '\0';
  } else {
    if (inet_ntop(olsr_cnf->ip_version, &pin.in6.sin6_addr, addr, INET6_ADDRSTRLEN) == NULL)
      addr[0] = '\0';
  }

  c = telnet_client_add(client_fd);
  if(c >= 0) {
#ifndef NODEBUG
      olsr_printf(2, "(TELNET) Connect from %s (client: %d)\n", addr, c);
#endif /* NODEBUG */
  } else {
    close(client_fd);
#ifndef NODEBUG
    olsr_printf(1, "(TELNET) Connect from %s (maximum number of clients reached!)\n", addr);
#endif /* NODEBUG */
  }
}
