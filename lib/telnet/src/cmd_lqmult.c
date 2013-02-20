
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
                  " lqmult (add|update|set) (<interface>|*) <neigbor> <value>\n\r"
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

static inline uint32_t cmd_lqmult_value_as_int(double value)
{
  return (uint32_t)(value * LINK_LOSS_MULTIPLIER);
}

static inline double cmd_lqmult_value_as_double(uint32_t value)
{
  return (double)(value) / (double)LINK_LOSS_MULTIPLIER;
}

static struct olsr_lq_mult* cmd_lqmult_find_neighbor(const struct olsr_if *ifs, const union olsr_ip_addr* neighbor)
{
  struct olsr_lq_mult* mult;
  for(mult = ifs->cnf->lq_mult; mult; mult = mult->next)
    if(ipequal(&(mult->addr), neighbor))
      return mult;

  return NULL;
}

static void cmd_lqmult_update_multiplier(const struct olsr_if *ifs)
{
  struct link_entry *walker;
  OLSR_FOR_ALL_LINK_ENTRIES(walker) {
    uint32_t val = 0;
    struct olsr_lq_mult* mult;

    if(walker->inter->olsr_if != ifs)
      continue;

    for (mult = ifs->cnf->lq_mult; mult != NULL; mult = mult->next) {
      if ((ipequal(&mult->addr, &olsr_ip_zero) && val == 0) || ipequal(&mult->addr, &walker->neighbor_iface_addr)) {
        val = mult->value;
      }
    }
    walker->loss_link_multiplier = val ? val : LINK_LOSS_MULTIPLIER;
  } OLSR_FOR_ALL_LINK_ENTRIES_END(walker);
}

/* ****** ADD ****** */
static void cmd_lqmult_add_if(const struct olsr_if *ifs, const int c, const union olsr_ip_addr* neighbor, double value)
{
  struct ipaddr_str addrbuf;
  struct olsr_lq_mult* mult;

  if(!ifs)
    return;

  mult = cmd_lqmult_find_neighbor(ifs, neighbor);
  if(mult) {
    telnet_client_printf(c, "FAILED: %s already has a multiplier of %0.2f on interface '%s'\n\r", olsr_ip_to_string(&addrbuf, neighbor),
                         cmd_lqmult_value_as_double(mult->value), ifs->name);
    return;
  }
  mult = olsr_malloc(sizeof(struct olsr_lq_mult), "lq mult");
  mult->addr = *neighbor;
  mult->value = cmd_lqmult_value_as_int(value);
  mult->next = ifs->cnf->lq_mult;
  ifs->cnf->lq_mult = mult;

  telnet_client_printf(c, "added multiplier %0.2f for %s to interface '%s'\n\r", value, olsr_ip_to_string(&addrbuf, neighbor), ifs->name);
  cmd_lqmult_update_multiplier(ifs);
}

static telnet_cmd_function cmd_lqmult_add(const int c, const char* if_name, const union olsr_ip_addr* neighbor, double value)
{
  if(!if_name)
    cmd_lqmult_foreach_interface(cmd_lqmult_add_if, c, neighbor, value);
  else
    cmd_lqmult_add_if(cmd_lqmult_get_ifs(c, if_name), c, neighbor, value);
  return NULL;
}

/* ****** UPDATE ****** */
static void cmd_lqmult_update_if(const struct olsr_if *ifs, const int c, const union olsr_ip_addr* neighbor, double value)
{
  struct ipaddr_str addrbuf;
  struct olsr_lq_mult* mult;
  double old_value;

  if(!ifs)
    return;

  mult = cmd_lqmult_find_neighbor(ifs, neighbor);
  if(!mult) {
    telnet_client_printf(c, "FAILED: %s has no multiplier on interface '%s'\n\r", olsr_ip_to_string(&addrbuf, neighbor), ifs->name);
    return;
  }
  old_value = cmd_lqmult_value_as_double(mult->value);
  mult->value = cmd_lqmult_value_as_int(value);
  telnet_client_printf(c, "updated multiplier for %s on interface '%s' from %0.2f to %0.2f\n\r", olsr_ip_to_string(&addrbuf, neighbor), ifs->name, old_value, value);
  cmd_lqmult_update_multiplier(ifs);
}

static telnet_cmd_function cmd_lqmult_update(const int c, const char* if_name, const union olsr_ip_addr* neighbor, double value)
{
  if(!if_name)
    cmd_lqmult_foreach_interface(cmd_lqmult_update_if, c, neighbor, value);
  else
    cmd_lqmult_update_if(cmd_lqmult_get_ifs(c, if_name), c, neighbor, value);
  return NULL;
}

/* ****** SET ****** */
static void cmd_lqmult_set_if(const struct olsr_if *ifs, const int c, const union olsr_ip_addr* neighbor, double value)
{
  struct ipaddr_str addrbuf;
  struct olsr_lq_mult* mult;

  if(!ifs)
    return;

  mult = cmd_lqmult_find_neighbor(ifs, neighbor);
  if(!mult) {
    mult = olsr_malloc(sizeof(struct olsr_lq_mult), "lq mult");
    mult->addr = *neighbor;
    mult->next = ifs->cnf->lq_mult;
    ifs->cnf->lq_mult = mult;
  }
  mult->value = cmd_lqmult_value_as_int(value);

  telnet_client_printf(c, "set multiplier %0.2f for %s on interface '%s'\n\r", value, olsr_ip_to_string(&addrbuf, neighbor), ifs->name);
  cmd_lqmult_update_multiplier(ifs);
}

