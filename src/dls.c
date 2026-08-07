/******************************************************************************************************************************/
/* ABLS-AGENT-DLS/dls.c  Gestion autonome du métier DLS                                                                       */
/* Projet Abls-Habitat                   Gestion d'habitat                                                01.08.2026 12:00:00 */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * dls.c
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

 #include <string.h>

 #include "server.h"

/******************************************************************************************************************************/
/* Dls_init: Initialisation du module DLS                                                                                    */
/* Entrée: rien                                                                                                               */
/* Sortie: rien                                                                                                               */
/******************************************************************************************************************************/
 void Dls_init ( )
  { g_rw_lock_init ( &Agent_vars->Dls_plugins_lock );
    g_rw_lock_init ( &Agent_vars->Liste_DO_synchro );
    g_rw_lock_init ( &Agent_vars->Liste_AO_synchro );
    g_rw_lock_init ( &Agent_vars->Liste_visuel_synchro );
    g_rw_lock_init ( &Agent_vars->Liste_msg_synchro );

    Agent_set_status ( Agent, "Loading mappings..." );
    MAP_Init();
    MAP_Remap();
    Agent_set_status ( Agent, "Loading plugins..." );
    Dls_Importer_plugins();
    Dls_Load_horloge_ticks();

    Agent_vars->next_top_2hz   = Agent->Top;
    Agent_vars->next_top_5hz   = Agent->Top;
    Agent_vars->next_top_1sec  = Agent->Top + 10;
    Agent_vars->next_top_2sec  = Agent->Top + 20;
    Agent_vars->next_top_5sec  = Agent->Top + 50;
    Agent_vars->next_top_10sec = Agent->Top + 100;
    Agent_vars->next_top_1min  = Agent->Top + 600;
    Agent_vars->next_top_10min = Agent->Top + 6000;
    Agent_vars->last_top       = Agent->Top;
  }

/******************************************************************************************************************************/
/* Dls_end: Liberation des ressources du module DLS                                                                          */
/* Entrée: rien                                                                                                               */
/* Sortie: rien                                                                                                               */
/******************************************************************************************************************************/
 void Dls_end ( void )
  { Dls_Decharger_plugins();

    g_rw_lock_clear ( &Agent_vars->Dls_plugins_lock );
    g_rw_lock_clear ( &Agent_vars->Liste_DO_synchro );
    g_rw_lock_clear ( &Agent_vars->Liste_AO_synchro );
    g_rw_lock_clear ( &Agent_vars->Liste_visuel_synchro );
    g_rw_lock_clear ( &Agent_vars->Liste_msg_synchro );

    MAP_End();
  }

