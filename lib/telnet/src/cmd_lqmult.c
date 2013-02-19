
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
#include "link_set.h"

#include "telnet_client.h"
#include "telnet_cmd.h"
#include "cmd_lqmult.h"

DEFINE_TELNET_CMD(cmd_lqmult_struct,
                  "lqmult", handle_lqmult,
                  "add/remove/change or show link quality multipliers",
                  " lqmult (add|update) (<interface>|*) <neigbor> <value>\n\r"
                  " lqmult del (<interface>|*) <neigbor>\n\r"
                  " lqmult get (<interface>|*) <neigbor>\n\r"
                  " lqmult flush [<interface>]\n\r"
                  " lqmult list [<interface>]");

int cmd_lqmult_init(void)
{
  return telnet_cmd_add(&cmd_lqmult_struct);
}

const char* cmd_lqmult_get_command(void)
{
  return cmd_lqmult_struct.command;
}


/* struct olsr_lq_mult { */
/*   union olsr_ip_addr addr; */
/*   uint32_t value; */
/*   struct olsr_lq_mult *next; */
/* }; */
  /* struct olsr_lq_mult *mult, *mult_temp; */
  /* ifs->cnf->lq_mult=NULL; */
  /* for (mult = olsr_cnf->interface_defaults->lq_mult; mult; mult=mult->next) { */
  /*   mult_temp=olsr_malloc(sizeof(struct olsr_lq_mult), "telnet inteface add mult_temp"); */
  /*   memcpy(mult_temp,mult,sizeof(struct olsr_lq_mult)); */
  /*   mult_temp->next=ifs->cnf->lq_mult; */
  /*   ifs->cnf->lq_mult=mult_temp; */
  /* } */

#define cmd_lqmult_foreach_interface(FUNC, ARGS...)                 \
  do {                                                              \
    struct olsr_if *ifs;                                            \
    for (ifs = olsr_cnf->interfaces; ifs != NULL; ifs = ifs->next)  \
      FUNC (ifs, ARGS);                                             \
  } while(false)

static struct olsr_if* cmd_lqmult_get_ifs(int c, const char* if_name)
{
  struct olsr_if *ifs = olsrif_ifwithname(if_name);
  if(!ifs) {
    telnet_client_printf(c, "FAILED: no such interface '%s'\n\r", if_name);
    return NULL;
  }
  return ifs;
}

/* ****** ADD ****** */
static void cmd_lqmult_add_if(struct olsr_if *ifs, int c, const union olsr_ip_addr* neighbor, uint32_t value)
{
  struct ipaddr_str addrbuf;

  if(!ifs)
    return;

  telnet_client_printf(c, "adding lqmult %s=%d to interface '%s'\n\r", olsr_ip_to_string(&addrbuf, neighbor), value, ifs->name);
}

static telnet_cmd_function cmd_lqmult_add(int c, const char* if_name, const union olsr_ip_addr* neighbor, uint32_t value)
{
  if(!if_name)
    cmd_lqmult_foreach_interface(cmd_lqmult_add_if, c, neighbor, value);
  else
    cmd_lqmult_add_if(cmd_lqmult_get_ifs(c, if_name), c, neighbor, value);
  return NULL;
}

/* ****** UPDATE ****** */
static void cmd_lqmult_update_if(struct olsr_if *ifs, int c, const union olsr_ip_addr* neighbor, uint32_t value)
{
  struct ipaddr_str addrbuf;

  if(!ifs)
    return;

  telnet_client_printf(c, "updating lqmult for %s on interface '%s' to %d\n\r", olsr_ip_to_string(&addrbuf, neighbor), ifs->name, value);
}

static telnet_cmd_function cmd_lqmult_update(int c, const char* if_name, const union olsr_ip_addr* neighbor, uint32_t value)
{
  if(!if_name)
    cmd_lqmult_foreach_interface(cmd_lqmult_update_if, c, neighbor, value);
  else
    cmd_lqmult_update_if(cmd_lqmult_get_ifs(c, if_name), c, neighbor, value);
  return NULL;
}

/* ****** DEL ****** */
static void cmd_lqmult_del_if(struct olsr_if *ifs, int c, const union olsr_ip_addr* neighbor)
{
  struct ipaddr_str addrbuf;

  if(!ifs)
    return;

  telnet_client_printf(c, "removing lqmult for %s on interface '%s'\n\r", olsr_ip_to_string(&addrbuf, neighbor), ifs->name);
}

static telnet_cmd_function cmd_lqmult_del(int c, const char* if_name, const union olsr_ip_addr* neighbor)
{
  if(!if_name)
    cmd_lqmult_foreach_interface(cmd_lqmult_del_if, c, neighbor);
  else
    cmd_lqmult_del_if(cmd_lqmult_get_ifs(c, if_name), c, neighbor);
  return NULL;
}

