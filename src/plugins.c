/******************************************************************************************************************************/
/* ABLS-AGENT-DLS/plugins.c  -> Gestion des plugins pour DLS                                                                  */
/* Projet Abls-Habitat                   Gestion d'habitat                                    dim. 02 janv. 2011 19:04:47 CET */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * plugins.c
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

 #include <glib.h>
 #include <string.h>
 #include <stdio.h>
 #include <dlfcn.h>
 #include <sys/stat.h>
 #include <sys/types.h>
 #include <fcntl.h>
 #include <unistd.h>
 #include <sys/file.h>                                                                /* Gestion des verrous sur les fichiers */
 #include <sys/wait.h>
 #include <sys/sysinfo.h>

/************************************************** Prototypes de fonctions ***************************************************/
 #include "dls.h"

 #define FACILITY_PLUGIN "plugin"

/******************************************************************************************************************************/
/* Dls_get_plugin_by_tech_id: Recupere le plugin depuis son tech_id                                                           */
/* Entrée: Le tech_id du plugin a récupérer                                                                                   */
/* Sortie: le plugin DLS ou NULL si besoin                                                                                    */
/******************************************************************************************************************************/
 struct DLS_PLUGIN *Dls_get_plugin_by_tech_id ( gchar *tech_id )
  { struct DLS_PLUGIN *found = NULL;

    g_rw_lock_reader_lock ( &Agent_vars->Dls_plugins_lock );
    GSList *liste = Agent_vars->Dls_plugins;
    while (liste)
     { struct DLS_PLUGIN *plugin;
       plugin = (struct DLS_PLUGIN *)liste->data;
       if ( ! strcasecmp(plugin->tech_id, tech_id ) )
        { found = plugin; break; }
       liste = liste->next;
     }
    g_rw_lock_reader_unlock ( &Agent_vars->Dls_plugins_lock );
    return(found);
  }
/******************************************************************************************************************************/
/* Dls_foreach_dls_tree: Parcours recursivement l'arbre DLS et execute des commandes en parametres                            */
/* Entrée : le Dls_tree et les fonctions a appliquer                                                                          */
/* Sortie : rien                                                                                                              */
/******************************************************************************************************************************/
 void Dls_foreach_plugins ( void (*do_plugin) (struct DLS_PLUGIN *) )
  { g_rw_lock_reader_lock ( &Agent_vars->Dls_plugins_lock );
    GSList *liste = Agent_vars->Dls_plugins;
    while (liste)
     { struct DLS_PLUGIN *plugin = liste->data;
       do_plugin( plugin );
       liste = liste->next;
     }
    g_rw_lock_reader_unlock ( &Agent_vars->Dls_plugins_lock );
  }
/******************************************************************************************************************************/
/* Activer_plugin_by_id: Active ou non un plugin by id                                                                        */
/* Entrée: l'ID du plugin                                                                                                     */
/* Sortie: Rien                                                                                                               */
/******************************************************************************************************************************/
 void Dls_Activer_plugin ( gchar *tech_id, gboolean actif )
  { if (!tech_id)
     { Info( __func__, FACILITY_PLUGIN, NULL, LOG_ERR, "tech_id is null.");
       return;
     }
    struct DLS_PLUGIN *plugin = Dls_get_plugin_by_tech_id ( tech_id );
    if (!plugin)
     { Info( __func__, FACILITY_PLUGIN, NULL, LOG_ERR, "Plugin '%s' not found", tech_id );
       return;
     }

    if (actif)
     { plugin->enable = TRUE;
       plugin->conso  = 0.0;
       plugin->start_date = time(NULL);
       Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_NOTICE, "'%s' enabled (%s)", plugin->tech_id, plugin->name );
     }
    else
     { plugin->enable = FALSE;
       plugin->conso  = 0.0;
       plugin->start_date = 0;
       Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_NOTICE, "'%s' disabled (%s)", plugin->tech_id, plugin->name );
     }
  }