/******************************************************************************************************************************/
/* Dls_loop: Boucle principale de traitement DLS                                                                             */
/* Entrée: rien                                                                                                               */
/* Sortie: rien                                                                                                               */
/******************************************************************************************************************************/
 void Dls_loop ( void )
  { Dls_Check_top_horaire ();                                           /* Mise à jour des variables de gestion de l'heure */
    if (Agent->Top>=Agent_vars->next_top_5hz)                                                         /* Toutes les 1/5 secondes */
     { Agent_vars->next_top_5hz = Agent->Top + 2;
       Dls_data_MONO_set ( NULL, Agent_vars->sys_top_5hz, TRUE );
       Dls_data_BI_set   ( NULL, Agent_vars->sys_flipflop_5hz, !Dls_data_BI_get ( Agent_vars->sys_flipflop_5hz) );
     }
    if (Agent->Top>=Agent_vars->next_top_2hz)                                                         /* Toutes les 1/2 secondes */
     { Agent_vars->next_top_2hz = Agent->Top + 5;
       Dls_data_MONO_set ( NULL, Agent_vars->sys_top_2hz, TRUE );
       Dls_data_BI_set   ( NULL, Agent_vars->sys_flipflop_2hz, !Dls_data_BI_get ( Agent_vars->sys_flipflop_2hz) );
     }
    if (Agent->Top>=Agent_vars->next_top_1sec)                                                          /* Toutes les secondes */
     { Agent_vars->next_top_1sec = Agent->Top + 10;
       Dls_data_MONO_set ( NULL, Agent_vars->sys_top_1sec, TRUE );
       Dls_data_BI_set   ( NULL, Agent_vars->sys_flipflop_1sec, !Dls_data_BI_get ( Agent_vars->sys_flipflop_1sec) );

       Agent_vars->audit_bit_interne_per_sec_hold += Agent_vars->audit_bit_interne_per_sec;
       Agent_vars->audit_bit_interne_per_sec_hold = Agent_vars->audit_bit_interne_per_sec_hold >> 1;
       Agent_vars->audit_bit_interne_per_sec = 0;
       Dls_data_AI_set ( Agent_vars->sys_bit_per_sec, (gdouble)Agent_vars->audit_bit_interne_per_sec_hold, TRUE );
     }
    if (Agent->Top>=Agent_vars->next_top_2sec)                                                        /* Toutes les 2 secondes */
     { Agent_vars->next_top_2sec = Agent->Top+20;
       Dls_data_BI_set ( NULL, Agent_vars->sys_flipflop_2sec, !Dls_data_BI_get ( Agent_vars->sys_flipflop_2sec) );
     }
    if (Agent->Top>=Agent_vars->next_top_5sec)                                                        /* Toutes les 5 secondes */
     { Agent_vars->next_top_5sec = Agent->Top + 50;
       Dls_data_MONO_set ( NULL, Agent_vars->sys_top_5sec, TRUE );
     }
    if (Agent->Top>=Agent_vars->next_top_10sec)                                                      /* Toutes les 10 secondes */
     { Agent_vars->next_top_10sec = Agent->Top + 100;
       Dls_data_MONO_set ( NULL, Agent_vars->sys_top_10sec, TRUE );
       Dls_data_BI_set ( NULL, Agent_vars->sys_mqtt_connected, Mqtt_is_connected ( Agent->mqtt_local ) );
     }
    if (Agent->Top>=Agent_vars->next_top_1min)                                                           /* Toutes les minutes */
     { Agent_vars->next_top_1min = Agent->Top + 600;
       Dls_data_MONO_set ( NULL, Agent_vars->sys_top_1min, TRUE );
       Dls_data_activer_horloge();
       Archive_all_thread();
     }
    if (Agent->Top>=Agent_vars->next_top_10min)                                                      /* Toutes les 10 minutes */
     { Agent_vars->next_top_10min = Agent->Top + 6000; }

    Dls_set_edge();
    Dls_set_cde_exterieure();
    Dls_foreach_plugins ( Dls_run_plugin );
    Dls_reset_edge();
    Dls_reset_cde_exterieure();

    Dls_Stop_top_horaire();

    Dls_data_HORLOGE_clear();
    Dls_data_MONO_set ( NULL, Agent_vars->sys_top_5hz,   FALSE );
    Dls_data_MONO_set ( NULL, Agent_vars->sys_top_2hz,   FALSE );
    Dls_data_MONO_set ( NULL, Agent_vars->sys_top_1sec,  FALSE );
    Dls_data_MONO_set ( NULL, Agent_vars->sys_top_5sec,  FALSE );
    Dls_data_MONO_set ( NULL, Agent_vars->sys_top_10sec, FALSE );
    Dls_data_MONO_set ( NULL, Agent_vars->sys_top_1min,  FALSE );

  }

 /******************************************************************************************************************************/
/* Chrono: renvoi la difference de temps entre deux structures timeval                                                        */
/* Entrée: le temps avant, et le temps apres l'action                                                                         */
/* Sortie: un float                                                                                                           */
/******************************************************************************************************************************/
 static float Chrono ( struct timeval *avant, struct timeval *apres )
  { if (!(avant && apres)) return(0.0);
    else return( apres->tv_sec - avant->tv_sec + (apres->tv_usec - avant->tv_usec)/1000000.0 );
  }
/******************************************************************************************************************************/
/* Set_cde_exterieure: Mise à un des bits de commande exterieure                                                              */
/* Entrée: rien                                                                                                               */
/* Sortie: rien                                                                                                               */
/******************************************************************************************************************************/
 void Dls_set_cde_exterieure ( void )
  { while( Agent_vars->Set_Dls_Data )                                                            /* A-t-on une entrée a allumer ?? */
     { struct DLS_DI *di = Agent_vars->Set_Dls_Data->data;
      Info( __func__, "dls", di->tech_id, LOG_NOTICE, "%s: Mise a 1 du bit DI %s:%s",
                 __func__, di->tech_id, di->acronyme );
       Agent_vars->Set_Dls_Data = g_slist_remove ( Agent_vars->Set_Dls_Data, di );
       Agent_vars->Reset_Dls_Data = g_slist_append ( Agent_vars->Reset_Dls_Data, di );
       Dls_data_DI_set ( di, TRUE );                                                             /* Mise a un du bit d'entrée */
     }
  }
