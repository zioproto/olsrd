
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

#include "telnet_client.h"
#include "telnet_cmd.h"
#include "cmd_interface.h"

DEFINE_TELNET_CMD(cmd_interface_struct,
                  "interface", handle_interface,
                  "add/remove or list interfaces",
                  " interface (enable|disable) <name>\n\r"
                  " interface (add|del) <name>\n\r"
                  " interface status <name>\n\r"
                  " interface list");

int cmd_interface_init(void)
{
  return telnet_cmd_add(&cmd_interface_struct);
}

const char* cmd_interface_get_command(void)
{
  return cmd_interface_struct.command;
}

/*
    This is a short version of what the function
      olsrd_sanity_check_cnf() @ src/cfgparser/olsrd_conf.c
    does. Given the knowledge that cnfi and cnf are always different and that there are no
    interface specific lq_mults this can be shortened up...
    would be nice if the core would provide a function to do this...
*/
static inline void cmd_interface_if_deep_copy(const struct olsr_if *ifs)
{
  struct olsr_lq_mult *mult, *mult_temp;

  memcpy((uint8_t*)ifs->cnf, (uint8_t*)olsr_cnf->interface_defaults, sizeof(*ifs->cnf));
  memset((uint8_t*)ifs->cnfi, 0, sizeof(*ifs->cnfi));

  ifs->cnf->lq_mult=NULL;
  for (mult = olsr_cnf->interface_defaults->lq_mult; mult; mult=mult->next) {
    mult_temp=olsr_malloc(sizeof(struct olsr_lq_mult), "telnet interface add mult_temp");
    memcpy(mult_temp,mult,sizeof(struct olsr_lq_mult));
    mult_temp->next=ifs->cnf->lq_mult;
    ifs->cnf->lq_mult=mult_temp;
  }
}

static inline int cmd_interface_holds_mainaddr(const struct interface *const rifs)
{
  return (olsr_cnf->ip_version == AF_INET ? ip4equal(&(olsr_cnf->main_addr.v4), &(rifs->int_addr.sin_addr))
                                          : ip6equal(&(olsr_cnf->main_addr.v6), &(rifs->int6_addr.sin6_addr)));
}

static inline struct olsr_if* cmd_interface_del_check(int c, const char* name)
{
  struct olsr_if *ifs = olsrif_ifwithname(name);
  if(!ifs) {
    telnet_client_printf(c, "FAILED: no such interface '%s'\n\r", name);
    return NULL;
  }
  if((ifnet->int_next == NULL) && (!olsr_cnf->allow_no_interfaces)) {
    telnet_client_printf(c, "FAILED: '%s' is the sole interface and olsrd is configured not to run without interfaces\n\r", name);
    return NULL;
  }
  if(ifs->interf && cmd_interface_holds_mainaddr(ifs->interf)) {
    struct ipaddr_str addrbuf;
    telnet_client_printf(c, "FAILED: '%s' holds the main address (%s) of this instance\n\r", name, olsr_ip_to_string(&addrbuf, &(olsr_cnf->main_addr)));
    return NULL;
  }
  return ifs;
}

static inline void cmd_interface_cleanup(struct olsr_if *ifs)
{
  if(olsr_cnf->interfaces == ifs) {
    olsr_cnf->interfaces = ifs->next;
  } else {
    struct olsr_if *if_tmp;
    for (if_tmp = olsr_cnf->interfaces; if_tmp; if_tmp=if_tmp->next) {
      if(if_tmp->next == ifs) {
        if_tmp->next = ifs->next;
        break;
      }
    }
  }
  free(ifs->name);
  while(ifs->cnf->lq_mult) {
    struct olsr_lq_mult *mult_temp = ifs->cnf->lq_mult->next;
    free(ifs->cnf->lq_mult);
    ifs->cnf->lq_mult = mult_temp;
  }
  free(ifs->cnf);
  free(ifs->cnfi);
  free(ifs);
}