static telnet_cmd_function cmd_lqmult_set(const int c, const char* if_name, const union olsr_ip_addr* neighbor, double value)
{
  if(!if_name)
    cmd_lqmult_foreach_interface(cmd_lqmult_set_if, c, neighbor, value);
  else
    cmd_lqmult_set_if(cmd_lqmult_get_ifs(c, if_name), c, neighbor, value);
  return NULL;
}

/* ****** DEL ****** */
static void cmd_lqmult_del_if(const struct olsr_if *ifs, const int c, const union olsr_ip_addr* neighbor)
{
  struct ipaddr_str addrbuf;
  struct olsr_lq_mult* mult;

  if(!ifs)
    return;

  mult = cmd_lqmult_find_neighbor(ifs, neighbor);
  if(!mult) {
    telnet_client_printf(c, "FAILED: %s has no multiplier on interface '%s'\n\r", olsr_ip_to_string(&addrbuf, neighbor), ifs->name);
    return;
  }
  if(ifs->cnf->lq_mult == mult) {
    ifs->cnf->lq_mult = mult->next;
  } else {
    struct olsr_lq_mult* mult_temp;
    for (mult_temp = ifs->cnf->lq_mult; mult_temp; mult_temp=mult_temp->next) {
      if(mult_temp->next == mult) {
        mult_temp->next = mult->next;
        break;
      }
    }
  }
  free(mult);
  cmd_lqmult_update_multiplier(ifs);
  telnet_client_printf(c, "removed lqmult for %s on interface '%s'\n\r", olsr_ip_to_string(&addrbuf, neighbor), ifs->name);
}

static telnet_cmd_function cmd_lqmult_del(const int c, const char* if_name, const union olsr_ip_addr* neighbor)
{
  if(!if_name)
    cmd_lqmult_foreach_interface(cmd_lqmult_del_if, c, neighbor);
  else
    cmd_lqmult_del_if(cmd_lqmult_get_ifs(c, if_name), c, neighbor);
  return NULL;
}

/* ****** GET ****** */
static void cmd_lqmult_get_if(const struct olsr_if *ifs, const int c, const union olsr_ip_addr* neighbor)
{
  struct ipaddr_str addrbuf;
  struct olsr_lq_mult* mult;

  if(!ifs)
    return;

  mult = cmd_lqmult_find_neighbor(ifs, neighbor);
  if(!mult) {
    telnet_client_printf(c, "FAILED: %s has no multiplier on interface '%s'\n\r", olsr_ip_to_string(&addrbuf, neighbor), ifs->name);
    return;
  }
  telnet_client_printf(c, "%s: %0.2f\n\r", ifs->name, cmd_lqmult_value_as_double(mult->value));
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
static void cmd_lqmult_list_if(const struct olsr_if *ifs, const int c)
{
  struct olsr_lq_mult *mult;

  if(!ifs)
    return;

  telnet_client_printf(c, "%s:\n\r", ifs->name);
  for(mult = ifs->cnf->lq_mult; mult; mult=mult->next) {
    struct ipaddr_str addrbuf;
    const char* n = (ipequal(&(mult->addr), &olsr_ip_zero)) ? "default" : olsr_ip_to_string(&addrbuf, &(mult->addr));
    telnet_client_printf(c, "  %s: %0.2f\n\r", n, cmd_lqmult_value_as_double(mult->value));
  }
}

static telnet_cmd_function cmd_lqmult_list(const int c, const char* if_name)
{
  if(!if_name)
    cmd_lqmult_foreach_interface(cmd_lqmult_list_if, c);
  else
    cmd_lqmult_list_if(cmd_lqmult_get_ifs(c, if_name), c);
  return NULL;
}

/* ****** FLUSH ****** */
static void cmd_lqmult_flush_if(const struct olsr_if *ifs, const int c)
{
  if(!ifs)
    return;

  while(ifs->cnf->lq_mult) {
    struct olsr_lq_mult *mult_temp = ifs->cnf->lq_mult->next;
    free(ifs->cnf->lq_mult);
    ifs->cnf->lq_mult = mult_temp;
  }
  telnet_client_printf(c, "flushed link quality multipliers for interface '%s'\n\r", ifs->name);
  cmd_lqmult_update_multiplier(ifs);
}

static telnet_cmd_function cmd_lqmult_flush(const int c, const char* if_name)
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
      if((value == 0 && argv[4] == end) || cmd_lqmult_value_as_int(value) > LINK_LOSS_MULTIPLIER) {
        telnet_client_printf(c, "FAILED: '%s' not a valid multiplier (must be between 0.0 and 1.0)'\n\r", argv[4]);
        return NULL;
      }
      if(!strcmp(argv[1], "add"))
        return cmd_lqmult_add(c, if_name, neighbor, value);
      else if(!strcmp(argv[1], "update"))
        return cmd_lqmult_update(c, if_name, neighbor, value);
      else if(!strcmp(argv[1], "set"))
        return cmd_lqmult_set(c, if_name, neighbor, value);
    }
  }

  telnet_print_usage(c, cmd_lqmult_struct);
  return NULL;
}
