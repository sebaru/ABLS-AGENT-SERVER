/******************************************************************************************************************************/
/* ABLS-AGENT-DLS/heure.c    ->    fonctions appelées par les plugins D.L.S                                                   */
/* Projet Abls-Habitat                   Gestion d'habitat                                       lun 22 déc 2003 16:46:02 CET */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * heure.c
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

 #include <time.h>

 #include "heure.h"

 static gint nbr_heure, nbr_minute;                                                    /* Gestion des demarrages à heure fixe */
 static gint num_jour_semaine;                                                                /* Numéro du jour de la semaine */
 static gboolean top_horaire = FALSE;                          /* Indique que l'heure a été mise à jour par Dls_Top_horaire() */

/******************************************************************************************************************************/
/* Dls_Check_horaire: Prend la date/heure actuelle de la machine. Appelée toutes les minutes par DLS                            */
/* Entrée: rien                                                                                                               */
/* Sortie: les variables globales de gestion de l'heure sont mises à jour                                                     */
/******************************************************************************************************************************/
 void Dls_Check_top_horaire ( void )
  { struct tm tm;
    time_t temps;

    time(&temps);
    localtime_r( &temps, &tm );
    if ( nbr_heure != tm.tm_hour || nbr_minute != tm.tm_min || num_jour_semaine != tm.tm_wday )              /* Si changement */
     { nbr_heure        = tm.tm_hour;          /* Sinon sauvegarde dans les variables, et edge_up=1 pendant un tour programme */
       nbr_minute       = tm.tm_min;
       num_jour_semaine = tm.tm_wday;
       top_horaire      = TRUE;
     }
  }
/******************************************************************************************************************************/
/* Dls_Top_horaire: Prend la date/heure actuelle de la machine. Appelée toutes les minutes par DLS                            */
/* Entrée: rien                                                                                                               */
/* Sortie: les variables globales de gestion de l'heure sont mises à jour                                                     */
/******************************************************************************************************************************/
 void Dls_Stop_top_horaire ( void )
  { top_horaire = FALSE; }
/******************************************************************************************************************************/
/* Dls_Jour_semaine: renvoie TRUE si le jour actuel est celui en parametre                                                    */
/* Entrée: le jour a tester                                                                                                   */
/* Sortie: TRUE / FALSE                                                                                                       */
/******************************************************************************************************************************/
 gboolean Dls_Jour_semaine ( int jour )
  { return( num_jour_semaine == jour ); }
/******************************************************************************************************************************/
/* Dls_Heure: renvoie TRUE si l'heure actuelle est celle en paramètre                                                         */
/* Entrée: heure et minute attendue                                                                                           */
/* Sortie: TRUE / FALSE                                                                                                       */
/******************************************************************************************************************************/
 gboolean Dls_Heure ( int heure, int minute )
  { return ( nbr_heure==heure && nbr_minute==minute && top_horaire ); }
/******************************************************************************************************************************/
/* Dls_Heure_apres: renvoie TRUE si l'heure actuelle est future à celle en parametre                                          */
/* Entrée: heure et minute attendue                                                                                           */
/* Sortie: TRUE / FALSE                                                                                                       */
/******************************************************************************************************************************/
 gboolean Dls_Heure_apres ( int heure, int minute )
  { return( (heure<nbr_heure || (heure==nbr_heure && minute<nbr_minute)) ); }
/******************************************************************************************************************************/
/* Dls_Heure_avant: renvoie TRUE si l'heure actuelle est antérieure a celle en parametre                                      */
/* Entrée: heure et minute attendue                                                                                           */
/* Sortie: TRUE / FALSE                                                                                                       */
/******************************************************************************************************************************/
 gboolean Dls_Heure_avant ( int heure, int minute )
  { return( (heure>nbr_heure || (heure==nbr_heure && minute>nbr_minute)) ); }
/******************************************************************************************************************************/
/* Dls_Heure_apres_egal: renvoie TRUE si l'heure actuelle est future à celle en parametre                                     */
/* Entrée: heure et minute attendue                                                                                           */
/* Sortie: TRUE / FALSE                                                                                                       */
/******************************************************************************************************************************/
 gboolean Dls_Heure_apres_egal ( int heure, int minute )
  { return( (heure<nbr_heure || (heure==nbr_heure && minute<=nbr_minute)) ); }
/******************************************************************************************************************************/
/* Dls_Heure_avant_egal: renvoie TRUE si l'heure actuelle est antérieure a celle en parametre                                 */
/* Entrée: heure et minute attendue                                                                                           */
/* Sortie: TRUE / FALSE                                                                                                       */
/******************************************************************************************************************************/
 gboolean Dls_Heure_avant_egal ( int heure, int minute )
  { return( (heure>nbr_heure || (heure==nbr_heure && minute>=nbr_minute)) ); }
/*----------------------------------------------------------------------------------------------------------------------------*/
