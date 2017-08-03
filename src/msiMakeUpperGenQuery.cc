/**
 * \file
 * \brief     iRODS microservice for uppercase version of msiMakeGenQuery.
 * \author    Lazlo Westerhof
 * \copyright Copyright (c) 2017, Utrecht University
 *
 * Copyright (c) 2017, Utrecht University
 *
 * This file is part of irods-uu-microservices.
 *
 * irods-uu-microservices is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * irods-uu-microservices is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with irods-uu-microservices.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *   Copyright (c) 2005-2016, Regents of the University of California, the
 *   University of North Carolina at Chapel Hill, and the Data Intensive
 *   Cyberinfrastructure Foundation
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   - Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *   - Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *   - Neither the name of the University of California, San Diego (UCSD),
 *     the University of North Carolina at Chapel Hill nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */

#include "irods_includes.hh"
#include "genQuery.h"
#include "rsGenQuery.hpp"
#include "rcMisc.h"


int _makeQuery( char *sel, char *cond, char **sql ) {
  *sql = ( char * ) malloc( strlen( sel ) + strlen( cond ) + 20 );
  if ( strlen( cond ) >  0 ) {
    sprintf( *sql, "SELECT %s WHERE %s", sel, cond );
  }
  else {
    sprintf( *sql, "SELECT %s ", sel );
  }
  return 0;
}

int  msiMakeUpperGenQuery( msParam_t* selectListStr, msParam_t* condStr, msParam_t* genQueryInpParam, ruleExecInfo_t *rei ) {
  char *sel, *cond, *rawQuery, *query;


  RE_TEST_MACRO( "    Calling msiMakeUpperGenQuery" )

    if ( rei == NULL || rei->rsComm == NULL ) {
      rodsLog( LOG_ERROR, "msiMakeUpperGenQuery: input rei or rsComm is NULL." );
      return SYS_INTERNAL_NULL_INPUT_ERR;
    }

  /* parse selectListStr */
  if ( ( sel = parseMspForStr( selectListStr ) ) == NULL ) {
    rodsLog( LOG_ERROR, "msiMakeUpperGenQuery: input selectListStr is NULL." );
    return USER__NULL_INPUT_ERR;
  }

  /* parse condStr */
  if ( ( cond = parseMspForStr( condStr ) ) == NULL ) {
    rodsLog( LOG_ERROR, "msiMakeUpperGenQuery: input condStr is NULL." );
    return USER__NULL_INPUT_ERR;
  }

  /* The code below is partly taken from msiMakeQuery and msiExecStrCondQuery. There may be a better way to do this. */

  /* Generate raw SQL query string */
  rei->status = _makeQuery( sel, cond, &rawQuery );

  /* allocate more memory for query string with expanded variable names */
  query = ( char * )malloc( strlen( rawQuery ) + 10 + MAX_NAME_LEN * 8 );
  strcpy( query, rawQuery );

  /* allocate memory for genQueryInp */
  genQueryInp_t * genQueryInp = ( genQueryInp_t* )malloc( sizeof( genQueryInp_t ) );
  memset( genQueryInp, 0, sizeof( genQueryInp_t ) );

  /* set up GenQueryInp */
  genQueryInp->maxRows = MAX_SQL_ROWS;
  genQueryInp->continueInx = 0;

  /* Ensure that the value in the 'where' condition will be made upper
     case so one can do case-insensitive queries. */
  genQueryInp->options = UPPER_CASE_WHERE;

  rei->status = fillGenQueryInpFromStrCond( query, genQueryInp );
  if ( rei->status < 0 ) {
    rodsLog( LOG_ERROR, "msiMakeUpperGenQuery: fillGenQueryInpFromStrCond failed." );
    freeGenQueryInp( &genQueryInp );
    free( rawQuery ); // cppcheck - Memory leak: rawQuery
    free( query );
    return rei->status;
  }

  /* return genQueryInp through GenQueryInpParam */
  genQueryInpParam->type = strdup( GenQueryInp_MS_T );
  genQueryInpParam->inOutStruct = genQueryInp;

  /* cleanup */
  free( rawQuery );
  free( query );

  return rei->status;
}


irods::ms_table_entry* plugin_factory() {
  irods::ms_table_entry *msvc = new irods::ms_table_entry(2);

  msvc->add_operation("msiMakeUpperGenQuery",
		      std::function<decltype(msiMakeUpperGenQuery)>(msiMakeUpperGenQuery));

  return msvc;
}
