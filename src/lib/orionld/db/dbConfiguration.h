#ifndef SRC_LIB_ORIONLD_DB_DBCONFIGURATION_H_
#define SRC_LIB_ORIONLD_DB_DBCONFIGURATION_H_

/*
*
* Copyright 2019 FIWARE Foundation e.V.
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
#include "kjson/KjNode.h"                                        // KjNode
}

#include "orionld/q/QNode.h"                                     // QNode
#include "orionld/types/OrionldProblemDetails.h"                 // OrionldProblemDetails
#include "orionld/types/OrionldTenant.h"                         // OrionldTenant



// -----------------------------------------------------------------------------
//
// DB_DRIVER_MONGO_CPP_LEGACY - Use the mongo C++ Legacy driver
//
#define DB_DRIVER_MONGO_CPP_LEGACY 1



// -----------------------------------------------------------------------------
//
// DB_DRIVER_MONGOC - Use the "newest" mongo C driver
//
// #define DB_DRIVER_MONGOC 1



// -----------------------------------------------------------------------------
//
// Callback types for the DB interface
//
typedef bool    (*DbSubscriptionMatchCallback)(const char* entityId, KjNode* subscriptionTree, KjNode* currentEntityTree, KjNode* incomingRequestTree);



// -----------------------------------------------------------------------------
//
// Function pointer types for the DB interface
//
typedef KjNode* (*DbEntitiesGet)(char** fieldV, int fields, bool entityIdPresent);
typedef KjNode* (*DbEntityTypesFromRegistrationsGet)(bool details);



// -----------------------------------------------------------------------------
//
// Function pointers for the DB interface - later
//

#endif  // SRC_LIB_ORIONLD_DB_DBCONFIGURATION_H_