/******************************************************************************************************************************/
/* Reset_cde_exterieure: Mise à zero des bits de commande exterieure                                                          */
/* Entrée: rien                                                                                                               */
/* Sortie: rien                                                                                                               */
/******************************************************************************************************************************/
 void Dls_reset_cde_exterieure ( void )
  { while( Agent_vars->Reset_Dls_Data )                                            /* A-t-on un monostable a éteindre ?? */
     { struct DLS_DI *di = Agent_vars->Reset_Dls_Data->data;
      Info( __func__, "dls", di->tech_id, LOG_DEBUG, "%s: Mise a 0 du bit DI %s:%s",
                 __func__, di->tech_id, di->acronyme );
       Agent_vars->Reset_Dls_Data = g_slist_remove ( Agent_vars->Reset_Dls_Data, di );
       Dls_data_DI_set ( di, FALSE );                                                          /* Mise a zero du bit d'entrée */
     }
  }
/******************************************************************************************************************************/
/* Set_cde_exterieure: Mise à un des bits de commande exterieure                                                              */
/* Entrée: rien                                                                                                               */
/* Sortie: rien                                                                                                               */
/******************************************************************************************************************************/
 void Dls_set_edge ( void )
  { while( Agent_vars->Set_Dls_MONO_Edge_up )                                       /* A-t-on un boolean up a allumer ?? */
     { struct DLS_MONO *mono = Agent_vars->Set_Dls_MONO_Edge_up->data;
       Agent_vars->Set_Dls_MONO_Edge_up   = g_slist_remove  ( Agent_vars->Set_Dls_MONO_Edge_up, mono );
       Agent_vars->Reset_Dls_MONO_Edge_up = g_slist_prepend ( Agent_vars->Reset_Dls_MONO_Edge_up, mono );
       mono->edge_up = TRUE;
     }
    while( Agent_vars->Set_Dls_MONO_Edge_down )                                   /* A-t-on un boolean down a allumer ?? */
     { struct DLS_MONO *mono = Agent_vars->Set_Dls_MONO_Edge_down->data;
       Agent_vars->Set_Dls_MONO_Edge_down   = g_slist_remove  ( Agent_vars->Set_Dls_MONO_Edge_down, mono );
       Agent_vars->Reset_Dls_MONO_Edge_down = g_slist_prepend ( Agent_vars->Reset_Dls_MONO_Edge_down, mono );
       mono->edge_down = TRUE;
     }
    while( Agent_vars->Set_Dls_BI_Edge_up )                                         /* A-t-on un boolean up a allumer ?? */
     { struct DLS_BI *bi = Agent_vars->Set_Dls_BI_Edge_up->data;
       Agent_vars->Set_Dls_BI_Edge_up   = g_slist_remove  ( Agent_vars->Set_Dls_BI_Edge_up, bi );
       Agent_vars->Reset_Dls_BI_Edge_up = g_slist_prepend ( Agent_vars->Reset_Dls_BI_Edge_up, bi );
       bi->edge_up = TRUE;
     }
    while( Agent_vars->Set_Dls_BI_Edge_down )                                     /* A-t-on un boolean down a allumer ?? */
     { struct DLS_BI *bi = Agent_vars->Set_Dls_BI_Edge_down->data;
       Agent_vars->Set_Dls_BI_Edge_down   = g_slist_remove  ( Agent_vars->Set_Dls_BI_Edge_down, bi );
       Agent_vars->Reset_Dls_BI_Edge_down = g_slist_prepend ( Agent_vars->Reset_Dls_BI_Edge_down, bi );
       bi->edge_down = TRUE;
     }
    while( Agent_vars->Set_Dls_DI_Edge_up )                                         /* A-t-on un boolean up a allumer ?? */
     { struct DLS_DI *di = Agent_vars->Set_Dls_DI_Edge_up->data;
       Agent_vars->Set_Dls_DI_Edge_up   = g_slist_remove  ( Agent_vars->Set_Dls_DI_Edge_up, di );
       Agent_vars->Reset_Dls_DI_Edge_up = g_slist_prepend ( Agent_vars->Reset_Dls_DI_Edge_up, di );
       di->edge_up = TRUE;
     }
    while( Agent_vars->Set_Dls_DI_Edge_down )                                     /* A-t-on un boolean down a allumer ?? */
     { struct DLS_DI *di = Agent_vars->Set_Dls_DI_Edge_down->data;
       Agent_vars->Set_Dls_DI_Edge_down   = g_slist_remove  ( Agent_vars->Set_Dls_DI_Edge_down, di );
       Agent_vars->Reset_Dls_DI_Edge_down = g_slist_prepend ( Agent_vars->Reset_Dls_DI_Edge_down, di );
       di->edge_down = TRUE;
     }
  }
