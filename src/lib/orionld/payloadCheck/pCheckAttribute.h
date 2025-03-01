#ifndef SRC_LIB_ORIONLD_PAYLOADCHECK_PCHECKATTRIBUTE_H_
#define SRC_LIB_ORIONLD_PAYLOADCHECK_PCHECKATTRIBUTE_H_

/*
*
* Copyright 2022 FIWARE Foundation e.V.
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
extern "C"
{
#include "kjson/KjNode.h"                                     // KjNode
}

#include "orionld/types/OrionldAttributeType.h"               // OrionldAttributeType
#include "orionld/context/OrionldContextItem.h"               // OrionldContextItem



// -----------------------------------------------------------------------------
//
// pCheckAttribute -
//
// attrTypeFromDb is needed, only for PATCH Entity/Attribute, to make sure
// the attribute update isn't trying to modify the type of the attribute.
// API endpoints other than those two need not make this check as attributes are REPLACED.
//
// Likewise, in the second (recursive) call to pCheckAttribute for PATCH Attribute, it is not
// needed as all sub-attributes are REPLACED.
//
extern bool pCheckAttribute
(
  const char*             entityId,
  KjNode*                 attrP,
  bool                    isAttribute,
  OrionldAttributeType    attrTypeFromDb,
  bool                    attrNameAlreadyExpanded,
  OrionldContextItem*     attrContextInfoP
);

#endif  // SRC_LIB_ORIONLD_PAYLOADCHECK_PCHECKATTRIBUTE_H_