/******************************************************************************************************************************/
/* Dls_Save_CodeC_to_disk: Enregistre un codec sur le disque pour le tech_id en parametre                                     */
/* Entrée: Le tech_id du DLS a sauver, et le codeC associé                                                                    */
/* Sortie: FALSE si erreur                                                                                                    */
/******************************************************************************************************************************/
 static gboolean Dls_Save_CodeC_to_disk ( gchar *tech_id, gchar *codec )
  { gchar source_file[128];

    Info( __func__, FACILITY_PLUGIN, tech_id, LOG_NOTICE, "Saving '%s' to disk started", tech_id );
    g_snprintf( source_file, sizeof(source_file), "Dls/%s.c", tech_id );
    unlink(source_file);
    gint id_fichier = open( source_file, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR );
    if (id_fichier<0 || lockf( id_fichier, F_TLOCK, 0 ) )
     { Info( __func__, FACILITY_PLUGIN, tech_id, LOG_WARNING, "Open file '%s' for write failed (%s)",
             source_file, strerror(errno) );
       close(id_fichier);
       return(FALSE);
     }

    gint taille_codec = strlen(codec);
    gint retour_write = write( id_fichier, codec, taille_codec );
    close(id_fichier);
    if (retour_write<0)
     { Info( __func__, FACILITY_PLUGIN, tech_id, LOG_ERR, "Write %d bytes to file '%s' failed (%s)",
             taille_codec, source_file, strerror(errno) );
       return(FALSE);
     }
    Info( __func__, FACILITY_PLUGIN, tech_id, LOG_DEBUG, "Write %d bytes to file '%s' OK.", taille_codec, source_file );
    return(TRUE);
  }
/******************************************************************************************************************************/
/* Compiler_source_dls: Compilation de la source DLS en librairie                                                             */
/* Entrée: Le tech_id du DLS a compiler, et le codeC associé                                                                  */
/* Sortie: FALSE si erreur                                                                                                    */
/******************************************************************************************************************************/
 static gboolean Dls_Compiler_source_dls( gchar *tech_id )
  { gchar source_file[128], target_file[128];

    Info( __func__, FACILITY_PLUGIN, tech_id, LOG_NOTICE, "Compilation of '%s' started", tech_id );
    gint top = Agent->Top;
    g_snprintf( source_file, sizeof(source_file), "Dls/%s.c", tech_id );
    g_snprintf( target_file, sizeof(target_file),  "Dls/libdls%s.so", tech_id );
    Info( __func__, FACILITY_PLUGIN, tech_id, LOG_DEBUG, "Starting GCC." );

    gint pidgcc = fork();
    if (pidgcc<0)
    { Info( __func__, FACILITY_PLUGIN, tech_id, LOG_WARNING, "Fils: envoi erreur Fork GCC '%s'", tech_id );
       return(FALSE);
     }
    else if (!pidgcc)
     { execlp( "gcc", "gcc",
               "-I/usr/include/glib-2.0", "-I/usr/lib/glib-2.0/include", "-I/usr/lib64/glib-2.0/include",
               "-I/usr/lib/i386-linux-gnu/glib-2.0/include", "-I/usr/lib/x86_64-linux-gnu/glib-2.0/include",
               "-I/usr/include/json-glib-1.0", "-I/usr/include/sysprof-4",
               "-I/usr/include/libmount", "-I/usr/include/blkid",
               "-shared", "--no-gnu-unique", "-Wno-unused-variable", "-ggdb", "-Wall", "-lwatchdog-dls", "-lm",
               source_file, "-fPIC", "-o", target_file, NULL );
       _exit(0);
     }

    Info( __func__, FACILITY_PLUGIN, tech_id, LOG_DEBUG, "Waiting for gcc to finish pid %d", pidgcc );
    gint wcode;
    waitpid(pidgcc, &wcode, 0 );
    gint gcc_return_code = WEXITSTATUS(wcode);
    if (gcc_return_code == 1) unlink(target_file);
    Info( __func__, FACILITY_PLUGIN, tech_id, LOG_DEBUG, "gcc pid %d is down with return code %d", pidgcc, gcc_return_code );
    Info( __func__, FACILITY_PLUGIN, tech_id, LOG_INFO, "Compilation of '%s' finished in %06.1fs", tech_id, (Agent->Top - top)/10.0 );
    return(TRUE);
  }
