/******************************************************************************************************************************/
/* ABLS-AGENT-DLS/map.c     Gestion des mappings pour l'agent DLS                                                             */
/* Projet Abls-Habitat                   Gestion d'habitat                                                01.08.2026 12:00:00 */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * dls_compat.c
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

 #include <stdarg.h>

 #include "dls.h"


 static JsonNode *Tree_root = NULL;                                                           /* Racine de l'arbre de mapping */
 static GTree    *Tree_to_local = NULL;                                                    /* Arbre de mapping agent -> local */
 static GRWLock   Tree_to_local_lock;                             /* Verrou de synchro pour l'arbre de mapping agent -> local */
 static GTree    *Tree_to_agent = NULL;                                                    /* Arbre de mapping local -> agent */
 static GRWLock   Tree_to_agent_lock;                             /* Verrou de synchro pour l'arbre de mapping local -> agent */

/******************************************************************************************************************************/
/* MAP_Comparer_clef_agent: Compare deux noeuds JSON pour le tri par agent_tech_id et agent_acronyme                          */
/* Entree: node1 - premier noeud JSON a comparer                                                                              */
/*         node2 - deuxieme noeud JSON a comparer                                                                             */
/* Sortie: -1, 0 ou 1 selon la comparaison                                                                                    */
/******************************************************************************************************************************/
 static gint MAP_Comparer_clef_agent ( gconstpointer a, gconstpointer b )
  { JsonNode *node1 = (JsonNode *)a;
    JsonNode *node2 = (JsonNode *)b;
    if (!node1) return(-1);
    if (!node2) return(1);
    gchar *agent_tech_id_1 = Json_get_string ( node1, "agent_tech_id" );
    gchar *agent_tech_id_2 = Json_get_string ( node2, "agent_tech_id" );
    if (!agent_tech_id_1) return(-1);
    if (!agent_tech_id_2) return(1);

    gint result = strcasecmp ( agent_tech_id_1, agent_tech_id_2 );
    if (result) return(result);

    gchar *agent_acronyme_1 = Json_get_string ( node1, "agent_acronyme" );
    gchar *agent_acronyme_2 = Json_get_string ( node2, "agent_acronyme" );
    if (!agent_acronyme_1) return(-1);
    if (!agent_acronyme_2) return(1);
    return ( strcasecmp ( agent_acronyme_1, agent_acronyme_2 ) );
  }
/******************************************************************************************************************************/
/* MAP_Comparer_clef_local: Compare deux noeuds JSON pour le tri par tech_id et acronyme                                      */
/* Entree: node1 - premier noeud JSON a comparer                                                                              */
/*         node2 - deuxieme noeud JSON a comparer                                                                             */
/* Sortie: -1, 0 ou 1 selon la comparaison                                                                                    */
/******************************************************************************************************************************/
 static gint MAP_Comparer_clef_local ( gconstpointer a, gconstpointer b )
  { JsonNode *node1 = (JsonNode *)a;
    JsonNode *node2 = (JsonNode *)b;
    if (!node1) return(-1);
    if (!node2) return(1);
    gchar *tech_id_1 = Json_get_string ( node1, "tech_id" );
    gchar *tech_id_2 = Json_get_string ( node2, "tech_id" );
    if (!tech_id_1) return(-1);
    if (!tech_id_2) return(1);

    gint result = strcasecmp ( tech_id_1, tech_id_2 );
    if (result) return(result);

    gchar *acronyme_1 = Json_get_string ( node1, "acronyme" );
    gchar *acronyme_2 = Json_get_string ( node2, "acronyme" );
    if (!acronyme_1) return(-1);
    if (!acronyme_2) return(1);
    return ( strcasecmp ( acronyme_1, acronyme_2 ) );
  }
/******************************************************************************************************************************/
/* MAP_init: Initialise les elements de gestion des mappings                                                                  */
/* Entree: néant                                                                                                              */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 void MAP_Init ( void )
  { Tree_to_agent = g_tree_new ( MAP_Comparer_clef_local );
    Tree_to_local = g_tree_new ( MAP_Comparer_clef_agent );
    g_rw_lock_init ( &Tree_to_local_lock );
    g_rw_lock_init ( &Tree_to_agent_lock );
  }
/******************************************************************************************************************************/
/* MAP_Clear: Efface les mappings                                                                                             */
/* Entree: neant                                                                                                              */
/* Sortie: neant                                                                                                              */
/******************************************************************************************************************************/
 static void MAP_Clear( void )
  { g_rw_lock_writer_lock ( &Tree_to_agent_lock );
    g_tree_destroy ( Tree_to_agent );
    Tree_to_agent = g_tree_new ( MAP_Comparer_clef_local );
    g_rw_lock_writer_unlock ( &Tree_to_agent_lock );

    g_rw_lock_writer_lock ( &Tree_to_local_lock );
    g_tree_destroy ( Tree_to_local );
    Tree_to_local = g_tree_new ( MAP_Comparer_clef_agent );
    g_rw_lock_writer_unlock ( &Tree_to_local_lock );

    Json_unref ( Tree_root );
    Tree_root = NULL;
  }
