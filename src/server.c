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
 struct ABLS_AGENT *Agent = NULL;                                                                     /* Structure de l'agent */
 struct ABLS_SERVER_VARS *Agent_vars = NULL;                                            /* Structure des variables de l'agent */

/******************************************************************************************************************************/
/* Start_one_agent: Installe et demarre un agent classe/tech_id en parametre                                                  */
/* Entrée: agent, agent_classe, agent_tech_id                                                                                 */
/* Sortie: aucune                                                                                                             */
/******************************************************************************************************************************/
 static void Start_one_agent ( gchar *agent_classe, gchar *agent_tech_id )
  { if (!agent_classe || !agent_tech_id) return;
    Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE, "Starting %s (class %s)", agent_tech_id, agent_classe );

    if (g_strcmp0 ( agent_tech_id, Agent->agent_tech_id ) == 0) return; /* On ne peut pas demarrer l'agent server sur lui-meme */

    gchar chaine[256];
    g_snprintf ( chaine, sizeof(chaine), "abls-agent-%s", agent_classe );
    gchar *path = g_find_program_in_path(chaine);
    if (!path)
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE,
            "package '%s' not found. Install in progress.", chaine );
       Thread_shell_queue ( Agent, "AGENT-INSTALL", "sudo -n %s install -y %s", (Agent->is_apt ? "apt" : "dnf"), chaine );
     } else g_free(path);
    g_snprintf ( chaine, sizeof(chaine), "abls-agent-%s@%s", agent_classe, agent_tech_id );
    Thread_shell_queue ( Agent, "AGENT-ENABLE", "sudo -n systemctl enable --now %s", chaine );
    Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE, "%s (class %s) is starting", agent_tech_id, agent_classe );
  }
/******************************************************************************************************************************/
/* Start_agents_by_array: Installe et demarre les agents contenus dans un tableau JSON                                        */
/* Entrée: array, index, element, user_data                                                                                   */
/* Sortie: aucune                                                                                                             */
/******************************************************************************************************************************/
 static void Start_agents_by_array ( JsonArray *array, guint index, JsonNode *element, gpointer user_data )
  { if (!array || !element || !user_data) return;
    gchar *agent_classe  = Json_get_string ( element, "agent_classe" );
    gchar *agent_tech_id = Json_get_string ( element, "agent_tech_id" );
    if (agent_classe && agent_tech_id)
     { Start_one_agent ( agent_classe, agent_tech_id ); }
  }