/******************************************************************************************************************************/
/* Dls_Dlopen_plugin: Ouverture dyamique d'un plugin dont la structure est en parametre                                       */
/* Entrée: Le plugin D.L.S                                                                                                    */
/* Sortie: FALSE si problème                                                                                                  */
/******************************************************************************************************************************/
 static gboolean Dls_Dlopen_plugin ( struct DLS_PLUGIN *plugin )
  { gchar nom_fichier_absolu[60];

    g_snprintf( nom_fichier_absolu, sizeof(nom_fichier_absolu), "Dls/libdls%s.so", plugin->tech_id );

    if (plugin->handle)                                /* Si deja chargé, on le décharge. A ce niveau, dls est stoppé (mutex) */
     { if (dlclose( plugin->handle ))
        { Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_NOTICE, "'%s': dlclose error '%s' (%s)",
                    plugin->tech_id, dlerror(), plugin->shortname );
        }
       plugin->handle = NULL;
       Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_NOTICE, "'%s' unloaded (%s)", plugin->tech_id, plugin->shortname );
     }

    plugin->handle = dlopen( nom_fichier_absolu, RTLD_LOCAL | RTLD_NOW );                   /* Ouverture du fichier librairie */
    if (!plugin->handle)
     { Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_WARNING, "'%s': dlopen failed (%s)", plugin->tech_id, dlerror() );
       return(FALSE);
     }

    plugin->version = dlsym( plugin->handle, "version" );                                         /* Recherche de la fonction */
    if (!plugin->version)
     { Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_WARNING, "'%s' does not provide version function", plugin->tech_id );
       dlclose( plugin->handle );
       plugin->handle = NULL;
       return(FALSE);
     }

    plugin->remap_all_alias = dlsym( plugin->handle, "remap_all_alias" );                         /* Recherche de la fonction */
    if (!plugin->remap_all_alias)
     { Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_WARNING, "'%s' does not provide remap_all_alias function", plugin->tech_id );
       dlclose( plugin->handle );
       plugin->handle = NULL;
       return(FALSE);
     }

    plugin->init = dlsym( plugin->handle, "Init" );                                          /* Recherche de la fonction Init */
    if (!plugin->init)
     { Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_WARNING, "'%s' does not provide init() function", plugin->tech_id );
       dlclose( plugin->handle );
       plugin->handle = NULL;
       return(FALSE);
     }
/*------------------------------------------------------- Init des variables -------------------------------------------------*/
    plugin->conso = 0.0;
    if (plugin->enable) plugin->start_date = time(NULL);
                   else plugin->start_date = 0;

/*------------------------------------------------------- Chargement GO ------------------------------------------------------*/
    plugin->go = dlsym( plugin->handle, "Go" );                                              /* Recherche de la fonction 'Go' */
    if (!plugin->go)
     { Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_WARNING, "'%s' failed sur absence GO", plugin->tech_id );
       dlclose( plugin->handle );
       plugin->handle = NULL;
       return(FALSE);
     }

    Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_NOTICE, "'%s' dlopened (%s)", plugin->tech_id, plugin->shortname );
    return(TRUE);
  }