/******************************************************************************************************************************/
/* MAP_end: Libère les elements de gestion des mappings                                                                       */
/* Entree: néant                                                                                                              */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 void MAP_End ( void )
  { MAP_Clear();
    g_rw_lock_clear ( &Tree_to_local_lock );
    g_rw_lock_clear ( &Tree_to_agent_lock );
  }
/******************************************************************************************************************************/
/* MAP_from_agent: Mappe un identifiant agent vers un identifiant local                                                       */
/* Entree: key - noeud JSON contenant agent_tech_id et agent_acronyme a mapper                                                */
/* Sortie: TRUE si mapping rejussi, FALSE sinon                                                                               */
/******************************************************************************************************************************/
 gboolean MAP_to_local ( JsonNode *key )
  { if (!key) return(FALSE);
    if (!Json_has_member ( key, "agent_tech_id" ) || !Json_has_member ( key, "agent_acronyme" ) ) return(FALSE);
    if (!Tree_to_local) return(FALSE);

    g_rw_lock_reader_lock ( &Tree_to_local_lock );
    JsonNode *found = g_tree_lookup ( Tree_to_local, key );
    if (found && Json_has_member ( found, "tech_id" ) && Json_has_member ( found, "acronyme" ) )
     { Json_add_string ( key, "tech_id",  Json_get_string ( found, "tech_id" ) );
       Json_add_string ( key, "acronyme", Json_get_string ( found, "acronyme" ) );
     }
    g_rw_lock_reader_unlock ( &Tree_to_local_lock );
    return( (found ? TRUE : FALSE ) );
  }
/******************************************************************************************************************************/
/* MAP_to_agent: Mappe un identifiant local vers un identifiant agent dans les mappings                                        */
/* Entree: key - noeud JSON contenant tech_id et acronyme ou a mapper                                                               */
/* Sortie: TRUE si mapping rejussi, FALSE sinon                                                                                       */
/******************************************************************************************************************************/
 gboolean MAP_to_agent ( JsonNode *key )
  { if (!key) return(FALSE);
    if (!Json_has_member ( key, "tech_id" ) || !Json_has_member ( key, "acronyme" ) ) return(FALSE);
    if (!Tree_to_agent) return(FALSE);

    g_rw_lock_reader_lock ( &Tree_to_agent_lock );
    JsonNode *found = g_tree_lookup ( Tree_to_agent, key );
    if (found && Json_has_member ( found, "tech_id" ) && Json_has_member ( found, "acronyme" ) )
    { Json_add_string ( key, "tech_id", Json_get_string ( found, "tech_id" ) );
      Json_add_string ( key, "acronyme", Json_get_string ( found, "acronyme" ) );
    }
    g_rw_lock_reader_unlock ( &Tree_to_agent_lock );
    return( (found ? TRUE : FALSE ) );
  }
/******************************************************************************************************************************/
/* MAP_Remap: Recharge les mappings depuis l'API globale                                                                      */
/* Entree: neant                                                                                                              */
/* Sortie: neant                                                                                                              */
/******************************************************************************************************************************/
void MAP_Remap( void )
 { MAP_Clear();
   Tree_root = Http_Post_to_global_API ( Agent, "/run/mapping/list", NULL );
   if (Tree_root && Json_get_int ( Tree_root, "http_code" ) == 200)
    { Info ( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE,
             "Remapping with %d maps", Json_get_int ( Tree_root, "nbr_mappings" ) );
      GList *results = json_array_get_elements ( Json_get_array ( Tree_root, "mappings" ) );
      GList *result = results;
      while(result)
       { JsonNode *element = result->data;
         g_rw_lock_writer_lock ( &Tree_to_agent_lock );
         g_tree_insert ( Tree_to_agent, element, element );
         g_rw_lock_writer_unlock ( &Tree_to_agent_lock );
         g_rw_lock_writer_lock ( &Tree_to_local_lock );
         g_tree_insert ( Tree_to_local, element, element );
         g_rw_lock_writer_unlock ( &Tree_to_local_lock );
         result = g_list_next(result);
       }
      g_list_free(results);
    }
   else if (Tree_root)
    { Json_unref ( Tree_root );
      Tree_root = NULL;
    }
 }
/******************************************************************************************************************************/
/* MQTT_Send_archive_to_API_compat: Envoie une archive vers l'API globale via MQTT                                                    */
/* Entree: tech_id - identifiant technique                                                                                              */
/*         acronyme - acronyme de la donnee                                                                                           */
/*         valeur - valeur a archiver                                                                                                  */
/* Sortie: neant                                                                                                                      */
/******************************************************************************************************************************/
void MQTT_Send_archive_to_API_compat ( gchar *tech_id, gchar *acronyme, gdouble valeur )
 { if (!Agent || !tech_id || !acronyme) return;
   JsonNode *payload = Json_create();
   if (!payload) return;
   Json_add_string ( payload, "tech_id", tech_id );
   Json_add_string ( payload, "acronyme", acronyme );
   Json_add_double ( payload, "valeur", valeur );
   Agent_send_mqtt_api_message ( Agent, payload, FALSE, "DLS_ARCHIVE/%s/%s", tech_id, acronyme );
   Json_unref ( payload );
 }
/*----------------------------------------------------------------------------------------------------------------------------*/
