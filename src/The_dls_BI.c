/******************************************************************************************************************************/
/* ABLS-AGENT-DLS/src/The_dls_BI.c        Déclaration des fonctions pour la gestion des booleans                                   */
/* Projet Abls-Habitat version 4.7       Gestion d'habitat                                                24.06.2019 22:07:06 */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * The_dls_BI.c
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
/* Dls_data_BI_create_by_array : Création d'un BI pour le plugin                                                              */
/* Entrée : l'acronyme, le tech_id et le pointeur de raccourci                                                                */
/******************************************************************************************************************************/
 void Dls_data_BI_create_by_array ( JsonArray *array, guint index, JsonNode *element, gpointer user_data )
  { struct DLS_PLUGIN *plugin = user_data;
    gchar *tech_id  = Json_get_string ( element, "tech_id" );
    gchar *acronyme = Json_get_string ( element, "acronyme" );
    struct DLS_BI *bit = g_try_malloc0 ( sizeof(struct DLS_BI) );
    if (!bit)
    { Info( __func__, "dls", tech_id, LOG_ERR, "Memory error for '%s:%s'", tech_id, acronyme );
       return;
     }
    g_snprintf( bit->tech_id,  sizeof(bit->tech_id),  "%s", tech_id );
    g_snprintf( bit->acronyme, sizeof(bit->acronyme), "%s", acronyme );
    g_snprintf( bit->libelle,  sizeof(bit->libelle),  "%s", Json_get_string ( element, "libelle" ) );
    bit->etat = Json_get_bool ( element, "etat" );
    plugin->Dls_data_BI = g_slist_prepend ( plugin->Dls_data_BI, bit );
    Info( __func__, "dls", tech_id, LOG_INFO,
              "Create bit DLS_BI '%s:%s'=%d (%s)", bit->tech_id, bit->acronyme, bit->etat, bit->libelle );
  }
/******************************************************************************************************************************/
/* Dls_data_BI_lookup : Recherche un CH dans les plugins DLS                                                                  */
/* Entrée : l'acronyme, le tech_id et le pointeur de raccourci                                                                */
/******************************************************************************************************************************/
 struct DLS_BI *Dls_data_BI_lookup ( gchar *tech_id, gchar *acronyme )
  { if (!(tech_id && acronyme)) return(NULL);
    GSList *plugins = Agent_vars->Dls_plugins;
    while (plugins)
     { struct DLS_PLUGIN *plugin = plugins->data;
       if (!strcasecmp( plugin->tech_id, tech_id ))
        { GSList *liste = plugin->Dls_data_BI;
          while (liste)
           { struct DLS_BI *bit = liste->data;
             if ( !strcasecmp ( bit->acronyme, acronyme ) ) return(bit);
             liste = g_slist_next(liste);
           }
        }
       plugins = g_slist_next(plugins);
     }
    return(NULL);
  }
/******************************************************************************************************************************/
/* Dls_data_BI_set: Positionne un bistable                                                                                    */
/* Sortie : TRUE sur le boolean est UP                                                                                        */
/******************************************************************************************************************************/
 void Dls_data_BI_set ( struct DLS_PLUGIN *plugin, struct DLS_BI *bi, gboolean valeur )
  { if (!bi) return;

    if (bi->etat != valeur)
    { Info( __func__, "dls", bi->tech_id, LOG_DEBUG,
                 "ligne %04d: Changing DLS_BI '%s:%s'=%d up %d down %d",
                 (plugin ? plugin->num_ligne : -1), bi->tech_id, bi->acronyme, valeur, bi->edge_up, bi->edge_down );
       bi->etat = valeur;
       if (bi->etat == TRUE)
        { Agent_vars->Set_Dls_BI_Edge_up   = g_slist_prepend ( Agent_vars->Set_Dls_BI_Edge_up, bi ); }
       else
        { Agent_vars->Set_Dls_BI_Edge_down = g_slist_prepend ( Agent_vars->Set_Dls_BI_Edge_down, bi ); }
       if (plugin && plugin->debug) Dls_BI_report_to_API ( bi );                                       /* Si debug, envoi a l'API */
       Agent_vars->audit_bit_interne_per_sec++;
     }
  }
/******************************************************************************************************************************/
/* Dls_data_BI_get: Remonte l'etat d'un bistable                                                                              */
/* Sortie : TRUE sur le boolean est UP                                                                                        */
/******************************************************************************************************************************/
 gboolean Dls_data_BI_get ( struct DLS_BI *bi )
  { if (!bi) return(FALSE);
    return( bi->etat );
  }
/******************************************************************************************************************************/
/* Dls_data_BI_get_up: Remonte le front montant d'un boolean                                                                  */
/* Sortie : TRUE sur le boolean vient de passer à UP                                                                          */
/******************************************************************************************************************************/
 gboolean Dls_data_BI_get_up ( struct DLS_BI *bi )
  { if (!bi) return(FALSE);
    return( bi->edge_up );
  }
/******************************************************************************************************************************/
/* Dls_data_BI_get_down: Remonte le front descendant d'un boolean                                                             */
/* Sortie : TRUE sur le boolean vient de passer à DOWN                                                                        */
/******************************************************************************************************************************/
 gboolean Dls_data_BI_get_down ( struct DLS_BI *bi )
  { if (!bi) return(FALSE);
    return( bi->edge_down );
  }
/******************************************************************************************************************************/
/* Dls_BI_report_to_API : Formate un bit au format JSON                                                                       */
/* Entrées: le bit                                                                                                            */
/* Sortie : le JSON                                                                                                           */
/******************************************************************************************************************************/
 void Dls_BI_report_to_API ( struct DLS_BI *bit )
  { JsonNode *element = Json_create ();
    if (element)
     { Json_add_bool ( element, "etat", bit->etat );
       Agent_send_mqtt_api_message ( Agent, element, TRUE, "DLS_REPORT/BI/%s/%s", bit->tech_id, bit->acronyme );
       Json_unref    ( element );
     }
  }
/******************************************************************************************************************************/
/* Dls_all_BI_to_json: Transforme tous les bits en JSON                                                                       */
/* Entrée: target                                                                                                             */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 void Dls_all_BI_to_json ( gpointer array, struct DLS_PLUGIN *plugin )
  { JsonArray *RootArray = array;
    GSList *liste = plugin->Dls_data_BI;
    while ( liste )
     { struct DLS_BI *bit = liste->data;
       JsonNode *element = Json_create();
       Json_add_string ( element, "tech_id",  bit->tech_id );
       Json_add_string ( element, "acronyme", bit->acronyme );
       Json_add_bool   ( element, "etat",     bit->etat );
       Json_array_add_element ( RootArray, element );
       liste = g_slist_next(liste);
     }
  }
/*----------------------------------------------------------------------------------------------------------------------------*/