/******************************************************************************************************************************/
/* Dls_remap: remap les alias et pointeurs internes d'un plugin                                                               */
/* Entrée: le plugin                                                                                                          */
/* Sortie : les alias sont mappés                                                                                             */
/******************************************************************************************************************************/
 static void Dls_plugin_remap_alias ( struct DLS_PLUGIN *plugin )
  { if (plugin->handle && plugin->remap_all_alias)
     { plugin->remap_all_alias(plugin);
       Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_DEBUG, "Remapping Alias for '%s' OK", plugin->tech_id );
     }
    else Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_ERR, "Remapping Alias for '%s' Failed", plugin->tech_id );

    if (!strcasecmp ( plugin->tech_id, "SYS" ) )                           /* Mapping des bits internes pour le plugin "SYS" */
     { Agent_vars->sys_flipflop_5hz   = Dls_data_BI_lookup   ( "SYS", "FLIPFLOP_5HZ" );
       Agent_vars->sys_flipflop_2hz   = Dls_data_BI_lookup   ( "SYS", "FLIPFLOP_2HZ" );
       Agent_vars->sys_flipflop_1sec  = Dls_data_BI_lookup   ( "SYS", "FLIPFLOP_1SEC" );
       Agent_vars->sys_flipflop_2sec  = Dls_data_BI_lookup   ( "SYS", "FLIPFLOP_2SEC" );
       Agent_vars->sys_mqtt_connected = Dls_data_BI_lookup   ( "SYS", "MQTT_CONNECTED" );
       Agent_vars->sys_top_5hz        = Dls_data_MONO_lookup ( "SYS", "TOP_5HZ" );
       Agent_vars->sys_top_2hz        = Dls_data_MONO_lookup ( "SYS", "TOP_2HZ" );
       Agent_vars->sys_top_1sec       = Dls_data_MONO_lookup ( "SYS", "TOP_1SEC" );
       Agent_vars->sys_top_5sec       = Dls_data_MONO_lookup ( "SYS", "TOP_5SEC" );
       Agent_vars->sys_top_10sec      = Dls_data_MONO_lookup ( "SYS", "TOP_10SEC" );
       Agent_vars->sys_top_1min       = Dls_data_MONO_lookup ( "SYS", "TOP_1MIN" );
       Agent_vars->sys_bit_per_sec    = Dls_data_AI_lookup   ( "SYS", "DLS_BIT_PER_SEC" );
       Agent_vars->sys_tour_per_sec   = Dls_data_AI_lookup   ( "SYS", "DLS_TOUR_PER_SEC" );
       Agent_vars->sys_dls_wait       = Dls_data_AI_lookup   ( "SYS", "DLS_WAIT" );
       Agent_vars->sys_maxrss         = Dls_data_AI_lookup   ( "SYS", "MAXRSS" );
       Agent_vars->sys_log_per_min    = Dls_data_AI_lookup   ( "SYS", "LOG_PER_MIN" );
     }

    plugin->dls_osyn_acquit             = Dls_data_DI_lookup   ( plugin->tech_id, "OSYN_ACQUIT" );
    plugin->dls_comm                    = Dls_data_MONO_lookup ( plugin->tech_id, "COMM" );
    plugin->dls_memsa_ok                = Dls_data_MONO_lookup ( plugin->tech_id, "MEMSA_OK" );
    plugin->dls_memsa_defaut            = Dls_data_MONO_lookup ( plugin->tech_id, "MEMSA_DEFAUT" );
    plugin->dls_memsa_defaut_fixe       = Dls_data_MONO_lookup ( plugin->tech_id, "MEMSA_DEFAUT_FIXE" );
    plugin->dls_memsa_alarme            = Dls_data_MONO_lookup ( plugin->tech_id, "MEMSA_ALARME" );
    plugin->dls_memsa_alarme_fixe       = Dls_data_MONO_lookup ( plugin->tech_id, "MEMSA_ALARME_FIXE" );
    plugin->dls_memssb_veille           = Dls_data_MONO_lookup ( plugin->tech_id, "MEMSSB_VEILLE" );
    plugin->dls_memssb_alerte           = Dls_data_MONO_lookup ( plugin->tech_id, "MEMSSB_ALERTE" );
    plugin->dls_memssb_alerte_fixe      = Dls_data_MONO_lookup ( plugin->tech_id, "MEMSSB_ALERTE_FIXE" );
    plugin->dls_memssp_ok               = Dls_data_MONO_lookup ( plugin->tech_id, "MEMSSP_OK" );
    plugin->dls_memssp_derangement      = Dls_data_MONO_lookup ( plugin->tech_id, "MEMSSP_DERANGEMENT" );
    plugin->dls_memssp_derangement_fixe = Dls_data_MONO_lookup ( plugin->tech_id, "MEMSSP_DERANGEMENT_FIXE" );
    plugin->dls_memssp_danger           = Dls_data_MONO_lookup ( plugin->tech_id, "MEMSSP_DANGER" );
    plugin->dls_memssp_danger_fixe      = Dls_data_MONO_lookup ( plugin->tech_id, "MEMSSP_DANGER_FIXE" );
    plugin->dls_msg_comm_ok             = Dls_data_MESSAGE_lookup ( plugin->tech_id, "MSG_COMM_OK" );
    plugin->dls_msg_comm_hs             = Dls_data_MESSAGE_lookup ( plugin->tech_id, "MSG_COMM_HS" );
  }