/* ****** GET ****** */
static void cmd_lqmult_get_if(const struct olsr_if *ifs, int c, const union olsr_ip_addr* neighbor)
{
  struct ipaddr_str addrbuf;

  if(!ifs)
    return;

  telnet_client_printf(c, "retreiving lqmult for %s on interface '%s'\n\r", olsr_ip_to_string(&addrbuf, neighbor), ifs->name);
}

static telnet_cmd_function cmd_lqmult_get(const int c, const char* if_name, const union olsr_ip_addr* neighbor)
{
  if(!if_name)
    cmd_lqmult_foreach_interface(cmd_lqmult_get_if, c, neighbor);
  else
    cmd_lqmult_get_if(cmd_lqmult_get_ifs(c, if_name), c, neighbor);
  return NULL;
}

/* ****** LIST ****** */
static void cmd_lqmult_list_if(const struct olsr_if *ifs, int c)
{
  struct olsr_lq_mult *mult;

  if(!ifs)
    return;

  telnet_client_printf(c, "%s:\n\r", ifs->name);
  for(mult = ifs->cnf->lq_mult; mult; mult=mult->next) {
    struct ipaddr_str addrbuf;
    const char* n = (ipequal(&(mult->addr), &olsr_ip_zero)) ? "default" : olsr_ip_to_string(&addrbuf, &(mult->addr));
    telnet_client_printf(c, "  %s: %0.2f\n\r", n, (double)(mult->value) / (double)LINK_LOSS_MULTIPLIER);
  }
}

static telnet_cmd_function cmd_lqmult_list(int c, const char* if_name)
{
  if(!if_name)
    cmd_lqmult_foreach_interface(cmd_lqmult_list_if, c);
  else
    cmd_lqmult_list_if(cmd_lqmult_get_ifs(c, if_name), c);
  return NULL;
}

/* ****** FLUSH ****** */
static void cmd_lqmult_flush_if(const struct olsr_if *ifs, int c)
{
  if(!ifs)
    return;

  while(ifs->cnf->lq_mult) {
    struct olsr_lq_mult *mult_temp = ifs->cnf->lq_mult->next;
    free(ifs->cnf->lq_mult);
    ifs->cnf->lq_mult = mult_temp;
  }
  telnet_client_printf(c, "removed all link quality multipliers for interface '%s'\n\r", ifs->name);
}

static telnet_cmd_function cmd_lqmult_flush(int c, const char* if_name)
{
  if(!if_name)
    cmd_lqmult_foreach_interface(cmd_lqmult_flush_if, c);
  else
    cmd_lqmult_flush_if(cmd_lqmult_get_ifs(c, if_name), c);
  return NULL;
}

/* ****** main ****** */

static telnet_cmd_function handle_lqmult(int c, int argc, char* argv[])
{
  if(argc == 2 || argc == 3) {

    if(!strcmp(argv[1], "list"))
      return cmd_lqmult_list(c, argc == 2 ? NULL : argv[2]);

    if(!strcmp(argv[1], "flush"))
      return cmd_lqmult_flush(c, argc == 2 ? NULL : argv[2]);
  }

  if(argc > 3) {
    const char* if_name = !(strcmp(argv[2], "*")) ? NULL : argv[2];
    const union olsr_ip_addr* neighbor = !(strcmp(argv[3] , "default")) ? &olsr_ip_zero : NULL;
    union olsr_ip_addr addr;
    if(!neighbor) {
      if(inet_pton(olsr_cnf->ip_version, argv[3], &addr) <= 0) {
        telnet_client_printf(c, "FAILED: '%s' is not a valid address'\n\r", argv[3]);
        return NULL;
      }
      neighbor = &addr;
    }

    if(!strcmp(argv[1], "get"))
      return cmd_lqmult_get(c, if_name, neighbor);
    else if(!strcmp(argv[1], "del"))
      return cmd_lqmult_del(c, if_name, neighbor);

    if(argc == 5) {
      char* end;
      double value = strtod(argv[4], &end);
      if((value == 0 && argv[4] == end) || value < 0.0 || value > 1.0) {
        telnet_client_printf(c, "FAILED: '%s' not a valid multiplier (must be between 0.0 and 1.0)'\n\r", argv[4]);
        return NULL;
      }
      if(!strcmp(argv[1], "add"))
        return cmd_lqmult_add(c, if_name, neighbor, (uint32_t)(value * LINK_LOSS_MULTIPLIER));
      else if(!strcmp(argv[1], "update"))
        return cmd_lqmult_update(c, if_name, neighbor, (uint32_t)(value * LINK_LOSS_MULTIPLIER));
    }
  }

  telnet_print_usage(c, cmd_lqmult_struct);
  return NULL;
}
