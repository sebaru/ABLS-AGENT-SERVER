/******************************************************************************************************************************/
/* ABLS-AGENT-SERVER/server.c  Template agent server                                                                         */
/* Projet Abls-Habitat                   Gestion d'habitat                                                17.07.2026 12:00:00 */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * server.c
 * This file is part of Abls-Habitat
 *
 * Copyright (C) 1988-2026 - Sebastien LEFEVRE
 *
 * ABLS-AGENT-SERVER is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ABLS-AGENT-SERVER is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ABLS-AGENT-SERVER; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "server.h"

gint main(gint argc, gchar *argv[]) {
  struct ABLS_AGENT *agent =
      Agent_init(argv[0], "server", ABLS_AGENT_SERVER_VERSION,
                 sizeof(struct ABLS_SERVER_VARS), argc, argv);
  struct ABLS_SERVER_VARS *vars = agent->vars;

  vars->initialized = TRUE;

  while (agent->Agent_run == AGENT_IS_RUNNING) {
    Agent_loop(agent);

    /* Template hook: consume MQTT/config messages and add server-specific logic here. */
    JsonNode *mqtt_local_message;
    while ((mqtt_local_message = Mqtt_get_message(agent->mqtt_local)) != NULL) {
      Json_unref(mqtt_local_message);
    }
  }

  Agent_end(agent);
}
