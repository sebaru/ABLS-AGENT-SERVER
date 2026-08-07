/******************************************************************************************************************************/
/* ABLS-AGENT-DLS/src/distrib_messages.c  Conversion des libellés dynamiques avec variables                                   */
/* Projet Abls-Habitat                   Gestion d'habitat                                                04.08.2026 12:00:00 */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * distrib_messages.c
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

/*********************************************************** Includes *********************************************************/
 #include "dls.h"

/******************************************************************************************************************************/
/* Convert_libelle_dynamique: Conversion du libelle en parametre pour gérer les $ dynamiques                                  */
/* Entrée : le libelle                                                                                                        */
/* Sortie: le nouveau libelle adapté, à libérer ensuite                                                                       */
/******************************************************************************************************************************/
 gchar *Convert_libelle_dynamique ( gchar *libelle_src )
  { gchar prefixe[128], tech_id[32], acronyme[64], suffixe[128], libelle[256], chaine[32];
    gint taille_result = 256;
    gchar *result = g_try_malloc0 ( taille_result );
    if (!result)
     { Info( __func__, "distrib", NULL, LOG_ERR, "Memory error for '%s'", libelle_src );
       return(NULL);
     }
    g_snprintf ( result, taille_result, "%s", libelle_src );
    g_snprintf ( libelle, sizeof(libelle), "%s", libelle_src );

    for(;;)
     { memset ( prefixe,  0, sizeof(prefixe)  );                    /* Mise à zero pour gérer correctement les fins de tampon */
       memset ( suffixe,  0, sizeof(suffixe)  );
       memset ( tech_id,  0, sizeof(tech_id)  );
       memset ( acronyme, 0, sizeof(acronyme) );

       sscanf ( libelle, "%128[^$]$%32[^:]:%64[a-zA-Z0-9_]%128[^\n]", prefixe, tech_id, acronyme, suffixe );
       if (prefixe[0] == '\0')                                                     /* si pas de prefixe, on retente en direct */
        { sscanf ( libelle, "$%32[^:]:%64[a-zA-Z0-9_]%128[^\n]", tech_id, acronyme, suffixe ); }

       if (tech_id[0] == '\0' || acronyme[0] == '\0') break;                    /* Aucun couple tech_id:acronyme, on termine */

       struct DLS_REGISTRE *reg;
       struct DLS_AI *ai;
       if ( (ai = Dls_data_AI_lookup ( tech_id, acronyme )) != NULL )
        { g_snprintf( chaine, sizeof(chaine), "%.02f %s", ai->valeur, ai->unite ); }
       else if ( (reg = Dls_data_REGISTRE_lookup ( tech_id, acronyme )) != NULL )
        { g_snprintf( chaine, sizeof(chaine), "%.02f %s", reg->valeur, reg->unite ); }
       else
        { g_snprintf( chaine, sizeof(chaine), "bit inconnu" ); }

       g_snprintf ( result, taille_result, "%s%s%s", prefixe, chaine, suffixe );
       g_snprintf ( libelle, sizeof(libelle), "%s", result );                             /* recopie pour prochaine itération */
     }
    return(result);
  }
/*----------------------------------------------------------------------------------------------------------------------------*/
