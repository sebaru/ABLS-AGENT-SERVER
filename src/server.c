/******************************************************************************************************************************/
/* ABLS-AGENT-SERVER/server.c  Template agent server                                                                          */
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

/******************************************************************************************************************************/
/* main: Prend en charge l'agent                                                                                              */
/* Entrée: argc, argv                                                                                                         */
/* Sortie: aucune                                                                                                             */
/******************************************************************************************************************************/
 gint main(gint argc, gchar *argv[])
  { gchar *hostname = g_utf8_strup ( g_get_host_name(), -1 );                /* Le tech_id d'un agent server est son hostname */
    setenv ( "ABLS_AGENT_TECH_ID", hostname, 1 );
    g_free ( hostname );
    struct ABLS_AGENT *agent = Agent_init ( argv[0], "server", ABLS_AGENT_SERVER_VERSION, sizeof(struct ABLS_SERVER_VARS), argc, argv );
    /*struct ABLS_AGENT_VARS *vars = agent->vars;*/

    Mqtt_subscribe ( agent->mqtt_api, "%s/AGENT/+/INSTALL", agent->domain_uuid );    /* Pour installer les agents sur le server */
    Mqtt_subscribe ( agent->mqtt_api, "%s/AGENT/+/UPGRADE", agent->domain_uuid );
    Mqtt_subscribe ( agent->mqtt_api, "%s/AGENT/+/RESTART", agent->domain_uuid );
    Mqtt_subscribe ( agent->mqtt_api, "%s/AGENT/+/STOP",    agent->domain_uuid );
    Mqtt_subscribe ( agent->mqtt_api, "%s/AGENT/+/START",   agent->domain_uuid );

    Agent_is_ready ( agent );

    while(agent->Agent_run == AGENT_IS_RUNNING)                                              /* On tourne tant que necessaire */
     { Agent_loop ( agent );                                             /* Loop sur l'agent pour mettre a jour la telemetrie */
/****************************************************** Ecoute du master ******************************************************/
       JsonNode *mqtt_local_message;
       while ( (mqtt_local_message = Mqtt_get_message ( agent->mqtt_local ) ) != NULL )
        { Json_unref ( mqtt_local_message );
        }
/****************************************************** Ecoute de l'api *******************************************************/
       JsonNode *mqtt_api_message;
       while ( (mqtt_api_message = Agent_get_mqtt_api_message ( agent ) ) != NULL )
        { gchar chaine[256];
          gchar *target = Json_get_string ( mqtt_api_message, "mqtt_topic_lvl2" );
          gchar *classe = Json_get_string ( mqtt_api_message, "agent_classe" );
/*------------------------------------------------------------ Stop ----------------------------------------------------------*/
          if ( Mqtt_topic_is ( mqtt_api_message, 4, "+", "AGENT", "+", "STOP" ) )
           { if(classe)
              { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to STOP %s (class %s)", target, classe );
                if (g_strcmp0 ( target, agent->agent_tech_id ) == 0)
                 { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR,
                         "Cannot self stop this agent-server. keep Running." );
                 }
                else
                 { g_snprintf ( chaine, sizeof(chaine), "abls-agent-%s@%s", classe, target );
                   Exec_sudo ( "systemctl", "disable", "--now", chaine, NULL );
                 }
              }
             else
              { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to STOP %s, but classe not provided", target );
              }
           }
/*------------------------------------------------------------ Start ---------------------------------------------------------*/
          else if ( Mqtt_topic_is ( mqtt_api_message, 4, "+", "AGENT", "+", "START" ) )
           { if(classe)
              { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to START %s (class %s)", target, classe );
                if (g_strcmp0 ( target, agent->agent_tech_id ) == 0)
                 { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR,
                         "Cannot self start this agent-server. keep Running." );
                 }
                else
                 { g_snprintf ( chaine, sizeof(chaine), "abls-agent-%s", classe);
                   gchar *path = g_find_program_in_path(chaine);
                   if (!path)
                    { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE,
                           "package '%s' not found. Install in progress.", chaine );
                      Exec_sudo ( (agent->is_debian ? "apt" : "dnf"), "install", chaine, NULL );
                    } else g_free(path);
                   g_snprintf ( chaine, sizeof(chaine), "abls-agent-%s@%s", classe, target );
                   Exec_sudo ( "systemctl", "enable", "--now", chaine, NULL );
                 }
              }
             else
              { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to START %s, but classe not provided", target );
              }
           }
/*------------------------------------------------------------ Restart -------------------------------------------------------*/
          else if ( Mqtt_topic_is ( mqtt_api_message, 4, "+", "AGENT", "+", "RESTART" ) )
           { if(classe)
              { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to RESTART %s (class %s)", target, classe );
                g_snprintf ( chaine, sizeof(chaine), "abls-agent-%s@%s", classe, target );
                Exec_sudo ( "systemctl", "restart", chaine, NULL );}
             else
              { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to RESTART %s, but classe not provided", target );
              }
           }
          else if ( Mqtt_topic_is ( mqtt_api_message, 4, "+", "AGENT", "+", "UPGRADE" ) )
           { if(classe)
              { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to UPGRADE %s (class %s)", target, classe );
                g_snprintf ( chaine, sizeof(chaine), "abls-agent-%s", classe );
                Exec_sudo ( (agent->is_debian ? "apt" : "dnf"), (agent->is_debian ? "update" : "upgrade"), chaine, "-y", NULL );
                g_snprintf ( chaine, sizeof(chaine), "abls-agent-%s@%s", classe, target );
                Exec_sudo ( "systemctl", "restart", chaine, NULL );
              }
             else
              { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to UPGRADE %s, but classe not provided", target );
              }
           }
          else Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE, "API sent unknown command %s", Json_get_string ( mqtt_api_message, "mqtt_topic" ) );
          Json_unref (mqtt_api_message);
        }
     }

    Agent_end(agent);
  }
/*----------------------------------------------------------------------------------------------------------------------------*/
