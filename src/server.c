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

 #include <abls-agent-libs/abls-agent-libs.h>
 #include <pwd.h>
 #include <systemd/sd-login.h>
 struct ABLS_AGENT *Agent = NULL;                                                                     /* Structure de l'agent */

/******************************************************************************************************************************/
/* Agent_get_active_session: appelé lorsque l'agent doit être arrêté                                                          */
/* Entrée: néant                                                                                                              */
/* Sortie: la structure passwd du user connecté, le cas échéant                                                               */
/******************************************************************************************************************************/
 static struct passwd *Agent_get_active_session ( void )
  { gchar *session;
    uid_t active_session;
    if (sd_seat_get_active( "seat0", &session, &active_session) < 0) return(NULL);
    g_free(session);
    struct passwd *pwd = getpwuid ( active_session );
    return(pwd);
  }
/******************************************************************************************************************************/
/* Agent_stop_thread: appelé lorsque l'agent doit être arrêté                                                                 */
/* Entrée: La structure afférente                                                                                             */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 static gpointer Agent_stop_thread ( gpointer user_data )
  { JsonNode *mqtt_api_message = user_data;
    Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE, "Stopping one agent." );
    if (!mqtt_api_message)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Error, mqtt_api_message is NULL" );
       return(NULL);
     }

    gchar *agent_classe  = Json_get_string ( mqtt_api_message, "agent_classe" );
    gchar *agent_tech_id = Json_get_string ( mqtt_api_message, "mqtt_topic_lvl4" );
    if (agent_classe == NULL || agent_tech_id == NULL)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR,
             "Error, agent_classe or agent_tech_id is missing in mqtt_api_message" );
       Json_unref ( mqtt_api_message );
       return(NULL);
     }
    gchar *description = Json_get_string ( mqtt_api_message, "description" );
    if (!description) description = "";

    gpointer agent_status = Agent_status_push ( Agent, "Stopping %s@%s", agent_classe, agent_tech_id );
    if ( g_strcmp0 ( agent_tech_id, Agent_get_tech_id ( Agent ) ) == 0 )                               /* Arret du server lui même ? */
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_CRIT,
             "Server is shuting down. It can only be restarted manually." );
       Run_shell ( "sudo -n systemctl disable abls-agent-server" );
       Run_shell_detached ( "sudo -n systemctl stop abls-agent-server" );
     }
    else if ( g_strcmp0 ( agent_classe, "audio" ) == 0 )                            /* Pour les agents en mode systemd --user */
     { struct passwd *pwd = Agent_get_active_session ();
       if (!pwd)
        { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE,
                "Active session: No active session found. Cannot stop %s (class %s): %s", agent_tech_id, agent_classe, description );
          return(NULL);
        }
       Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE,
             "Active session found name = '%s' for user id '%d'", pwd->pw_name, pwd->pw_uid );
       Run_shell ( "env XDG_RUNTIME_DIR=/run/user/%d DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/%d/bus "
                   "sudo -n -E -u %s systemctl --user disable abls-agent-%s@%s",
                    pwd->pw_uid, pwd->pw_uid, pwd->pw_name, agent_classe, agent_tech_id );
       Run_shell ( "env XDG_RUNTIME_DIR=/run/user/%d DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/%d/bus "
                   "sudo -n -E -u %s systemctl --user stop abls-agent-%s@%s",
                    pwd->pw_uid, pwd->pw_uid, pwd->pw_name, agent_classe, agent_tech_id );
     }
    else                                                              /* Pour tous les autres agents en mode systemctl direct */
     { Run_shell ( "sudo -n systemctl disable abls-agent-%s@%s", agent_classe, agent_tech_id );
       Run_shell ( "sudo -n systemctl stop    abls-agent-%s@%s", agent_classe, agent_tech_id );
     }
    Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE,
          "Agent '%s' (class '%s') stopped", agent_tech_id, agent_classe );
    Json_unref ( mqtt_api_message );
    Agent_status_pop ( Agent, agent_status );
    return(NULL);
  }
