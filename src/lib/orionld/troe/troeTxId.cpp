/*
*
* Copyright 2026 FIWARE Foundation e.V.
*
* This file is part of Orion-LD Context Broker.
*
* Orion-LD Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion-LD Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion-LD Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* orionld at fiware dot org
*
* Author: Ken Zangelin
*/
#include "orionld/common/orionldState.h"                       // orionldState
#include "orionld/common/uuidGenerate.h"                       // uuidGenerate
#include "orionld/troe/troeTxId.h"                             // Own interface



// -----------------------------------------------------------------------------
//
// troeTxId -
//
// Returns the per-request transaction/snapshot id, generating it on first use.
// 'orionldState' is zeroed at the start of every request (orionldStateInit), so the
// lazy generation produces exactly one id per request, shared by all TRoE rows.
//
char* troeTxId(void)
{
  if (orionldState.troeTxId[0] == 0)
    uuidGenerate(orionldState.troeTxId, sizeof(orionldState.troeTxId), "urn:ngsi-ld:tx:");

  return orionldState.troeTxId;
}
