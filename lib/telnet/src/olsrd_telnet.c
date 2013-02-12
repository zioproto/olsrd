/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org)
 *                     includes code by Bruno Randolf
 *                     includes code by Andreas Lopatic
 *                     includes code by Sven-Ola Tuecke
 *                     includes code by Lorenz Schori
 *                     includes bugs by Markus Kittenberger
 *                     includes bugs by Hans-Christoph Steiner
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
#ifndef _WIN32
#include <sys/select.h>
#endif /* _WIN32 */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <libgen.h>

#ifdef __linux__
#include <fcntl.h>
#endif /* __linux__ */

#include "olsr.h"
#include "olsr_types.h"
#include "scheduler.h"

#include "olsrd_telnet.h"
#include "olsrd_plugin.h"

#ifdef _WIN32
#define close(x) closesocket(x)
#endif /* _WIN32 */

static int telnet_socket;

/* TELNET initialization function */
static int plugin_telnet_init(void);

static void telnet_action(int, void *, unsigned int);

static int telnet_client_add(int);
static void telnet_client_remove(int);
static int telnet_client_find(int);
static void telnet_client_read(int, void *, unsigned int);
static void telnet_client_write(int, void *, unsigned int);

#define MAX_CLIENTS 3
#define BUF_SIZE 1024

typedef struct {
  char buf[BUF_SIZE];
  size_t len;
  size_t written;
} telnet_buffer_t;
  
typedef struct {
  int fd;
  telnet_buffer_t out;
  telnet_buffer_t in;
} client_t;

static client_t clients[MAX_CLIENTS];


/**
 *Do initialization here
 *
 *This function is called by the my_init
 *function in olsrd_plugin.c
 */
int
olsrd_plugin_init(void)
{
  telnet_socket = -1;
  plugin_telnet_init();
  return 1;
}

/**
 * destructor - called at unload
 */
void
olsr_plugin_exit(void)
{
  if (telnet_socket != -1)
    close(telnet_socket);
}

static int
plugin_telnet_init(void)
{
  union olsr_sockaddr sst;
  uint32_t yes = 1, i;
  socklen_t addrlen;

  /* Init telnet socket */
  if ((telnet_socket = socket(olsr_cnf->ip_version, SOCK_STREAM, 0)) == -1) {
#ifndef NODEBUG
    olsr_printf(1, "(TELNET) socket()=%s\n", strerror(errno));
#endif /* NODEBUG */
    return 0;
  } else {
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

    for(i=0; i<MAX_CLIENTS; ++i)
      clients[i].fd = -1;
  }
  return 1;
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
  if(c < MAX_CLIENTS) {
#ifndef NODEBUG
      olsr_printf(0, "(TELNET) Connect from %s (client: %d)\n", addr, c);
#endif /* NODEBUG */
  } else {
    close(client_fd);
#ifndef NODEBUG
    olsr_printf(0, "(TELNET) Connect from %s (maximum number of clients reached!)\n", addr);
#endif /* NODEBUG */    
  }
}

static int telnet_client_add(int fd)
{
  int c;
  for(c=0; c < MAX_CLIENTS; c++) {
    if(clients[c].fd == -1) {
      clients[c].fd = fd;
      clients[c].out.len = 0;
      clients[c].out.written = 0;
      clients[c].in.len = 0;
      clients[c].in.written = 0;
      add_olsr_socket(fd, &telnet_client_read, NULL, NULL, SP_PR_READ);
      break;
    }
  }
  return c;
}

static void telnet_client_remove(int c)
{
  remove_olsr_socket(clients[c].fd, &telnet_client_read, NULL);
  remove_olsr_socket(clients[c].fd, &telnet_client_write, NULL);
  close(clients[c].fd);
  clients[c].fd = -1;
}

static int telnet_client_find(int fd)
{
  int c;
  for(c=0; c<MAX_CLIENTS; c++) {
    if(clients[c].fd == fd) 
      break;
  }
  return c;
}

static void
telnet_client_read(int fd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused)))
{
  int c;
  size_t  old_len, i;
  ssize_t result;

  c = telnet_client_find(fd);
  if(c == MAX_CLIENTS) {        // unknown client...???
    remove_olsr_socket(fd, &telnet_client_read, NULL);
    close(fd);
    return;
  }

  result = recv(fd, (void *)&(clients[c].in.buf[clients[c].in.len]), sizeof(clients[c].in.buf) - clients[c].in.len, 0);   /* Win32 needs the cast here */
  if (result > 0) {
    old_len = clients[c].in.len;
    clients[c].in.len += result;
    
    for(i = old_len; i < clients[c].in.len; ++i) {
      if(clients[c].in.buf[i] == '\n' || clients[c].in.buf[i] == '\r') {
        if(((i+1) < clients[c].in.len) && (clients[c].in.buf[i+1] == '\n' || clients[c].in.buf[i+1] == '\r')) {
          clients[c].in.buf[i] = 0;
          i++;
        }
        clients[c].in.buf[i] = 0;
        
#ifndef NODEBUG
        olsr_printf(0, "(TELNET) client %i: got %i bytes '%s'\n", c, i, clients[c].in.buf);
#endif /* NODEBUG */    

            // TODO: parse the request
        if(!strcmp(clients[c].in.buf, "quit") && strlen(clients[c].in.buf) == 4) {
          telnet_client_remove(c);
        } else {
          clients[c].out.len = snprintf(clients[c].out.buf, sizeof(clients[c].out.buf), "%s\n\r", clients[c].in.buf);
          clients[c].out.written = 0;
          add_olsr_socket(fd, &telnet_client_write, NULL, NULL, SP_PR_WRITE);
        }

        if((i+1) < clients[c].in.len) {
          memmove((void *)clients[c].in.buf, (void *)&clients[c].in.buf[i+1], clients[c].in.len - 1 - i);
          clients[c].in.len = clients[c].in.len - 1 - i;
        } else {
          clients[c].in.len = 0;
        }
      }
    }

    if(clients[c].in.len == sizeof(clients[c].in.buf)) {
          //buffer overrun
          // TODO: ignore this and all until next '\n' or '\r'
    }
  } else {
#ifndef NODEBUG
    if(result == 0)
      olsr_printf(0, "(TELNET) client %i - disconnected\n", c);
    else 
      olsr_printf(0, "(TELNET) client %i recv(): %s\n", c, strerror(errno));
#endif /* NODEBUG */    
    telnet_client_remove(c);
  }
}

static void
telnet_client_write(int fd, void *data __attribute__ ((unused)), unsigned int flags __attribute__ ((unused)))
{
  int c;
  ssize_t result;

  c = telnet_client_find(fd);
  if(c == MAX_CLIENTS) {        // unknown client...???
    remove_olsr_socket(fd, &telnet_client_write, NULL);
    close(fd);
    return;
  }

  result = send(fd, (void *)&(clients[c].out.buf[clients[c].out.written]), clients[c].out.len - clients[c].out.written, 0);
  if (result > 0) {
    clients[c].out.written += result;
    if(clients[c].out.written == clients[c].out.len) {
#ifndef NODEBUG
      olsr_printf(0, "(TELNET) client %i - all data written\n", c);
#endif /* NODEBUG */    
      remove_olsr_socket(fd, &telnet_client_write, NULL);
      clients[c].out.written = clients[c].out.len = 0;
    }
  } else if(result < 0) {
#ifndef NODEBUG
    olsr_printf(0, "(TELNET) client %i write(): %s\n", c, strerror(errno));
#endif /* NODEBUG */    
    telnet_client_remove(c);
  }
}