/******************************************************************************************************************************/
/* Agent_restart_thread: redémarre l'agent                                                                                    */
/* Entrée: La structure afférente                                                                                             */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 static gpointer Agent_restart_thread ( gpointer user_data )
  { JsonNode *mqtt_api_message = user_data;
    Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE, "Restarting one agent." );
    if (!mqtt_api_message)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Error, mqtt_api_message is NULL" );
       return(NULL);
     }

    gchar *agent_classe  = Json_get_string ( mqtt_api_message, "agent_classe" );
    gchar *agent_tech_id = Json_get_string ( mqtt_api_message, "mqtt_topic_lvl4" );
    if (agent_classe == NULL || agent_tech_id == NULL)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR,
             "Error, agent_classe or agent_tech_id is missing in mqtt_api_message" );
       Json_unref ( mqtt_api_message );
       return(NULL);
     }
    gchar *description = Json_get_string ( mqtt_api_message, "description" );
    if (!description) description = "";

    gpointer agent_status = Agent_status_push ( Agent, "Restarting %s@%s", agent_classe, agent_tech_id );
    if ( g_strcmp0 ( agent_tech_id, Agent_get_tech_id ( Agent ) ) == 0 )                               /* Arret du server lui même ? */
     { Run_shell ( "sudo -n systemctl enable abls-agent-server" );
       Run_shell_detached ( "sudo -n systemctl restart abls-agent-server" );
     }
    else if ( g_strcmp0 ( agent_classe, "audio" ) == 0 )                            /* Pour les agents en mode systemd --user */
     { struct passwd *pwd = Agent_get_active_session ();
       if (!pwd)
        { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE,
                "Active session: No active session found. Cannot restart %s (class %s): %s", agent_tech_id, agent_classe, description );
          return(NULL);
        }
       Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE,
             "Active session found name = '%s' for user id '%d'", pwd->pw_name, pwd->pw_uid );
       Run_shell ( "env XDG_RUNTIME_DIR=/run/user/%d DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/%d/bus "
                   "sudo -n -E -u %s systemctl --user enable abls-agent-%s@%s",
                    pwd->pw_uid, pwd->pw_uid, pwd->pw_name, agent_classe, agent_tech_id );
       Run_shell ( "env XDG_RUNTIME_DIR=/run/user/%d DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/%d/bus "
                   "sudo -n -E -u %s systemctl --user restart abls-agent-%s@%s",
                    pwd->pw_uid, pwd->pw_uid, pwd->pw_name, agent_classe, agent_tech_id );
     }
    else                                                              /* Pour tous les autres agents en mode systemctl direct */
     { Run_shell ( "sudo -n systemctl enable abls-agent-%s@%s", agent_classe, agent_tech_id );
       Run_shell ( "sudo -n systemctl restart abls-agent-%s@%s", agent_classe, agent_tech_id );
     }
    Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE,
          "Agent '%s' (class '%s') restarted", agent_tech_id, agent_classe );
    Json_unref ( mqtt_api_message );
    Agent_status_pop ( Agent, agent_status );
    return(NULL);
  }
/******************************************************************************************************************************/
/* Agent_upgrade_thread: appelé pour redemarrer un agent                                                                      */
/* Entrée: La structure afférente                                                                                             */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 static gpointer Agent_upgrade_thread ( gpointer user_data )
  { JsonNode *mqtt_api_message = user_data;
    Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE, "Upgrade one agent." );
    if (!mqtt_api_message)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Error, mqtt_api_message is NULL" );
       return(NULL);
     }

    gchar *agent_classe  = Json_get_string ( mqtt_api_message, "agent_classe" );
    gchar *agent_tech_id = Json_get_string ( mqtt_api_message, "mqtt_topic_lvl4" );
    if (agent_classe == NULL || agent_tech_id == NULL)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR,
             "Error, agent_classe or agent_tech_id is missing in mqtt_api_message" );
       Json_unref ( mqtt_api_message );
       return(NULL);
     }
    gchar *description = Json_get_string ( mqtt_api_message, "description" );
    if (!description) description = "";

    gpointer agent_status = Agent_status_push ( Agent, "Upgrading %s@%s", agent_classe, agent_tech_id );
    if (Agent_is_apt ( Agent ))
     { Run_shell ( "sudo -n apt update" );
       if ( g_strcmp0 ( agent_tech_id, Agent_get_tech_id ( Agent ) ) == 0 )                            /* Arret du server lui même ? */
            { Run_shell ( "sudo -n apt upgrade -y abls-agent-server" ); }
       else { Run_shell ( "sudo -n apt upgrade -y abls-agent-%s", agent_classe ); }
     }
    else
     { if ( g_strcmp0 ( agent_tech_id, Agent_get_tech_id ( Agent ) ) == 0 )                            /* Arret du server lui même ? */
            { Run_shell ( "sudo -n dnf upgrade abls-agent-server" ); }
       else { Run_shell ( "sudo -n dnf upgrade abls-agent-%s", agent_classe ); }
     }
    Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE,
          "Agent '%s' (class '%s') upgraded, Restarting.", agent_tech_id, agent_classe );
    Agent_restart_thread ( user_data );
    Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE,
          "Agent '%s' (class '%s') upgraded and restarted", agent_tech_id, agent_classe );
    Json_unref ( mqtt_api_message );
    Agent_status_pop ( Agent, agent_status );
    return(NULL);
  }
