/******************************************************************************************************************************/
/* ABLS-AGENT-DLS/The_dls_AI.c  Gestion des Analog Input                                                                      */
/* Projet Abls-Habitat version 4.7       Gestion d'habitat                                                30.01.2022 14:07:24 */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * The_dls_AI.c
 * This file is part of Abls-Habitat
 *
 * Copyright (C) 1988-2026 - Sébastien LEFÈVRE
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

 #include "dls.h"

/******************************************************************************************************************************/
/* Dls_data_AI_create_by_array : Création d'un AI pour le plugin                                                              */
/* Entrée : l'acronyme, le tech_id et le pointeur de raccourci                                                                */
/******************************************************************************************************************************/
 void Dls_data_AI_create_by_array ( JsonArray *array, guint index, JsonNode *element, gpointer user_data )
  { struct DLS_PLUGIN *plugin = user_data;
    gchar *tech_id  = Json_get_string ( element, "tech_id" );
    gchar *acronyme = Json_get_string ( element, "acronyme" );
    struct DLS_AI *bit = g_try_malloc0 ( sizeof(struct DLS_AI) );
    if (!bit)
     { Info( __func__, "dls", tech_id, LOG_ERR, "Memory error for '%s:%s'", tech_id, acronyme );
       return;
     }
    g_snprintf( bit->tech_id,  sizeof(bit->tech_id),  "%s", tech_id );
    g_snprintf( bit->acronyme, sizeof(bit->acronyme), "%s", acronyme );
    g_snprintf( bit->libelle,  sizeof(bit->libelle),  "%s", Json_get_string ( element, "libelle" ) );
    g_snprintf( bit->unite,    sizeof(bit->unite),    "%s", Json_get_string ( element, "unite" ) );
    bit->archivage = Json_get_int    ( element, "archivage" );
    bit->valeur    = Json_get_double ( element, "valeur"    );
    bit->in_range  = Json_get_bool   ( element, "in_range"  );
    plugin->Dls_data_AI = g_slist_prepend ( plugin->Dls_data_AI, bit );
    Info( __func__, "dls", tech_id, LOG_INFO,
          "Create bit DLS_AI '%s:%s'=%f %s (%s) archivage=%d",
          bit->tech_id, bit->acronyme, bit->valeur, bit->unite, bit->libelle, bit->archivage );
  }
/******************************************************************************************************************************/
/* Dls_data_AI_lookup : Recherche un AI dans les plugins DLS                                                                  */
/* Entrée : l'acronyme, le tech_id et le pointeur de raccourci                                                                */
/******************************************************************************************************************************/
 struct DLS_AI *Dls_data_AI_lookup ( gchar *tech_id, gchar *acronyme )
  { if (!(tech_id && acronyme)) return(NULL);
    GSList *plugins = Agent_vars->Dls_plugins;
    while (plugins)
     { struct DLS_PLUGIN *plugin = plugins->data;
       if (!strcasecmp( plugin->tech_id, tech_id ))
        { GSList *liste = plugin->Dls_data_AI;
          while (liste)
           { struct DLS_AI *bit = liste->data;
             if ( !strcasecmp ( bit->acronyme, acronyme ) ) return(bit);
             liste = g_slist_next(liste);
           }
        }
       plugins = g_slist_next(plugins);
     }
    return(NULL);
  }
/******************************************************************************************************************************/
/* Dls_data_AI_get : Recupere la valeur de l'EA en parametre                                                                  */
/* Entrée : l'acronyme, le tech_id et le pointeur de raccourci                                                                */
/******************************************************************************************************************************/
 gdouble Dls_data_AI_get ( struct DLS_AI *bit )
  { if (!bit) return(0.0);
    return( bit->valeur );
  }
/******************************************************************************************************************************/
/* Dls_data_AI_get : Recupere la valeur de l'EA en parametre                                                                  */
/* Entrée : l'acronyme, le tech_id et le pointeur de raccourci                                                                */
/******************************************************************************************************************************/
 gboolean Dls_data_AI_get_inrange ( struct DLS_AI *bit )
  { if (!bit) return(FALSE);
    return( bit->in_range );
  }
/******************************************************************************************************************************/
/* Met à jour l'entrée analogique num à partir de sa valeur avant mise a l'echelle                                            */
/* Sortie : Néant                                                                                                             */
/******************************************************************************************************************************/
 void Dls_data_AI_set ( struct DLS_AI *bit, gdouble valeur, gboolean in_range )
  { if (!bit) return;
    bit->valeur   = valeur;
    bit->in_range = in_range;
    Info( __func__, "dls", bit->tech_id, LOG_DEBUG,
              "Changing DLS_AI '%s:%s'=%f %s", bit->tech_id, bit->acronyme, bit->valeur, bit->unite );
    Dls_AI_report_to_API ( bit );                                                                            /* envoi a l'API */
  }
