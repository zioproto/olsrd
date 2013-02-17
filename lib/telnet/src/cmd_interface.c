
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

#include <unistd.h>

#include "olsr.h"
#include "olsr_types.h"
#include "ipcalc.h"
#include "interfaces.h"

#include "olsrd_telnet.h"
#include "cmd_handler.h"
#include "cmd_interface.h"

static void cmd_interface_add(int c, const char* name)
{
  const struct olsr_if *ifs = olsr_create_olsrif(name, false);
  if(!ifs) {
    telnet_client_printf(c, "FAILED: to add interface '%s', see log output for further information\n\r", name);
    return;
  }
/*
  interface config deep copy
    This is a short version of what the function
      olsrd_sanity_check_cnf() @ src/cfgparser/olsrd_conf.c
    does. Given the knowledge that cnfi and cnf are always different and that there are no
    inteface specific lq_mults.
    would be nice if the core would provide a function to do this...
*/
  memcpy((uint8_t*)ifs->cnf, (uint8_t*)olsr_cnf->interface_defaults, sizeof(*ifs->cnf));
  memset((uint8_t*)ifs->cnfi, 0, sizeof(*ifs->cnfi));
  {
    struct olsr_lq_mult *mult, *mult_temp;
    ifs->cnf->lq_mult=NULL;
    for (mult = olsr_cnf->interface_defaults->lq_mult; mult; mult=mult->next) {
      mult_temp=olsr_malloc(sizeof(struct olsr_lq_mult), "telnet inteface add mult_temp");
      memcpy(mult_temp,mult,sizeof(struct olsr_lq_mult));
      mult_temp->next=ifs->cnf->lq_mult;
      ifs->cnf->lq_mult=mult_temp;
    }
  }
/* end of interface config deep copy */
}

static void cmd_interface_del(int c, const char* name)
{
  struct olsr_if *ifs = olsrif_ifwithname(name);
  if(!ifs) {
    telnet_client_printf(c, "FAILED: no such interface '%s'\n\r", name);
    return;
  }
  if((ifnet->int_next == NULL) && (!olsr_cnf->allow_no_interfaces)) {
    telnet_client_printf(c, "FAILED: '%s' is the sole interface and olsrd is configured not to run without interfaces\n\r", name);
    return;
  }
  if(ifs->interf)
    olsr_remove_interface(ifs);

/* also removing interface from global configuration */
  if(olsr_cnf->interfaces == ifs) {
    olsr_cnf->interfaces = ifs->next;
    free(ifs);
  } else {
    struct olsr_if *if_tmp;
    for (if_tmp = olsr_cnf->interfaces; if_tmp; if_tmp=if_tmp->next) {
      if(if_tmp->next == ifs) {
        if_tmp->next = ifs->next;
        free(ifs);
        break;
      }
    }
  }
/* also removing interface from global configuration */
}

static void cmd_interface_status(int c, const char* name)
{
  const struct olsr_if *ifs = olsrif_ifwithname(name);
  if(ifs) {
    const struct interface *const rifs = ifs->interf;
    telnet_client_printf(c, "Interface '%s':\n\r", ifs->name);
    telnet_client_printf(c, " Status: %s\n\r", (!rifs) ? "Down" : "Up" );
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
  telnet_client_printf(c, "FAILED: no such interface '%s'\n\r", name);
}

static void cmd_interface(int c, int argc, char* argv[])
{
  if(argc == 2 && !strcmp(argv[1], "list")) {
    const struct olsr_if *ifs;
    for (ifs = olsr_cnf->interfaces; ifs != NULL; ifs = ifs->next)
      telnet_client_printf(c, " %-10s (%s)\n\r", ifs->name, (!(ifs->interf)) ? "DOWN" : "UP" );

    return;
  }

  if(argc != 3) {
    print_usage(c, &interface_cmd);
    return;
  }

  if(!strcmp(argv[1], "add")) {
    cmd_interface_add(c, argv[2]);
  }
  else if(!strcmp(argv[1], "del")) {
    cmd_interface_del(c, argv[2]);
  }
  else if(!strcmp(argv[1], "status")) {
    cmd_interface_status(c, argv[2]);
  }
  else
    print_usage(c, &interface_cmd);
}

cmd_t interface_cmd = {
   "interface", cmd_interface,
   "add/remove or list interfaces",
   " interface (add|del) <name>\n\r"
   " interface status <name>\n\r"
   " interface list"
};
