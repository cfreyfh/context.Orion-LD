#ifndef SRC_LIB_LOGMSG_TRACELEVELS_H_
#define SRC_LIB_LOGMSG_TRACELEVELS_H_

/*
*
* Copyright 2013 Telefonica Investigacion y Desarrollo, S.A.U
*
* This file is part of Orion Context Broker.
*
* Orion Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* iot_support at tid dot es
*
* Author: Ken Zangelin
*/



/* ****************************************************************************
*
* TraceLevels -
*/
typedef enum TraceLevels
{
  //
  // Requests and responses
  //
  LmtRequest = 30,                     // Incoming requests
  LmtResponse,                         // HTTP Response
  LmtHeaders,                          // HTTP Headers
  LmtUriParams,                        // HTTP URI Parameters
  LmtUriParamOptions,                  // HTTP URI Parameter 'options'

  //
  // Alterations and Notifications
  //
  LmtAlt = 40,                         // Notifications: Alterations
  LmtRegMatch,                         // Distributed Operations: registration matching
  LmtWatchedAttributes,                // Watched attributes in subscriptions
  LmtNotificationMsg,                  // Notifications: Messages
  LmtNotificationStats,                // Errors and timestamps for subscriptions
  LmtNotificationSend,                 // Sending of notifications
  LmtNotificationHeaders,              // notification request/response headers
  LmtNotificationBody,                 // notification request/response body
  LmtShowChanges,                      // Add the field 'previousX' to attributes in notifications

  //
  // Subscription Cache
  //
  LmtSubCache = 50,                    // Subscription Cache
  LmtSubCacheMatch,                    // Subscription Cache Matches
  LmtSubCacheDebug,                    // Subscription Cache Debug
  LmtSubCacheSync,                     // Subscription Cache Refresh
  LmtSubCacheStats,                    // Errors and timestamps in sub-cache

  //
  // Registration Cache
  //
  LmtRegCache = 60,                    // Registration Cache

  //
  // Distributed Operations
  //
  LmtDistOpMsgs = 70,                  // Distributed Operations: messages
  LmtDistOpRequest,                    // ONLY the verb, path, and body of a distributed request
  LmtDistOpResponse,                   // ONLY the body and status code of the response to a distributed request
  LmtDistOp207,                        // Merging of the final 207 response
  LmtDistOpResponseBuf,                // Specific debugging of the incoming response of a distributed message
  LmtDistOpResponseDetail,             // Details on responses to distributed requests
  LmtDistOpResponseHeaders,            // HTTP headers of responses to distributed requests
  LmtDistOpRequestHeaders,             // HTTP headers of request of distributed requests
  LmtDistOpList,                       // Linked list of DistOps

  //
  // Context
  //
  LmtContexts = 80,                    // Contexts
  LmtContextTree,                      // Context Tree
  LmtContextCache,                     // Context Cache
  LmtCoreContext,                      // Core Context

  // GeoJSON
  LmtGeoJSON = 90,                     // GeoJSON ... everything (for now)

  //
  // Misc
  //
  LmtMongoc = 230,                     // Entire mongoc library
  LmtSR,                               // Service Routine (whatever it is doing)
  LmtSemaphore,                        // Semaphores
  LmtKjlParse,                         // Trace level start for K libs
  LmtLegacy,                           // Old code (mongoBackend, json parsers, etc)
  LmtLegacySubMatch,                   // Old code - update/subscription match for subs/notifs
  LmtLegacySubCacheRefresh,            // Old code - sub-cache-refresh
  LmtMqtt,                             // MQTT notifications
  LmtQ,                                // Query language
  LmtSql,                              // SQL command for TRoE
  LmtPgPool,                           // Postgres Connection Pool
  LmtSocketService,                    // Socket Service

  LmtCurl    = 250,                    // CURL library
  LmtToDo,                             // To Do list
  LmtPatchEntity,                      // Real merge+patch
  LmtPatchEntity2,                     // Real merge+patch: merging for final API Entity, for notifications
  LmtSysAttrs,                         // System Attributes
  LmtLeak                              // Used when debugging leaks and valgrind errors
} TraceLevels;



/* ****************************************************************************
*
* traceLevelName -
*/
extern char* traceLevelName(TraceLevels level);

#endif  // SRC_LIB_LOGMSG_TRACELEVELS_H_