/******************************************************************************************************************************/
/* Dls_plugins_remap_all_alias: remap les alias de tous les plugins                                                           */
/* Entrée: rien                                                                                                               */
/* Sortie : les alias sont mappés                                                                                             */
/******************************************************************************************************************************/
 static void Dls_plugins_remap_all_alias ( void )
  { Dls_foreach_plugins ( Dls_plugin_remap_alias ); }
/******************************************************************************************************************************/
/* Dls_Importer_un_plugin: Ajoute ou Recharge un plugin dans la liste des plugins                                             */
/* Entrée: le tech_id associé                                                                                                 */
/* Sortie: Néant                                                                                                              */
/******************************************************************************************************************************/
 struct DLS_PLUGIN *Dls_Reload_un_plugin ( gchar *tech_id )
  { Info( __func__, FACILITY_PLUGIN, tech_id, LOG_INFO, "Starting reload of plugin '%s'", tech_id );
    Dls_Decharger_un_plugin( tech_id );                                                 /* d'abord on le libère de la mémoire */
                                                                                 /* Récupère les metadata du plugin à charger */
    JsonNode *api_result = Http_Get_from_global_API ( Agent, "/run/dls/load", "tech_id=%s", tech_id );
    if (api_result == NULL || Json_get_int ( api_result, "http_code" ) != 200)
     { Info( __func__, FACILITY_PLUGIN, tech_id, LOG_ERR, "'%s': API Error.", tech_id );
       return(NULL);
     }

    if ( !Json_has_member ( api_result, "codec" ) )
     { Info( __func__, FACILITY_PLUGIN, tech_id, LOG_ERR, "'%s': Missing CodeC.", tech_id );
       Json_unref(api_result);
       return(NULL);
     }

    Dls_Save_CodeC_to_disk ( tech_id, Json_get_string ( api_result, "codec" ) );
    Dls_Compiler_source_dls ( tech_id );

    struct DLS_PLUGIN *plugin = g_try_malloc0 ( sizeof (struct DLS_PLUGIN) );
    if (!plugin)                                                                                /* Peuplement de la structure */
     { Info( __func__, FACILITY_PLUGIN, tech_id, LOG_ERR, "'%s' Memory error", tech_id );
       Json_unref(api_result);
       return(NULL);
     }
    g_snprintf ( plugin->tech_id,   sizeof(plugin->tech_id),   "%s", tech_id );
    g_snprintf ( plugin->name,      sizeof(plugin->name),      "%s", Json_get_string ( api_result, "name" ) );
    g_snprintf ( plugin->shortname, sizeof(plugin->shortname), "%s", Json_get_string ( api_result, "shortname" ) );
    plugin->enable = Json_get_bool ( api_result, "enable" );

/********************************* Chargement des nouveaux CI *****************************************************************/
    Json_foreach_array_element ( api_result, "mnemos_CI", Dls_data_CI_create_by_array, plugin );
    Json_foreach_array_element ( api_result, "mnemos_CH", Dls_data_CH_create_by_array, plugin );
    Json_foreach_array_element ( api_result, "mnemos_REGISTRE", Dls_data_REGISTRE_create_by_array, plugin );
    Json_foreach_array_element ( api_result, "mnemos_AI", Dls_data_AI_create_by_array, plugin );
    Json_foreach_array_element ( api_result, "mnemos_DI", Dls_data_DI_create_by_array, plugin );
    Json_foreach_array_element ( api_result, "mnemos_DO", Dls_data_DO_create_by_array, plugin );
    Json_foreach_array_element ( api_result, "mnemos_AO", Dls_data_AO_create_by_array, plugin );
    Json_foreach_array_element ( api_result, "mnemos_MONO", Dls_data_MONO_create_by_array, plugin );
    Json_foreach_array_element ( api_result, "mnemos_BI", Dls_data_BI_create_by_array, plugin );
    Json_foreach_array_element ( api_result, "mnemos_VISUEL", Dls_data_VISUEL_create_by_array, plugin );
    Json_foreach_array_element ( api_result, "mnemos_MESSAGE", Dls_data_MESSAGE_create_by_array, plugin );
    Json_foreach_array_element ( api_result, "mnemos_WATCHDOG", Dls_data_WATCHDOG_create_by_array, plugin );
    Json_foreach_array_element ( api_result, "mnemos_TEMPO", Dls_data_TEMPO_create_by_array, plugin );
    Json_foreach_array_element ( api_result, "mnemos_HORLOGE", Dls_data_HORLOGE_create_by_array, plugin );

    if (Dls_Dlopen_plugin ( plugin ) == FALSE)               /* DlOpen before remap (sinon on mappe pas la bonne zone mémoire */
     { Info( __func__, FACILITY_PLUGIN, tech_id, LOG_ERR, "'%s' Error when dlopening", tech_id ); }

/******************************************* Calcul des agent_tech_ids de dependances *****************************************/
    if (plugin->Agent_tech_ids) { g_slist_free_full ( plugin->Agent_tech_ids, (GDestroyNotify) g_free ); plugin->Agent_tech_ids = NULL; }

    GList *Agent_tech_ids = json_array_get_elements ( Json_get_array ( api_result, "agent_tech_ids" ) );
    GList *agent_tech_ids = Agent_tech_ids;
    while(agent_tech_ids)
     { JsonNode *element = agent_tech_ids->data;
       plugin->Agent_tech_ids = g_slist_append ( plugin->Agent_tech_ids, Json_get_string ( element, "agent_tech_id" ) );
       agent_tech_ids = g_list_next(agent_tech_ids);
     }
    g_list_free(Agent_tech_ids);

    g_rw_lock_writer_lock( &Agent_vars->Dls_plugins_lock );                    /* On stoppe DLS pour éviter la compilation multiple */
    Agent_vars->Dls_plugins = g_slist_append( Agent_vars->Dls_plugins, plugin );                          /* Ajout à la liste */
    g_rw_lock_writer_unlock( &Agent_vars->Dls_plugins_lock );

    Dls_plugins_remap_all_alias();                                             /* Remap de tous les alias de tous les plugins */
    if (plugin->init) plugin->init(plugin);                                            /* Appel de la fonction Init du plugin */

    Json_unref(api_result);
    return(plugin);
  }