/******************************************************************************************************************************/
/* Reset_cde_exterieure: Mise à zero des bits de commande exterieure                                                          */
/* Entrée: rien                                                                                                               */
/* Sortie: rien                                                                                                               */
/******************************************************************************************************************************/
 void Dls_reset_edge ( void )
  { while( Agent_vars->Reset_Dls_MONO_Edge_up )                                     /* A-t-on un boolean up a allumer ?? */
     { struct DLS_MONO *mono = Agent_vars->Reset_Dls_MONO_Edge_up->data;
       Agent_vars->Reset_Dls_MONO_Edge_up = g_slist_remove ( Agent_vars->Reset_Dls_MONO_Edge_up, mono );
       mono->edge_up = FALSE;
     }
    while( Agent_vars->Reset_Dls_MONO_Edge_down )                                 /* A-t-on un boolean down a allumer ?? */
     { struct DLS_MONO *mono = Agent_vars->Reset_Dls_MONO_Edge_down->data;
       Agent_vars->Reset_Dls_MONO_Edge_down = g_slist_remove ( Agent_vars->Reset_Dls_MONO_Edge_down, mono );
       mono->edge_down = FALSE;
     }
    while( Agent_vars->Reset_Dls_BI_Edge_up )                                       /* A-t-on un boolean up a allumer ?? */
     { struct DLS_BI *bi = Agent_vars->Reset_Dls_BI_Edge_up->data;
       Agent_vars->Reset_Dls_BI_Edge_up = g_slist_remove ( Agent_vars->Reset_Dls_BI_Edge_up, bi );
       bi->edge_up = FALSE;
     }
    while( Agent_vars->Reset_Dls_BI_Edge_down )                                   /* A-t-on un boolean down a allumer ?? */
     { struct DLS_BI *bi = Agent_vars->Reset_Dls_BI_Edge_down->data;
       Agent_vars->Reset_Dls_BI_Edge_down = g_slist_remove ( Agent_vars->Reset_Dls_BI_Edge_down, bi );
       bi->edge_down = FALSE;
     }
    while( Agent_vars->Reset_Dls_DI_Edge_up )                                       /* A-t-on un boolean up a allumer ?? */
     { struct DLS_DI *di = Agent_vars->Reset_Dls_DI_Edge_up->data;
       Agent_vars->Reset_Dls_DI_Edge_up = g_slist_remove ( Agent_vars->Reset_Dls_DI_Edge_up, di );
       di->edge_up = FALSE;
     }
    while( Agent_vars->Reset_Dls_DI_Edge_down )                                   /* A-t-on un boolean down a allumer ?? */
     { struct DLS_DI *di = Agent_vars->Reset_Dls_DI_Edge_down->data;
       Agent_vars->Reset_Dls_DI_Edge_down = g_slist_remove ( Agent_vars->Reset_Dls_DI_Edge_down, di );
       di->edge_down = FALSE;
     }
  }
/******************************************************************************************************************************/
/* Dls_data_set_bus : Envoi un message sur le bus système                                                                     */
/* Entrée : l'acronyme, le owner dls, un pointeur de raccourci, et les paramètres du message                                  */
/******************************************************************************************************************************/
 void Dls_data_set_bus ( struct DLS_PLUGIN *plugin, gchar *agent_tech_id, gchar *commande )
  { JsonNode *RootNode = Json_create ();
    if (RootNode)
     { Json_add_string ( RootNode, "commande", commande );
       Agent_send_mqtt_api_message ( Agent, RootNode, FALSE, "SET_BUS/%s", agent_tech_id );
       Json_unref(RootNode);
     }
  }