/******************************************************************************************************************************/
/* Dls_data_AI_set_from_thread_ai: Positionne une AI dans DLS depuis une AI 'thread'                                          */
/* Entrées: la structure JSON                                                                                                 */
/* Sortie : TRUE si OK, sinon FALSE                                                                                           */
/******************************************************************************************************************************/
 gboolean Dls_data_AI_set_from_thread_ai ( JsonNode *request )
  { if (! (Json_has_member ( request, "agent_tech_id" ) && Json_has_member ( request, "agent_acronyme" ) &&
           Json_has_member ( request, "valeur" ) && Json_has_member ( request, "in_range" )
          )
       ) return(FALSE);

    gchar *agent_tech_id  = Json_get_string ( request, "agent_tech_id" );
    gchar *agent_acronyme = Json_get_string ( request, "agent_acronyme" );
    gchar *tech_id        = agent_tech_id;
    gchar *acronyme       = agent_acronyme;

    if (MAP_to_local ( request ) )
     { tech_id  = Json_get_string ( request, "tech_id" );
       acronyme = Json_get_string ( request, "acronyme" );
     }

    struct DLS_AI *bit = Dls_data_AI_lookup ( tech_id, acronyme );
    if (!bit)
     { Info( __func__, "mqtt", "local", LOG_DEBUG, "SET_AI '%s:%s'/'%s:%s' not found",
                 agent_tech_id, agent_acronyme, tech_id, acronyme );
       return(FALSE);
     }

    Info( __func__, "mqtt", "local", LOG_INFO, "SET_AI '%s:%s'/'%s:%s'=%f %s (range=%d) (%s)",
              agent_tech_id, agent_acronyme, tech_id, acronyme,
              Json_get_double ( request, "valeur" ), bit->unite,
              Json_get_bool ( request, "in_range" ), bit->libelle );
    Dls_data_AI_set ( bit, Json_get_double ( request, "valeur" ), Json_get_bool ( request, "in_range" ) );
    return(TRUE);
  }
/******************************************************************************************************************************/
/* Dls_AI_to_json : Formate un bit au format JSON                                                                             */
/* Entrées: le JsonNode et le bit                                                                                             */
/* Sortie : néant                                                                                                             */
/******************************************************************************************************************************/
 void Dls_AI_to_json ( JsonNode *element, struct DLS_AI *bit )
  { Json_add_string ( element, "classe",    "AI" );
    Json_add_string ( element, "tech_id",   bit->tech_id );
    Json_add_string ( element, "acronyme",  bit->acronyme );
    Json_add_double ( element, "valeur",    bit->valeur );
    Json_add_string ( element, "unite",     bit->unite );
    Json_add_bool   ( element, "in_range",  bit->in_range );
    Json_add_int    ( element, "archivage", bit->archivage );
    Json_add_string ( element, "libelle",   bit->libelle );
  }
/******************************************************************************************************************************/
/* Dls_all_AI_to_json: Transforme tous les bits en JSON                                                                       */
/* Entrée: target                                                                                                             */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 void Dls_all_AI_to_json ( gpointer array, struct DLS_PLUGIN *plugin )
  { JsonArray *RootArray = array;
    GSList *liste = plugin->Dls_data_AI;
    while ( liste )
     { struct DLS_AI *bit = liste->data;
       JsonNode *element = Json_create();
       Dls_AI_to_json ( element, bit );
       Json_array_add_element ( RootArray, element );
       liste = g_slist_next(liste);
     }
  }
/******************************************************************************************************************************/
/* Dls_AI_report_to_API : Formate un bit au format JSON                                                                       */
/* Entrées: le JsonNode et le bit                                                                                             */
/* Sortie : néant                                                                                                             */
/******************************************************************************************************************************/
 void Dls_AI_report_to_API ( struct DLS_AI *bit )
  { JsonNode *element = Json_create ();
    if (element && bit)
     { Json_add_double ( element, "valeur",   bit->valeur );
       Json_add_bool   ( element, "in_range", bit->in_range );
       Agent_send_mqtt_api_message ( Agent, element, TRUE, "DLS_REPORT/AI/%s/%s", bit->tech_id, bit->acronyme );
       Json_unref      ( element );
     }
  }
/*----------------------------------------------------------------------------------------------------------------------------*/
