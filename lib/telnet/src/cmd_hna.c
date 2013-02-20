
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

#include "olsr_types.h"
#include "ipcalc.h"

#include "cmd_hna.h"
#include "telnet_client.h"
#include "telnet_cmd.h"

DEFINE_TELNET_CMD(cmd_hna_struct,
                  "hna", handle_hna,
                  "alter or show HNA table",
                  " hna (add|del) <address>/<netmask>\n\r"
                  " hna list");

int cmd_hna_init(void)
{
  return telnet_cmd_add(&cmd_hna_struct);
}

const char* cmd_hna_get_command(void)
{
  return cmd_hna_struct.command;
}


static telnet_cmd_function cmd_hna_add(int c, struct olsr_ip_prefix* hna_entry)
{
  if(ip_prefix_list_find(olsr_cnf->hna_entries, &(hna_entry->prefix), hna_entry->prefix_len)) {
    telnet_client_printf(c, "FAILED: %s already in HNA table\n\r", olsr_ip_prefix_to_string(hna_entry));
    return NULL;
  }

  ip_prefix_list_add(&olsr_cnf->hna_entries, &(hna_entry->prefix), hna_entry->prefix_len);
  telnet_client_printf(c, "added %s to HNA table\n\r", olsr_ip_prefix_to_string(hna_entry));
  return NULL;
}

static telnet_cmd_function cmd_hna_del(int c, struct olsr_ip_prefix* hna_entry)
{
  if(ip_prefix_list_remove(&olsr_cnf->hna_entries, &(hna_entry->prefix), hna_entry->prefix_len)) {
    telnet_client_printf(c, "removed %s from HNA table\n\r", olsr_ip_prefix_to_string(hna_entry));
    return NULL;
  }

  telnet_client_printf(c, "FAILED: %s not found in HNA table\n\r", olsr_ip_prefix_to_string(hna_entry));
  return NULL;
}

static telnet_cmd_function handle_hna(int c, int argc, char* argv[])
{
  struct olsr_ip_prefix hna_entry;

  if(argc == 2 && !strcmp(argv[1], "list")) {
    struct ip_prefix_list *h;
    for (h = olsr_cnf->hna_entries; h != NULL; h = h->next)
      telnet_client_printf(c, " %s\n\r", olsr_ip_prefix_to_string(&(h->net)));
    return NULL;
  }

  if(argc != 3) {
    telnet_print_usage(c, cmd_hna_struct);
    return NULL;
  }

  if(olsr_string_to_prefix(olsr_cnf->ip_version, &hna_entry, argv[2])) {
    telnet_client_printf(c, "FAILED: address invalid\n\r");
    return NULL;
  }

  if(!strcmp(argv[1], "add")) {
    return cmd_hna_add(c, &hna_entry);
  }
  else if(!strcmp(argv[1], "del")) {
    return cmd_hna_del(c, &hna_entry);
  }

  telnet_print_usage(c, cmd_hna_struct);
  return NULL;
}