/******************************************************************************************************************************/
/* Dls_PID_reset: Reset les données calculées du PID                                                                          */
/* Sortie : les bits somme et prev sont mis à 0                                                                               */
/******************************************************************************************************************************/
 void Dls_PID_reset ( struct DLS_PLUGIN *plugin, struct DLS_REGISTRE *input )
  { if (!input) return;

    input->pid_somme_erreurs = 0.0;
    input->pid_prev_erreur   = 0.0;
  }
/******************************************************************************************************************************/
/* Dls_get_top: Récupètre la valeur de l'horloge                                                                              */
/* Sortie : le top horloge                                                                                                    */
/******************************************************************************************************************************/
 gint Dls_get_top ( void )
  { return (Agent->Top); }
/******************************************************************************************************************************/
/* Dls_PID: Gestion du PID                                                                                                    */
/* Sortie : TRUE sur le regean est UP                                                                                         */
/******************************************************************************************************************************/
 void Dls_PID ( struct DLS_PLUGIN *plugin, struct DLS_REGISTRE *input, struct DLS_REGISTRE *consigne,
                struct DLS_REGISTRE *kp,struct DLS_REGISTRE *ki, struct DLS_REGISTRE *kd,
                struct DLS_REGISTRE *outputmin, struct DLS_REGISTRE *outputmax, struct DLS_REGISTRE *output
              )
  { if ( ! (input && consigne && kp && ki && kd && outputmin && outputmax && output ) ) return;

    gdouble erreur           = consigne->valeur - input->valeur;
    input->pid_somme_erreurs+= erreur;                                /* possibilité de débordement si trop long a stabiliser */
    gdouble variation_erreur = erreur - input->pid_prev_erreur;
    gdouble result = kp->valeur * erreur + ki->valeur * input->pid_somme_erreurs + kd->valeur * variation_erreur;
    input->pid_prev_erreur = erreur;

         if (result > outputmax->valeur ) result = outputmax->valeur;
    else if (result < outputmin->valeur ) result = outputmin->valeur;
    Info( __func__, "dls", input->tech_id, LOG_DEBUG,
              "ligne %04d: Changing DLS_PID for '%s:%s'=> '%s:%s'=%f. Somme_Erreur = %f, Variation_Erreur = %f",
              (plugin ? plugin->num_ligne : -1),
              input->tech_id, input->acronyme,
              output->tech_id, output->acronyme, result,
              input->pid_somme_erreurs, variation_erreur
            );
    Dls_data_REGISTRE_set ( plugin, output, result );
  }
 /******************************************************************************************************************************/
/* Dls_sync_all_output: Envoi une synchronisation globale de toutes les sorties DO et AO                                      */
/* Entrée : le Dls_tree correspondant                                                                                         */
/* Sortie : rien                                                                                                              */
/******************************************************************************************************************************/
 void Dls_sync_all_output ( gpointer user_data, struct DLS_PLUGIN *plugin )
  { if (!plugin->handle) return;                                                 /* si plugin non chargé, on ne l'éxecute pas */
    GSList *liste = plugin->Dls_data_DO;
    while ( liste )                                                                                     /* Pour toutes les DO */
     { struct DLS_DO *bit = liste->data;
       JsonNode *RootNode = Json_create ();
       if (RootNode)
        { Dls_DO_to_json ( RootNode, bit );
          g_rw_lock_writer_lock ( &Agent_vars->Liste_DO_synchro );
          Agent_vars->Liste_DO = g_slist_append ( Agent_vars->Liste_DO, RootNode );
          g_rw_lock_writer_unlock ( &Agent_vars->Liste_DO_synchro );
        }
       else Info( __func__, "dls", NULL, LOG_ERR, "JSon RootNode creation failed" );
       liste = g_slist_next ( liste );
     }

    liste = plugin->Dls_data_AO;
    while ( liste )                                                                                     /* Pour toutes les AO */
     { struct DLS_AO *bit = liste->data;
       JsonNode *RootNode = Json_create ();
       if (RootNode)
        { Dls_AO_to_json ( RootNode, bit );
          g_rw_lock_writer_lock ( &Agent_vars->Liste_AO_synchro );
          Agent_vars->Liste_AO = g_slist_append ( Agent_vars->Liste_AO, RootNode );
          g_rw_lock_writer_unlock ( &Agent_vars->Liste_AO_synchro );
        }
       else Info( __func__, "dls", NULL, LOG_ERR, "JSon RootNode creation failed" );
       liste = g_slist_next ( liste );
     }
  }
