
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

#include <string.h>

#include "olsr.h"
#include "olsr_types.h"
#include "ipcalc.h"

#include "olsrd_telnet.h"
#include "cmd_handler.h"


static void hna(int, int, char**);
static void terminate(int, int, char**);
static void quit(int, int, char**);
static void help(int, int, char**);


struct dispatch_table_element {
  const char* command;
  void (*callback)(int, int, char**);
  const char* helptext;
};

struct dispatch_table_element dispatch_table[] = {
{ "hna", hna, "update HNA table" },
{ "quit", quit, "terminates telnet connection" },
{ "terminate", terminate, "terminate olsrd" },
{ "help", help, "prints this" }
};

void cmd_dispatcher(int c, int argc, char* argv[])
{
  size_t i;

  if(argc < 1)
    return;

  for(i = 0; i < sizeof(dispatch_table)/sizeof(struct dispatch_table_element); ++i) {
    if(!strcmp(dispatch_table[i].command, argv[0]))
      return dispatch_table[i].callback(c, argc, argv);
  }

  telnet_client_printf(c, "command '%s' unknown\n\r", argv[0]);
}

static void hna(int c, int argc, char* argv[])
{
  struct olsr_ip_prefix hna_entry;

  if(argc != 3) {
    telnet_client_printf(c, "usage: hna (add|del) <address>/<netmask>\n\r");
    return;
  }

  if(olsr_string_to_prefix(olsr_cnf->ip_version, &hna_entry, argv[2])) {
    telnet_client_printf(c, "address invalid\n\r");
    return;
  }

  if(!strcmp(argv[1], "add")) {
    if(ip_prefix_list_find(olsr_cnf->hna_entries, &(hna_entry.prefix), hna_entry.prefix_len))
      telnet_client_printf(c, "FAILED: %s already in HNA table\n\r", olsr_ip_prefix_to_string(&hna_entry));
    else {
      ip_prefix_list_add(&olsr_cnf->hna_entries, &(hna_entry.prefix), hna_entry.prefix_len);
      telnet_client_printf(c, "added %s to HNA table\n\r", olsr_ip_prefix_to_string(&hna_entry));
    }
  }
  else if(!strcmp(argv[1], "del")) {
    if(ip_prefix_list_remove(&olsr_cnf->hna_entries, &(hna_entry.prefix), hna_entry.prefix_len))
      telnet_client_printf(c, "removed %s from HNA table\n\r", olsr_ip_prefix_to_string(&hna_entry));
    else
      telnet_client_printf(c, "FAILED: %s not found in HNA table\n\r", olsr_ip_prefix_to_string(&hna_entry));
  }
  else
    telnet_client_printf(c, "usage: hna (add|del) <address>/<netmask>\n\r");
}

static void quit(int c, int argc __attribute__ ((unused)), char* argv[] __attribute__ ((unused)))
{
  telnet_client_quit(c);
}

static void help(int c, int argc __attribute__ ((unused)), char* argv[] __attribute__ ((unused)))
{
  size_t i;
  for(i = 0; i < sizeof(dispatch_table)/sizeof(struct dispatch_table_element); ++i) {
    telnet_client_printf(c, "  %s\t%s\n\r", dispatch_table[i].command, dispatch_table[i].helptext);
  }
}

static void terminate(int c, int argc, char* argv[])
{
  if(argc != 2) {
    telnet_client_printf(c, "usage: terminate <reason>\n\r");
    return;
  }

  olsr_exit(argv[1], EXIT_SUCCESS);
}
