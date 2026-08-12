/******************************************************************************************************************************/
/* ABLS-AGENT-DLS/src/distrib_outputs.c  Distribution des sorties DO et AO vers les agents                                    */
/* Projet Abls-Habitat                   Gestion d'habitat                                                04.08.2026 12:00:00 */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * distrib_outputs.c
 * This file is part of Abls-Habitat
 *
 * Copyright (C) 1988-2026 - Sebastien LEFEVRE
 *
 * ABLS-AGENT-DLS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ABLS-AGENT-DLS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ABLS-AGENT-DLS; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/************************************************************* Includes *******************************************************/
 #include "dls.h"

/******************************************************************************************************************************/
/* Distribuer_outputs: Distribution des sorties DO et AO vers les agents connectes                                            */
/* Entrée: rien                                                                                                               */
/* Sortie: rien                                                                                                               */
/******************************************************************************************************************************/
 void Distribuer_outputs ( void )
  { JsonNode *RootNode;
    guint cpt;

    cpt=0;
    while ( Agent_vars->Liste_DO && cpt < 50 )
     { g_rw_lock_writer_lock ( &Agent_vars->Liste_DO_synchro );
       RootNode = Agent_vars->Liste_DO->data;                                                 /* Recuperation du numero de DO */
       Agent_vars->Liste_DO = g_slist_remove ( Agent_vars->Liste_DO, RootNode );
       g_rw_lock_writer_unlock ( &Agent_vars->Liste_DO_synchro );

       if (MAP_to_agent ( RootNode ))
        { JsonNode *Node = Json_create ();
          if (Node)
           { Json_add_string ( Node, "tech_id",  Json_get_string ( RootNode, "tech_id" ) );
             Json_add_string ( Node, "acronyme", Json_get_string ( RootNode, "acronyme" ) );
             Json_add_bool   ( Node, "etat",     Json_get_bool   ( RootNode, "etat" ) );
             Agent_send_mqtt_api_message ( Agent, Node, TRUE, "SET_DO/%s/%s",
                                      Json_get_string ( RootNode, "agent_tech_id" ),
                                      Json_get_string ( RootNode, "agent_acronyme" ) );
             Json_unref ( Node );
           }
          else Info( __func__, "distrib", Json_get_string ( RootNode, "agent_tech_id" ), LOG_ERR, "'%s:%s': Json node create error",
                         Json_get_string ( RootNode, "agent_tech_id" ),
                         Json_get_string ( RootNode, "agent_acronyme" ) );
        }
       else Info( __func__, "distrib", Json_get_string ( RootNode, "tech_id" ), LOG_NOTICE,
                      "'%s:%s' is not mapped. dropping",
                       Json_get_string ( RootNode, "tech_id" ), Json_get_string ( RootNode, "acronyme" ) );
       Json_unref ( RootNode );
       cpt++;
     }

    cpt=0;
    while ( Agent_vars->Liste_AO && cpt < 50 )
     { g_rw_lock_writer_lock( &Agent_vars->Liste_AO_synchro );                            /* Traitement des AO à distribuer */
       RootNode = Agent_vars->Liste_AO->data;                                                    /* Recuperation du numero de AO */
       Agent_vars->Liste_AO = g_slist_remove ( Agent_vars->Liste_AO, RootNode );
       g_rw_lock_writer_unlock( &Agent_vars->Liste_AO_synchro );                            /* Traitement des AO à distribuer */

       if (MAP_to_agent ( RootNode ))
        { JsonNode *Node = Json_create ();
          if (Node)
           { Json_add_string ( Node, "tech_id",  Json_get_string ( RootNode, "tech_id" ) );
             Json_add_string ( Node, "acronyme", Json_get_string ( RootNode, "acronyme" ) );
             Json_add_double ( Node, "valeur",   Json_get_double ( RootNode, "valeur" ) );
             Agent_send_mqtt_api_message ( Agent, Node, TRUE, "SET_AO/%s/%s",
                                      Json_get_string ( RootNode, "agent_tech_id" ),
                                      Json_get_string ( RootNode, "agent_acronyme" ) );
             Json_unref ( Node );
           }
          else Info( __func__, "distrib", Json_get_string ( RootNode, "agent_tech_id" ), LOG_ERR, "'%s:%s': Json node create error",
                         Json_get_string ( RootNode, "agent_tech_id" ),
                         Json_get_string ( RootNode, "agent_acronyme" ) );
        }
       else Info( __func__, "distrib", Json_get_string ( RootNode, "tech_id" ), LOG_NOTICE,
                      "'%s:%s' is not mapped. dropping",
                       Json_get_string ( RootNode, "tech_id" ), Json_get_string ( RootNode, "acronyme" ) );
       Json_unref ( RootNode );
       cpt++;
     }

  }
/*----------------------------------------------------------------------------------------------------------------------------*/
