/******************************************************************************************************************************/
/* ABLS-AGENT-DLS/include/map.h  Declarations pour la gestion des mappings                                                   */
/* Projet Abls-Habitat                   Gestion d'habitat                                                01.08.2026 12:00:00 */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * map.h
 * This file is part of Abls-Habitat
 *
 * Copyright (C) 1988-2026 - Sébastien LEFEVRE
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

#ifndef _ABLS_AGENT_DLS_MAP_H_
 #define _ABLS_AGENT_DLS_MAP_H_

 #include <glib.h>
 #include <json-glib/json-glib.h>

 extern void MAP_Init ( void );
 extern void MAP_End ( void );
 extern gboolean MAP_to_local ( JsonNode *key );
 extern gboolean MAP_to_agent ( JsonNode *key );
 extern void MAP_Remap ( void );

 #endif
/*----------------------------------------------------------------------------------------------------------------------------*/