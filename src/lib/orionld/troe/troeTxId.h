#ifndef SRC_LIB_ORIONLD_TROE_TROETXID_H_
#define SRC_LIB_ORIONLD_TROE_TROETXID_H_

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



// -----------------------------------------------------------------------------
//
// troeTxId - return the common transaction/snapshot id for the current request
//
// All TRoE rows (entities, attributes, subAttributes) written while serving a single
// request share this identifier, giving a guaranteed-unique key that links all attributes
// of one write operation - something instanceId (unique per attribute), observedAt and ts
// (not guaranteed unique) cannot provide. The id is generated lazily on first use and reset
// per request (orionldState is zeroed in orionldStateInit).
//
extern char* troeTxId(void);

#endif  // SRC_LIB_ORIONLD_TROE_TROETXID_H_
