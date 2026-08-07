/******************************************************************************************************************************/
/* ABLS-AGENT-SERVER/include/server.h   Header du template agent server                                                      */
/* Projet Abls-Habitat                   Gestion d'habitat                                                17.07.2026 12:00:00 */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * server.h
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

#ifndef _ABLS_SERVER_H_
#define _ABLS_SERVER_H_

#include <abls-agent-libs/abls-agent-libs.h>
#include "dls.h"

 struct ABLS_SERVER_VARS
  { gboolean initialized;
    GSList *Dls_plugins;
    GRWLock Dls_plugins_lock;

    GSList *Set_Dls_DI_Edge_up;
    GSList *Set_Dls_DI_Edge_down;
    GSList *Set_Dls_MONO_Edge_up;
    GSList *Set_Dls_MONO_Edge_down;
    GSList *Set_Dls_BI_Edge_up;
    GSList *Set_Dls_BI_Edge_down;
    GSList *Set_Dls_Data;
    GSList *Reset_Dls_DI_Edge_up;
    GSList *Reset_Dls_DI_Edge_down;
    GSList *Reset_Dls_MONO_Edge_up;
    GSList *Reset_Dls_MONO_Edge_down;
    GSList *Reset_Dls_BI_Edge_up;
    GSList *Reset_Dls_BI_Edge_down;
    GSList *Reset_Dls_Data;
    GSList *HORLOGE_actives;
    JsonNode *HORLOGE_ticks;

    struct DLS_BI *sys_flipflop_5hz;
    struct DLS_BI *sys_flipflop_2hz;
    struct DLS_BI *sys_flipflop_1sec;
    struct DLS_BI *sys_flipflop_2sec;
    struct DLS_BI *sys_mqtt_connected;
    struct DLS_MONO *sys_top_5hz;
    struct DLS_MONO *sys_top_2hz;
    struct DLS_MONO *sys_top_1sec;
    struct DLS_MONO *sys_top_5sec;
    struct DLS_MONO *sys_top_10sec;
    struct DLS_MONO *sys_top_1min;
    struct DLS_AI *sys_bit_per_sec;
    struct DLS_AI *sys_tour_per_sec;
    struct DLS_AI *sys_dls_wait;
    struct DLS_AI *sys_maxrss;
    struct DLS_AI *sys_log_per_min;

    GRWLock Liste_DO_synchro;
    GSList *Liste_DO;
    GRWLock Liste_AO_synchro;
    GSList *Liste_AO;
    GRWLock Liste_visuel_synchro;
    GSList *Liste_visuel;
    GRWLock Liste_msg_synchro;
    GSList *Liste_msg;

    guint audit_bit_interne_per_sec;
    guint audit_bit_interne_per_sec_hold;

    guint next_top_2hz;
    guint next_top_5hz;
    guint next_top_1sec;
    guint next_top_2sec;
    guint next_top_5sec;
    guint next_top_10sec;
    guint next_top_1min;
    guint next_top_10min;
    guint last_top;
 };

#endif
/*----------------------------------------------------------------------------------------------------------------------------*/