/******************************************************************************************************************************/
/* Start_one_agent: Installe et demarre un agent classe/tech_id en parametre                                                  */
/* Entrée: agent, agent_classe, agent_tech_id                                                                                 */
/* Sortie: aucune                                                                                                             */
/******************************************************************************************************************************/
 static void Agent_start ( gchar *agent_classe, gchar *agent_tech_id, gchar *description )
  { if (!agent_classe || !agent_tech_id)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Error, agent_classe or agent_tech_id is missing" );
       return;
     }
    if (!description) description = "";

    if (g_strcmp0 ( agent_tech_id, Agent_get_tech_id ( Agent ) ) == 0) return;/* On ne peut pas demarrer l'agent server sur lui-meme */

    Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE, "Starting %s (class %s): %s",
          agent_tech_id, agent_classe, description );

    gchar chaine[256];
    g_snprintf ( chaine, sizeof(chaine), "abls-agent-%s", agent_classe );

    gpointer agent_status = Agent_status_push ( Agent, "Starting %s@%s", agent_classe, agent_tech_id );

    gchar *path = g_find_program_in_path(chaine);
    if (!path)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE, "package '%s' not found. Install in progress.", chaine );
       Run_shell ( "sudo -n %s install -y abls-agent-%s", (Agent_is_apt ( Agent ) ? "apt" : "dnf"), agent_classe );
     } else g_free(path);

    if ( g_strcmp0 ( agent_classe, "audio" ) == 0 )                                 /* Pour les agents en mode systemd --user */
     { struct passwd *pwd = Agent_get_active_session ();
       if (!pwd)
        { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE,
                "Active session: No active session found. Cannot start %s (class %s): %s", agent_tech_id, agent_classe, description );
          return;
        }
       Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE,
             "Active session found name = '%s' for user id '%d'", pwd->pw_name, pwd->pw_uid );
       Run_shell ( "env XDG_RUNTIME_DIR=/run/user/%d DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/%d/bus "
                   "sudo -n -E -u %s systemctl --user enable abls-agent-%s@%s",
                    pwd->pw_uid, pwd->pw_uid, pwd->pw_name, agent_classe, agent_tech_id );
       Run_shell ( "env XDG_RUNTIME_DIR=/run/user/%d DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/%d/bus "
                   "sudo -n -E -u %s systemctl --user start abls-agent-%s@%s",
                    pwd->pw_uid, pwd->pw_uid, pwd->pw_name, agent_classe, agent_tech_id );
     }
    else                                                                                         /* Agent sans session active */
     { Run_shell ( "sudo -n systemctl enable abls-agent-%s@%s", agent_classe, agent_tech_id );
       Run_shell ( "sudo -n systemctl start abls-agent-%s@%s", agent_classe, agent_tech_id );
     }
    Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE, "Agent '%s' (class '%s') '%s' is starting",
          agent_tech_id, agent_classe, description );
    Agent_status_pop ( Agent, agent_status );
  }
/******************************************************************************************************************************/
/* Start_one_agent_by_api_message_thread: Lance un agent depuis une demande de l'API                                          */
/* Entrée: le message api et la description de l'agent                                                                        */
/* Sortie: aucune                                                                                                             */
/******************************************************************************************************************************/
 static gpointer Agent_start_thread ( gpointer user_data )
  { JsonNode *mqtt_api_message = user_data;
    if (!mqtt_api_message)
     { Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_ERR, "Error, mqtt_api_message is NULL" );
       return(NULL);
     }
    Agent_start ( Json_get_string ( mqtt_api_message, "agent_classe" ),
                  Json_get_string ( mqtt_api_message, "mqtt_topic_lvl4" ),
                  Json_get_string ( mqtt_api_message, "description" ) );
    Json_unref ( mqtt_api_message );
    return(NULL);
  }
