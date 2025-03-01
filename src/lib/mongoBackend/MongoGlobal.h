#ifndef SRC_LIB_MONGOBACKEND_MONGOGLOBAL_H_
#define SRC_LIB_MONGOBACKEND_MONGOGLOBAL_H_

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
* Author: Fermín Galán
*/
#include <stdint.h>   // int64_t et al

#include <string>
#include <vector>
#include <map>

#include "mongo/client/dbclient.h"

#include "logMsg/logMsg.h"

#include "rest/uriParamNames.h"                // Default values for URI parameters
#include "common/RenderFormat.h"
#include "ngsi/EntityId.h"
#include "ngsi/ContextRegistrationAttribute.h"
#include "ngsi/ContextAttribute.h"
#include "ngsi/EntityIdVector.h"
#include "ngsi/StringList.h"
#include "ngsi/ContextElementResponseVector.h"
#include "ngsi/ConditionValueList.h"
#include "ngsi/Restriction.h"
#include "ngsi/NotifyConditionVector.h"
#include "ngsi10/UpdateContextResponse.h"
#include "ngsi9/RegisterContextRequest.h"
#include "ngsi9/RegisterContextResponse.h"
#include "ngsiNotify/Notifier.h"
#include "apiTypesV2/Subscription.h"
#include "apiTypesV2/HttpInfo.h"
#include "mongoBackend/TriggeredSubscription.h"
#include "orionld/types/OrionldTenant.h"                       // OrionldTenant



/* ****************************************************************************
*
* mongoMultitenant -
*/
extern bool mongoMultitenant(void);



/* ****************************************************************************
*
* mongoInit -
*/
void mongoInit
(
  const char*  dbHost,
  const char*  rplSet,
  std::string  dbName,
  const char*  user,
  const char*  pwd,
  bool         mtenant,
  int64_t      timeout,
  int          writeConcern,
  int          dbPoolSize,
  bool         mutexTimeStat
);



/* ****************************************************************************
*
* mongoStart - 
*/
extern bool mongoStart
(
  const char* host,
  const char* db,
  const char* rplSet,
  const char* username,
  const char* passwd,
  bool        _multitenant,
  double      timeout,
  int         writeConcern = 1,
  int         poolSize     = 10,
  bool        semTimeStat  = false
);



#ifdef UNIT_TEST
extern void setMongoConnectionForUnitTest(mongo::DBClientBase* _connection);
#endif



/* ****************************************************************************
*
* getNotifier -
*/
extern Notifier* getNotifier(void);



/* ****************************************************************************
*
* setNotifier -
*/
extern void setNotifier(Notifier* n);



/* ****************************************************************************
*
* getMongoConnection -
*
* I would prefer to have per-collection methods, to have a better encapsulation, but
* the Mongo C++ API seems not to work that way
*/
extern mongo::DBClientBase* getMongoConnection(void);



/* ****************************************************************************
*
* releaseMongoConnection - 
*/
extern void releaseMongoConnection(mongo::DBClientBase* connection);



/* ****************************************************************************
*
* setDbPrefix -
*/
extern void setDbPrefix(const std::string& dbPrefix);



/* ****************************************************************************
*
* getDbPrefix -
*/
extern const std::string& getDbPrefix(void);



/* ****************************************************************************
*
* getOrionDatabases -
*
* Return the list of Orion databases (the ones that start with the dbPrefix + "_").
* Note that the DB belonging to the default service is not included in the
* returned list
*
* Function return value is false in the case of problems accessing database,
* true otherwise.
*/
extern bool getOrionDatabases(std::vector<std::string>* dbs);



/* ****************************************************************************
*
* tenantFromDb -
*/
extern std::string tenantFromDb(const std::string& database);



/* ****************************************************************************
*
* mongoLocationCapable -
*/
extern bool mongoLocationCapable(void);



/* ****************************************************************************
*
* mongoExpirationCapable -
*/
extern bool mongoExpirationCapable(void);



/* ****************************************************************************
*
* ensureIdIndex -
*/
extern void ensureIdIndex(OrionldTenant* tenantP);



/* ****************************************************************************
*
* ensureLocationIndex -
*/
extern void ensureLocationIndex(OrionldTenant* tenantP);



/* ****************************************************************************
*
* ensureDateExpirationIndex -
*/
extern void ensureDateExpirationIndex(OrionldTenant* tenantP);



/* ****************************************************************************
*
* matchEntity -
*/
extern bool matchEntity(const EntityId* en1, const EntityId* en2);



/* ****************************************************************************
*
* includedEntity -
*/
extern bool includedEntity(EntityId en, const EntityIdVector& entityIdV);



/* ****************************************************************************
*
* includedAttribute -
*/
extern bool includedAttribute(const ContextRegistrationAttribute& attr, const StringList& attrsV);



/* *****************************************************************************
*
* processAreaScopeV2 -
*
*/
extern bool processAreaScopeV2(const Scope* scoP, mongo::BSONObj* areaQueryP);