/******************************************************************************************************************************/
/* main: Prend en charge l'agent                                                                                              */
/* Entrée: argc, argv                                                                                                         */
/* Sortie: aucune                                                                                                             */
/******************************************************************************************************************************/
 gint main(gint argc, gchar *argv[])
  { gchar *hostname = g_utf8_strup ( g_get_host_name(), -1 );                /* Le tech_id d'un agent server est son hostname */
    setenv ( "ABLS_AGENT_TECH_ID", hostname, 1 );
    g_free ( hostname );
    setenv ( "ABLS_TPS", "100", 1 );
    Agent = Agent_init ( argv[0], "server", ABLS_AGENT_SERVER_VERSION, sizeof(struct ABLS_SERVER_VARS), argc, argv );
    Agent_vars = Agent->vars;

    g_mkdir ( "Dls", 0755 );                                                                    /* Creation du repertoire DLS */

    Mqtt_subscribe ( Agent->mqtt_api, "%s/AGENT/+/UPGRADE", Agent->domain_uuid );  /* Pour installer les agents sur le server */
    Mqtt_subscribe ( Agent->mqtt_api, "%s/AGENT/+/RESTART", Agent->domain_uuid );
    Mqtt_subscribe ( Agent->mqtt_api, "%s/AGENT/+/STOP",    Agent->domain_uuid );
    Mqtt_subscribe ( Agent->mqtt_api, "%s/AGENT/+/START",   Agent->domain_uuid );

    Mqtt_subscribe ( Agent->mqtt_local, "SET_AI/#" );
    Mqtt_subscribe ( Agent->mqtt_local, "SET_DI/#" );
    Mqtt_subscribe ( Agent->mqtt_local, "SET_WATCHDOG/#" );

    gboolean is_master = Json_get_bool ( Agent->api_config, "is_master" );

    Agent_is_ready ( Agent );

    /* Demarrage des agents locaux a activer */
    Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE,
          "Starting %d local_agents", Json_array_get_length(Agent->api_config, "local_agents") );
    Json_foreach_array_element ( Agent->api_config, "local_agents", Start_agents_by_array, NULL );

    /* Demarrage du DLS si master */
    if (is_master)
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE, "This server is the master of the domain" );
       Dls_init();
       Agent_set_status ( Agent, "D.L.S Running" );
     }
    else
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE, "This server is a slave of the domain" );
       Agent_set_status ( Agent, "Waiting for command" );
     }

    while(Agent->Agent_run == AGENT_IS_RUNNING)                                              /* On tourne tant que necessaire */
     { Agent_loop ( Agent );                                             /* Loop sur l'agent pour mettre a jour la telemetrie */
/****************************************************** Ecoute du master ******************************************************/
       JsonNode *mqtt_local_message;
       while ( (mqtt_local_message = Mqtt_get_message ( Agent->mqtt_local ) ) != NULL )
        { if (Mqtt_topic_is ( mqtt_local_message, 2, "SET_AI", "+" ))
           { Json_add_string ( mqtt_local_message, "agent_tech_id", Json_get_string ( mqtt_local_message, "mqtt_topic_lvl1" ) );
             Json_add_string ( mqtt_local_message, "agent_acronyme", Json_get_string ( mqtt_local_message, "mqtt_topic_lvl2" ) );
             Dls_data_AI_set_from_thread_ai ( mqtt_local_message );
           }
          else if (Mqtt_topic_is ( mqtt_local_message, 2, "SET_DI", "+" ))
           { Json_add_string ( mqtt_local_message, "agent_tech_id", Json_get_string ( mqtt_local_message, "mqtt_topic_lvl1" ) );
             Json_add_string ( mqtt_local_message, "agent_acronyme", Json_get_string ( mqtt_local_message, "mqtt_topic_lvl2" ) );
             Dls_data_DI_set_from_thread_di ( mqtt_local_message );
           }
          else if (Mqtt_topic_is ( mqtt_local_message, 2, "SET_WATCHDOG", Agent->agent_tech_id ))
           { Json_add_string ( mqtt_local_message, "agent_tech_id", Json_get_string ( mqtt_local_message, "mqtt_topic_lvl1" ) );
             Json_add_string ( mqtt_local_message, "agent_acronyme", Json_get_string ( mqtt_local_message, "mqtt_topic_lvl2" ) );
             Dls_data_WATCHDOG_set_from_thread_watchdog ( mqtt_local_message );
           }
          Json_unref ( mqtt_local_message );
        }
/****************************************************** Ecoute de l'api *******************************************************/
       JsonNode *mqtt_api_message;
       while ( (mqtt_api_message = Agent_get_mqtt_api_message ( Agent ) ) != NULL )
        { gchar chaine[256];
          gchar *target = Json_get_string ( mqtt_api_message, "mqtt_topic_lvl2" );
          gchar *classe = Json_get_string ( mqtt_api_message, "agent_classe" );
/*------------------------------------------------------------ Stop ----------------------------------------------------------*/
          if ( Mqtt_topic_is ( mqtt_api_message, 4, "+", "AGENT", "+", "STOP" ) )
           { if(classe)
              { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to STOP %s (class %s)", target, classe );
                if (g_strcmp0 ( target, Agent->agent_tech_id ) == 0)
                 { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR,
                         "Cannot self stop this agent-server. keep Running." );
                 }
                else
                 { g_snprintf ( chaine, sizeof(chaine), "abls-agent-%s@%s", classe, target );
                   Thread_shell_queue ( Agent, "AGENT-DISABLE", "sudo -n systemctl disable --now %s", chaine, NULL );
                 }
              }
             else
              { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to STOP %s, but classe not provided", target );
              }
           }
/*------------------------------------------------------------ Start ---------------------------------------------------------*/
          else if ( Mqtt_topic_is ( mqtt_api_message, 4, "+", "AGENT", "+", "START" ) )
           { if(classe) Start_one_agent ( classe, target );
             else
              { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to START %s, but classe not provided", target );
              }
           }
/*------------------------------------------------------------ Restart -------------------------------------------------------*/
          else if ( Mqtt_topic_is ( mqtt_api_message, 4, "+", "AGENT", "+", "RESTART" ) )
           { if(classe)
              { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to RESTART %s (class %s)", target, classe );
                if (g_strcmp0 ( target, Agent->agent_tech_id ) == 0)
                 { Agent->Agent_run = AGENT_NEED_TO_RESTART; }
                else
                 { g_snprintf ( chaine, sizeof(chaine), "abls-agent-%s@%s", classe, target );
                   Thread_shell_queue ( Agent, "AGENT-RESTART", "sudo -n systemctl restart %s", chaine );
                 }
               }
             else
              { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to RESTART %s, but classe not provided", target );
              }
           }
/*------------------------------------------------------------ Upgrade -------------------------------------------------------*/
          else if ( Mqtt_topic_is ( mqtt_api_message, 4, "+", "AGENT", "+", "UPGRADE" ) )
           { if(classe)
              { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to UPGRADE %s (class %s)", target, classe );
                g_snprintf ( chaine, sizeof(chaine), "abls-agent-%s", classe );
                Thread_shell_queue ( Agent, "AGENT-UPGRADE", "sudo -n %s %s -y %s", (Agent->is_apt ? "apt" : "dnf"), (Agent->is_apt ? "update" : "upgrade"), chaine );
                if (g_strcmp0 ( target, Agent->agent_tech_id ) != 0)
                 { g_snprintf ( chaine, sizeof(chaine), "abls-agent-%s@%s", classe, target );
                   Thread_shell_queue ( Agent, "AGENT-RESTART", "sudo -n systemctl restart %s", chaine );
                 }
              }
             else
              { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE,
                      "API is asking to UPGRADE %s, but classe not provided", target );
              }
           }
          else if ( Mqtt_topic_is ( mqtt_api_message, 2, "+", "DLS", "REMAP" ) )
           { MAP_Remap(); }
          else if ( Mqtt_topic_is ( mqtt_api_message, 4, "+", "DLS", "+", "RELOAD" ) )
           { gchar *target = Json_get_string ( mqtt_api_message, "mqtt_topic_lvl2" );
             Dls_Reload_un_plugin ( target );
           }
          else if ( Mqtt_topic_is ( mqtt_api_message, 2, "+", "DLS", "RELOAD_HORLOGE_TICK" ) )
           { Dls_Load_horloge_ticks(); }
          else Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE, "API sent unknown command %s", Json_get_string ( mqtt_api_message, "mqtt_topic" ) );
          Json_unref (mqtt_api_message);
        }

/********************************************************** Running DLS *******************************************************/
        if (is_master) Dls_loop();
     }

    if (is_master) Dls_end();
    Agent_end(Agent);
  }
/*----------------------------------------------------------------------------------------------------------------------------*/