/******************************************************************************************************************************/
/* Start_agents_by_array: Installe et demarre les agents contenus dans un tableau JSON                                        */
/* Entrée: array, index, element, user_data                                                                                   */
/* Sortie: aucune                                                                                                             */
/******************************************************************************************************************************/
 static void Agent_Start_by_array ( JsonArray *array, guint index, JsonNode *element, gpointer user_data )
  { if (!array || !element || !user_data) return;
    gchar *agent_classe  = Json_get_string ( element, "agent_classe" );
    gchar *agent_tech_id = Json_get_string ( element, "agent_tech_id" );
    gchar *description   = Json_get_string ( element, "description" );
    Agent_start ( agent_classe, agent_tech_id, description );
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
    Agent = Agent_init ( argv[0], "server", ABLS_AGENT_SERVER_VERSION, 0, argc, argv );

                                                                                  /* Pour installer les agents sur le serveur */
    Agent_subscribe_mqtt_api ( Agent, "%s/AGENT/%s/START/+",   Agent_get_domain_uuid ( Agent ), Agent_get_tech_id ( Agent ) );
    Agent_subscribe_mqtt_api ( Agent, "%s/AGENT/%s/UPGRADE/+", Agent_get_domain_uuid ( Agent ), Agent_get_tech_id ( Agent ) );
    Agent_subscribe_mqtt_api ( Agent, "%s/AGENT/%s/RESTART/+", Agent_get_domain_uuid ( Agent ), Agent_get_tech_id ( Agent ) );
    Agent_subscribe_mqtt_api ( Agent, "%s/AGENT/%s/STOP/+",    Agent_get_domain_uuid ( Agent ), Agent_get_tech_id ( Agent ) );

    Agent_is_ready ( Agent );


    Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE,       /* Demarrage des agents locaux */
          "Starting %d local_agents", Agent_config_get_array_length ( Agent, "local_agents" ) );
    Agent_config_foreach_array_element ( Agent, "local_agents", Agent_Start_by_array, NULL );

    while(Agent_is_running ( Agent ))                                                        /* On tourne tant que necessaire */
     { Agent_loop ( Agent );                                             /* Loop sur l'agent pour mettre a jour la telemetrie */
/****************************************************** Ecoute de l'api *******************************************************/
       JsonNode *mqtt_api_message;
       while ( (mqtt_api_message = Agent_get_mqtt_api_message ( Agent ) ) != NULL )
        { if ( Mqtt_topic_is ( mqtt_api_message, 4, "+", "AGENT", Agent_get_tech_id ( Agent ), "TEST" ) )
           { Info(__func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE, "Agent Test from API."); }
/*------------------------------------------------------------ Start ---------------------------------------------------------*/
          else if ( Mqtt_topic_is ( mqtt_api_message, 5, "+", "AGENT", Agent_get_tech_id ( Agent ), "START", "+" ) )
           { Json_ref ( mqtt_api_message );
             Run_thread_detached ( "Start one agent", Agent_start_thread, mqtt_api_message );
           }
          else if ( Mqtt_topic_is ( mqtt_api_message, 5, "+", "AGENT", Agent_get_tech_id ( Agent ), "STOP", "+" ) )
           { Json_ref ( mqtt_api_message );
             Run_thread_detached ( "Stopping agent", Agent_stop_thread, mqtt_api_message );
           }
          else if ( Mqtt_topic_is ( mqtt_api_message, 5, "+", "AGENT", Agent_get_tech_id ( Agent ), "RESTART", "+" ) )
           { Json_ref ( mqtt_api_message );
             Run_thread_detached ( "Restarting agent", Agent_restart_thread, mqtt_api_message );
           }
          else if ( Mqtt_topic_is ( mqtt_api_message, 5, "+", "AGENT", Agent_get_tech_id ( Agent ), "UPGRADE", "+" ) )
           { Json_ref ( mqtt_api_message );
             Run_thread_detached ( "Upgrading agent", Agent_upgrade_thread, mqtt_api_message );
           }
          else Info( __func__, Agent_get_classe ( Agent ), Agent_get_tech_id ( Agent ), LOG_NOTICE, "API sent unknown command %s", Json_get_string ( mqtt_api_message, "mqtt_topic" ) );
          Json_unref (mqtt_api_message);
        }
     }

    Agent_end(Agent);
  }
/*----------------------------------------------------------------------------------------------------------------------------*/