/******************************************************************************************************************************/
/* Dls_run_plugin: Fait tourner les DLS synoptique en parametre                                                               */
/* Entrée : le plugin DLS correspondant                                                                                       */
/* Sortie : rien                                                                                                              */
/******************************************************************************************************************************/
 void Dls_run_plugin ( struct DLS_PLUGIN *plugin )
  { struct timeval tv_avant, tv_apres;
    if (!plugin->handle) return;                                                 /* si plugin non chargé, on ne l'éxecute pas */

/*--------------------------------------------- Calcul des bits internals ----------------------------------------------------*/
    gboolean bit_comm_module = TRUE;
    GSList *liste = plugin->Arbre_Comm;
    while ( liste )                                                   /* Calcul de la COMM du DLS a partir de ses dependances */
     { struct DLS_WATCHDOG *bit = liste->data;
       bit_comm_module &= Dls_data_WATCHDOG_get( bit );
       liste = g_slist_next ( liste );
     }

    if ( Dls_data_MONO_get ( plugin->dls_comm ) != bit_comm_module )                    /* Envoi à l'API si il y a écart */
     { Dls_data_MONO_set ( plugin, plugin->dls_comm, bit_comm_module );
      Dls_MONO_report_to_API ( plugin->dls_comm );
     }

/*-------------------------------------------------- Calcul du MEMSA_OK ------------------------------------------------------*/
    gboolean new_memsa_ok = bit_comm_module && !( Dls_data_MONO_get( plugin->dls_memsa_defaut ) ||
                                                  Dls_data_MONO_get( plugin->dls_memsa_defaut_fixe ) ||
                                                  Dls_data_MONO_get( plugin->dls_memsa_alarme ) ||
                                                  Dls_data_MONO_get( plugin->dls_memsa_alarme_fixe )
                                                );
    Dls_data_MONO_set ( plugin, plugin->dls_memsa_ok, new_memsa_ok );

/*-------------------------------------------------- Calcul du MEMSSP_OK -----------------------------------------------------*/
    Dls_data_MONO_set ( plugin, plugin->dls_memssp_ok,
                        !( Dls_data_MONO_get( plugin->dls_memssp_derangement ) ||
                           Dls_data_MONO_get( plugin->dls_memssp_derangement_fixe ) ||
                           Dls_data_MONO_get( plugin->dls_memssp_danger ) ||
                           Dls_data_MONO_get( plugin->dls_memssp_danger_fixe )
                         )
                      );

/*----------------------------------------------- Mise a jour des messages de comm -------------------------------------------*/
   if (bit_comm_module) Dls_data_MESSAGE_set ( plugin, plugin->dls_msg_comm_ok );
                   else Dls_data_MESSAGE_set ( plugin, plugin->dls_msg_comm_hs );

/*----------------------------------------------- Lancement du plugin --------------------------------------------------------*/
    gettimeofday( &tv_avant, NULL );
    if (plugin->enable && plugin->go)                                                  /* Si plugin enabled ET fonction go ok */
     { if(plugin->restart)
        { Info( __func__, "dls", plugin->tech_id, LOG_INFO, "Send '_START' to '%s'", plugin->tech_id ); }
       plugin->go( plugin );                                                                            /* On appel le plugin */
     }
    Dls_data_MESSAGE_apply ( plugin );                                             /* Application des nouveaux etats messages */
    Dls_data_VISUEL_apply ( plugin );
    plugin->restart = FALSE;
    gettimeofday( &tv_apres, NULL );
    plugin->conso+=Chrono( &tv_avant, &tv_apres );                                                         /* Ajoute la conso */
  }
/*----------------------------------------------------------------------------------------------------------------------------*/