/* ****************************************************************************
*
* entitiesQuery -
*/
extern bool entitiesQuery
(
  const EntityIdVector&            enV,
  const StringList&                attrL,
  const StringList&                metadataList,
  const Restriction&               res,
  ContextElementResponseVector*    cerV,
  std::string*                     err,
  bool                             includeEmpty,
  OrionldTenant*                   tenantP,
  const std::vector<std::string>&  servicePath,
  int                              offset         = DEFAULT_PAGINATION_OFFSET_INT,
  int                              limit          = DEFAULT_PAGINATION_LIMIT_INT,
  bool*                            limitReached   = NULL,
  long long*                       countP         = NULL,
  char*                            sortOrderList  = NULL,
  ApiVersion                       apiVersion     = V1
);



/* ****************************************************************************
*
* pruneContextElements -
*/
extern void pruneContextElements(const ContextElementResponseVector& oldCerV, ContextElementResponseVector* newCerVP);



/* ****************************************************************************
*
* registrationsQuery -
*/
extern bool registrationsQuery
(
  const EntityIdVector&               enV,
  const StringList&                   attrL,
  ContextRegistrationResponseVector*  crrV,
  std::string*                        err,
  OrionldTenant*                      tenantP,
  const std::vector<std::string>&     servicePathV,
  int                                 offset       = DEFAULT_PAGINATION_OFFSET_INT,
  int                                 limit        = DEFAULT_PAGINATION_LIMIT_INT,
  bool                                details      = false,
  long long*                          countP       = NULL
);



/* ****************************************************************************
*
* condValueAttrMatch -
*/
extern bool condValueAttrMatch(const mongo::BSONObj& sub, const std::vector<std::string>& modifiedAttrs);



/* ****************************************************************************
*
* subToEntityIdVector -
*
* Extract the entity ID vector from a BSON document (in the format of the csubs
* collection)
*/
extern EntityIdVector subToEntityIdVector(const mongo::BSONObj& sub);



/* ****************************************************************************
*
* subToAttributeList -
*
* Extract the attribute list from a BSON document (in the format of the csubs collection)
*/
extern StringList subToAttributeList(const mongo::BSONObj* subP);



/* ****************************************************************************
*
* processConditionVector -
*
* NGSIv2 wrapper
*/
extern mongo::BSONArray processConditionVector
(
  const std::vector<std::string>&    condAttributesV,
  const std::vector<ngsiv2::EntID>&  entitiesV,
  const std::vector<std::string>&    notifAttributesV,
  const std::vector<std::string>&    metadataV,
  const std::string&                 subId,
  const ngsiv2::HttpInfo&            httpInfo,
  bool*                              notificationDone,
  RenderFormat                       renderFormat,
  OrionldTenant*                     tenantP,
  const char*                        xauthToken,
  const std::vector<std::string>&    servicePathV,
  const Restriction*                 resP,
  const std::string&                 status,
  const std::string&                 fiwareCorrelator,
  const std::vector<std::string>&    attrsOrder,
  bool                               blacklist
);



/* ****************************************************************************
*
* processAvailabilitySubscriptions -
*/
extern bool processAvailabilitySubscription(
    const EntityIdVector& enV,
    const StringList&     attrL,
    const std::string&    subId,
    const std::string&    notifyUrl,
    RenderFormat          renderFormat,
    OrionldTenant*        tenantP,
    const std::string&    fiwareCorrelator
);



/* ****************************************************************************
*
* slashEscape - 
*
* When the 'to' buffer is full, slashEscape returns.
* No warnings, no nothing.
* Make sure 'to' is big enough!
*/
extern void slashEscape(const char* from, char* to, unsigned int toLen);



/* ****************************************************************************
*
* releaseTriggeredSubscriptions -
*/
extern void releaseTriggeredSubscriptions(std::map<std::string, TriggeredSubscription*>* subsP);



/* ****************************************************************************
*
* fillQueryServicePath -
*/
extern mongo::BSONObj fillQueryServicePath(const std::vector<std::string>& servicePath);



/* ****************************************************************************
*
* fillContextProviders -
*/
extern void fillContextProviders(ContextElementResponse* cer, const ContextRegistrationResponseVector& crrV);



/* ****************************************************************************
*
* someContextElementNotFound -
*/
extern bool someContextElementNotFound(const ContextElementResponse& cer);



/* ****************************************************************************
*
* cprLookupByAttribute -
*/
extern void cprLookupByAttribute
(
  EntityId&                                en,
  const std::string&                       attrName,
  const ContextRegistrationResponseVector& crrV,
  std::string*                             perEntPa,
  MimeType*                                perEntPaMimeType,
  std::string*                             perAttrPa,
  MimeType*                                perAttrPaMimeType
);



#ifdef ORIONLD
/* ****************************************************************************
*
* mongoIdentifier - create a unique identifier using OID
*/
extern char* mongoIdentifier(char* buffer);

#endif  // ORIONLD

#endif  // SRC_LIB_MONGOBACKEND_MONGOGLOBAL_H_