/******************************************************************************************************************************/
/* Dls_Reload_un_plugin_by_array: Import un DLS par array                                                                     */
/* Entrée: les parametres du tableau                                                                                          */
/* Sortie: Néant                                                                                                              */
/******************************************************************************************************************************/
 static void Dls_Reload_un_plugin_by_array ( JsonArray *array, guint index, JsonNode *element, gpointer user_data )
  { Dls_Reload_un_plugin ( Json_get_string ( element, "tech_id" ) ); }
/******************************************************************************************************************************/
/* Dls_Importer_plugins: Importe tous les plugins depuis l'API                                                                */
/* Entrée: Rien                                                                                                               */
/* Sortie: Rien                                                                                                               */
/******************************************************************************************************************************/
 void Dls_Importer_plugins ( void )
  { guint top = Agent->Top;
    JsonNode *api_result = Http_Post_to_global_API ( Agent, "/run/dls/plugins", NULL );
    if (api_result == NULL || Json_get_int ( api_result, "http_code" ) != 200)
     { Info( __func__, FACILITY_PLUGIN, NULL, LOG_ERR, "API Request for /run/dls/plugins failed. No plugin loaded." );
       Json_unref ( api_result );
       return;
     }
    Info( __func__, FACILITY_PLUGIN, NULL, LOG_INFO, "API Request for /run/dls/plugins OK." );

    Json_foreach_array_element_by_thread ( api_result, "plugins", Dls_Reload_un_plugin_by_array, NULL, 2*get_nprocs() );
    Info( __func__, FACILITY_PLUGIN, NULL, LOG_NOTICE, "%03d plugins loaded in %06.1fs (with %02d proc)",
          Json_get_int ( api_result, "nbr_plugins" ), (Agent->Top-top)/10.0, get_nprocs() );
    Json_unref ( api_result );
  }
