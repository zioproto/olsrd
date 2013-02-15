
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

#include <string.h>

#include "olsr.h"
#include "olsr_types.h"
#include "ipcalc.h"

#include "olsrd_telnet.h"
#include "cmd_handler.h"

typedef struct {
  const char* command;
  void (*callback)(int, int, char**);
  const char* short_help;
  const char* usage_text;
} cmd_t;


void hna(int, int, char**);
cmd_t hna_cmd = {
   "hna", hna,
   "alter or show HNA table",
   " hna (add|del) <address>/<netmask>\n\r"
   " hna list"
};

void interface(int, int, char**);
cmd_t interface_cmd = {
   "interface", interface,
   "add/remove or list interfaces",
   " interface (add|del) <name>\n\r"
   " interface status <name>\n\r"
   " interface list"
};

void quit(int, int, char**);
cmd_t quit_cmd = {
   "quit", quit,
   "terminates telnet connection",
   " quit"
};

void terminate(int, int, char**);
cmd_t terminate_cmd = {
   "terminate", terminate,
   "terminate olsr daemon",
   " terminate <reason>"
};

void help(int, int, char**);
cmd_t help_cmd = {
   "help", help,
   "prints usage strings",
   " help [<command>]"
};

const char* USAGE_FMT = "usage:\n\r%s\n\r";
cmd_t* dispatch_table[] = {
  &hna_cmd,
  &interface_cmd,
  &quit_cmd,
  &terminate_cmd,
  &help_cmd
};

void cmd_dispatcher(int c, int argc, char* argv[])
{
  size_t i;

  if(argc < 1)
    return;

  for(i = 0; i < sizeof(dispatch_table)/sizeof(cmd_t*); ++i) {
    if(!strcmp(dispatch_table[i]->command, argv[0]))
      return dispatch_table[i]->callback(c, argc, argv);
  }

  telnet_client_printf(c, "command '%s' unknown - enter help for a list of commands\n\r", argv[0]);
}


void hna(int c, int argc, char* argv[])
{
  struct olsr_ip_prefix hna_entry;

  if(argc == 2 && !strcmp(argv[1], "list")) {
    struct ip_prefix_list *h;
    for (h = olsr_cnf->hna_entries; h != NULL; h = h->next)
      telnet_client_printf(c, " %s\n\r", olsr_ip_prefix_to_string(&(h->net)));
    return;
  }

  if(argc != 3) {
    telnet_client_printf(c, USAGE_FMT, hna_cmd.usage_text);
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
    telnet_client_printf(c, USAGE_FMT, hna_cmd.usage_text);
}



void interface(int c, int argc, char* argv[])
{
  if(argc == 2 && !strcmp(argv[1], "list")) {
    const struct olsr_if *ifs;
    for (ifs = olsr_cnf->interfaces; ifs != NULL; ifs = ifs->next)
      telnet_client_printf(c, " %-10s (%s)\n\r", ifs->name, (!(ifs->interf)) ? "DOWN" : "UP" );

    return;
  }

  if(argc != 3) {
    telnet_client_printf(c, USAGE_FMT, interface_cmd.usage_text);
    return;
  }

  if(!strcmp(argv[1], "add")) {
    const struct olsr_if *ifs = olsr_create_olsrif(argv[2], false);
    if(!ifs || !chk_if_up(ifs, 3)) {
      telnet_client_printf(c, "FAILED: add interface '%s', see log output for further information\n\r", argv[2]);
      return;
    }
  }
  else if(!strcmp(argv[1], "del")) {
    struct olsr_if *ifs = olsrif_ifwithname(argv[2]);
    if(!ifs) {
      telnet_client_printf(c, "FAILED: no such interface '%s'\n\r", argv[2]);
      return;
    }
    olsr_remove_interface(ifs);
  }
  else if(!strcmp(argv[1], "status")) {
    const struct olsr_if *ifs = olsrif_ifwithname(argv[2]);
    if(ifs) {
      const struct interface *const rifs = ifs->interf;
      telnet_client_printf(c, "Interface '%s':\n\r", ifs->name);
      telnet_client_printf(c, " Status: %s\n\r", (!rifs) ? "DOWN" : "UP" );
      if (!rifs)
        return;

      if (olsr_cnf->ip_version == AF_INET) {
        struct ipaddr_str addrbuf, maskbuf, bcastbuf;
        telnet_client_printf(c, " IP: %s\n\r", ip4_to_string(&addrbuf, rifs->int_addr.sin_addr));
        telnet_client_printf(c, " MASK: %s\n\r", ip4_to_string(&maskbuf, rifs->int_netmask.sin_addr));
        telnet_client_printf(c, " BCAST: %s\n\r", ip4_to_string(&bcastbuf, rifs->int_broadaddr.sin_addr));
      } else {
        struct ipaddr_str addrbuf, maskbuf;
        telnet_client_printf(c, " IP: %s\n\r", ip6_to_string(&addrbuf, &rifs->int6_addr.sin6_addr));
        telnet_client_printf(c, " MCAST: %s\n\r", ip6_to_string(&maskbuf, &rifs->int6_multaddr.sin6_addr));
      }
      telnet_client_printf(c, " MTU: %d\n\r", rifs->int_mtu);
      telnet_client_printf(c, " WLAN: %s\n\r", rifs->is_wireless ? "Yes" : "No");
      return;
    }
    telnet_client_printf(c, "FAILED: no such interface '%s'\n\r", argv[2]);
    return;
  }
  else
    telnet_client_printf(c, USAGE_FMT, interface_cmd.usage_text);
}



void quit(int c, int argc __attribute__ ((unused)), char* argv[] __attribute__ ((unused)))
{
  if(argc != 1) {
    telnet_client_printf(c, USAGE_FMT, quit_cmd.usage_text);
    return;
  }

  telnet_client_quit(c);
}


void help(int c, int argc __attribute__ ((unused)), char* argv[] __attribute__ ((unused)))
{
  size_t i;

  switch(argc) {
  case 1:
    for(i = 0; i < sizeof(dispatch_table)/sizeof(cmd_t*); ++i) {
      telnet_client_printf(c, " %-16s %s\n\r", dispatch_table[i]->command, dispatch_table[i]->short_help);
    }
    return;
  case 2:
    for(i = 0; i < sizeof(dispatch_table)/sizeof(cmd_t*); ++i) {
      if(!strcmp(dispatch_table[i]->command, argv[1])) {
        telnet_client_printf(c, "%s: %s\n\r\n\r", dispatch_table[i]->command, dispatch_table[i]->short_help);
        telnet_client_printf(c, USAGE_FMT, dispatch_table[i]->usage_text);
        return;
      }
    }
    return telnet_client_printf(c, "command '%s' unknown\n\r", argv[1]);
  default:
    return telnet_client_printf(c, USAGE_FMT, help_cmd.usage_text);
  }

}


void terminate(int c, int argc, char* argv[])
{
  if(argc != 2) {
    telnet_client_printf(c, USAGE_FMT, terminate_cmd.usage_text);
    return;
  }

  olsr_exit(argv[1], EXIT_SUCCESS);
}