/* ****** ENABLE ****** */
static telnet_cmd_function cmd_interface_enable(int c, const char* name)
{
  struct olsr_if *ifs = olsrif_ifwithname(name);
  if(!ifs) {
    telnet_client_printf(c, "FAILED: no such interface '%s'\n\r", name);
    return NULL;
  }
  if(ifs->cnf->autodetect_chg) {
    if(ifs->interf)
      telnet_client_printf(c, "interface '%s' is enabled and up\n\r", name);
    else
      telnet_client_printf(c, "FAILED: interface '%s' already enabled but seems to be down - check hardware or configuration\n\r", name);
    return NULL;
  }

  ifs->cnf->autodetect_chg = 1;
  telnet_client_printf(c, "enabled interface '%s'\n\r", name);
  return NULL;
}

/* ****** DISABLE ****** */
static telnet_cmd_function cmd_interface_disable(int c, const char* name)
{
  struct olsr_if *ifs = cmd_interface_del_check(c, name);
  if(!ifs)
    return NULL;

  if(ifs->interf)
    olsr_remove_interface(ifs);
  ifs->cnf->autodetect_chg = 0;

  telnet_client_printf(c, "disabled interface %s\n\r", name);
  return NULL;
}

/* ****** ADD ****** */
static telnet_cmd_function cmd_interface_add(int c, const char* name)
{
  const struct olsr_if *ifs = olsr_create_olsrif(name, false);
  if(!ifs) {
    telnet_client_printf(c, "FAILED: to add interface '%s', see log output for further information\n\r", name);
    return NULL;
  }
  cmd_interface_if_deep_copy(ifs);
  telnet_client_printf(c, "added interface %s\n\r", name);
  return NULL;
}

/* ****** DEL ****** */
static telnet_cmd_function cmd_interface_del(int c, const char* name)
{
  struct olsr_if *ifs = cmd_interface_del_check(c, name);
  if(!ifs)
    return NULL;

  if(ifs->interf)
    olsr_remove_interface(ifs);
  cmd_interface_cleanup(ifs);

  telnet_client_printf(c, "removed interface %s\n\r", name);
  return NULL;
}

/* ****** DISABLE ****** */
static telnet_cmd_function cmd_interface_status(int c, const char* name)
{
  const struct olsr_if *ifs = olsrif_ifwithname(name);
  if(ifs) {
    const struct interface *const rifs = ifs->interf;
    telnet_client_printf(c, "Interface '%s:\n\r", ifs->name);
    telnet_client_printf(c, " Status: %s, %s\n\r", ifs->cnf->autodetect_chg ? "enabled" : "disabled", (!rifs) ? "down" : "up");
    if (!rifs)
      return NULL;

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
    return NULL;
  }
  telnet_client_printf(c, "FAILED: no such interface '%s'\n\r", name);
  return NULL;
}

/* ****** main ****** */
static telnet_cmd_function handle_interface(int c, int argc, char* argv[])
{
  if(argc == 2 && !strcmp(argv[1], "list")) {
    const struct olsr_if *ifs;
    for (ifs = olsr_cnf->interfaces; ifs != NULL; ifs = ifs->next)
      telnet_client_printf(c, " %-10s (%s, %s)\n\r", ifs->name, ifs->cnf->autodetect_chg ? "enabled" : "disabled", (!(ifs->interf)) ? "down" : "up");

    return NULL;
  }

  if(argc != 3) {
    telnet_print_usage(c, cmd_interface_struct);
    return NULL;
  }

  if(!strcmp(argv[1], "enable")) {
    return cmd_interface_enable(c, argv[2]);
  }
  else if(!strcmp(argv[1], "disable")) {
    return cmd_interface_disable(c, argv[2]);
  }
  else if(!strcmp(argv[1], "add")) {
    return cmd_interface_add(c, argv[2]);
  }
  else if(!strcmp(argv[1], "del")) {
    return cmd_interface_del(c, argv[2]);
  }
  else if(!strcmp(argv[1], "status")) {
    return cmd_interface_status(c, argv[2]);
  }

  telnet_print_usage(c, cmd_interface_struct);
  return NULL;
}