/******************************************************************************************************************************/
/* Decharger_un_plugin: Decharge un plugin DLS                                                                                */
/* Entrée: le tech_id                                                                                                         */
/* Sortie: Rien                                                                                                               */
/******************************************************************************************************************************/
 void Dls_Decharger_un_plugin ( gchar *tech_id )
  { struct DLS_PLUGIN *plugin = Dls_get_plugin_by_tech_id( tech_id );
    if (!plugin) { Info( __func__, FACILITY_PLUGIN, tech_id, LOG_ERR, "'%s': Plugin not found.", tech_id ); return; }

    g_rw_lock_writer_lock ( &Agent_vars->Dls_plugins_lock );
    Agent_vars->Dls_plugins = g_slist_remove ( Agent_vars->Dls_plugins, plugin );
    g_rw_lock_writer_unlock ( &Agent_vars->Dls_plugins_lock );
    Dls_plugins_remap_all_alias();                                             /* Remap de tous les alias de tous les plugins */

    Dls_Save_Data_to_API ( plugin );                                              /* Sauvegarde les valeurs des bits internes */
    if (plugin->handle && dlclose( plugin->handle ))
     { Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_NOTICE, "dlclose error '%s' for '%s' (%s)",
             dlerror(), plugin->tech_id, plugin->shortname );
     }

    if (plugin->Dls_data_CI)       g_slist_free_full ( plugin->Dls_data_CI, (GDestroyNotify) g_free );
    if (plugin->Dls_data_CH)       g_slist_free_full ( plugin->Dls_data_CH, (GDestroyNotify) g_free );
    if (plugin->Dls_data_DI)       g_slist_free_full ( plugin->Dls_data_DI, (GDestroyNotify) g_free );
    if (plugin->Dls_data_DO)       g_slist_free_full ( plugin->Dls_data_DO, (GDestroyNotify) g_free );
    if (plugin->Dls_data_AI)       g_slist_free_full ( plugin->Dls_data_AI, (GDestroyNotify) g_free );
    if (plugin->Dls_data_AO)       g_slist_free_full ( plugin->Dls_data_AO, (GDestroyNotify) g_free );
    if (plugin->Dls_data_BI)       g_slist_free_full ( plugin->Dls_data_BI, (GDestroyNotify) g_free );
    if (plugin->Dls_data_MONO)     g_slist_free_full ( plugin->Dls_data_MONO, (GDestroyNotify) g_free );
    if (plugin->Dls_data_REGISTRE) g_slist_free_full ( plugin->Dls_data_REGISTRE, (GDestroyNotify) g_free );
    if (plugin->Dls_data_TEMPO)    g_slist_free_full ( plugin->Dls_data_TEMPO, (GDestroyNotify) g_free );
    if (plugin->Dls_data_HORLOGE)  g_slist_free_full ( plugin->Dls_data_HORLOGE, (GDestroyNotify) g_free );
    if (plugin->Dls_data_WATCHDOG) g_slist_free_full ( plugin->Dls_data_WATCHDOG, (GDestroyNotify) g_free );
    if (plugin->Dls_data_VISUEL)   g_slist_free_full ( plugin->Dls_data_VISUEL, (GDestroyNotify) g_free );
    if (plugin->Arbre_Comm) g_slist_free(plugin->Arbre_Comm);
    Dls_data_MESSAGE_free_all ( plugin );

                                                                             /* Destruction de l'entete associé dans la GList */
    Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_INFO, "plugin '%s' unloaded (%s)", plugin->tech_id, plugin->name );
    g_free( plugin );
  }
/******************************************************************************************************************************/
/* Decharger_plugins: Decharge tous les plugins DLS                                                                           */
/* Entrée: Rien                                                                                                               */
/* Sortie: Rien                                                                                                               */
/******************************************************************************************************************************/
 void Dls_Decharger_plugins ( void )
  { while(Agent_vars->Dls_plugins)                                                                /* Liberation mémoire des modules */
     { struct DLS_PLUGIN *plugin = Agent_vars->Dls_plugins->data;
       Dls_Decharger_un_plugin ( plugin->tech_id );
     }
  }
/******************************************************************************************************************************/
/* Dls_Debug_plugin: Active ou non le debug d'un plugin                                                                       */
/* Entrée: le tech_id et le choix actif ou non                                                                                */
/* Sortie: Rien                                                                                                               */
/******************************************************************************************************************************/
 void Dls_Debug_plugin ( gchar *tech_id, gboolean actif )
  { if (!tech_id)
     { Info( __func__, FACILITY_PLUGIN, NULL, LOG_ERR, "tech_id is null.");
       return;
     }
    struct DLS_PLUGIN *plugin = Dls_get_plugin_by_tech_id ( tech_id );
    if (!plugin)
    { Info( __func__, FACILITY_PLUGIN, tech_id, LOG_ERR, "Plugin '%s' not found", tech_id );
       return;
     }

    plugin->debug = actif;
    Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_NOTICE, "'%s' debug %s ('%s')",
          plugin->tech_id, (actif ? "started" : "stopped"), plugin->name );
  }
/******************************************************************************************************************************/
/* Activer_plugin_by_id: Active ou non un plugin by id                                                                        */
/* Entrée: l'ID du plugin                                                                                                     */
/* Sortie: Rien                                                                                                               */
/******************************************************************************************************************************/
 void Dls_Acquitter_plugin ( gchar *tech_id )
  { if (!tech_id)
     { Info( __func__, FACILITY_PLUGIN, NULL, LOG_ERR, "tech_id is null.");
       return;
     }
    struct DLS_PLUGIN *plugin = Dls_get_plugin_by_tech_id ( tech_id );
    if (!plugin)
     { Info( __func__, FACILITY_PLUGIN, tech_id, LOG_ERR, "Plugin '%s' not found", tech_id );
       return;
     }

    Info( __func__, FACILITY_PLUGIN, plugin->tech_id, LOG_NOTICE, "'%s' acquitté ('%s')", plugin->tech_id, plugin->shortname );
    struct DLS_DI *bit = Dls_data_DI_lookup ( plugin->tech_id, "OSYN_ACQUIT" );
    Dls_data_DI_set_pulse ( plugin, bit );
  }
/*----------------------------------------------------------------------------------------------------------------------------*/
