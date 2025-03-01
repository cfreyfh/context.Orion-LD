/*
*
* Copyright 2015 Telefonica Investigacion y Desarrollo, S.A.U
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
#include <sys/types.h>
#include <regex.h>
#include <string.h>

#include <string>
#include <vector>
#include <map>

extern "C"
{
#include "kbase/kTime.h"                             // kTimeGet
#include "kalloc/kaStrdup.h"                         // kaStrdup
#include "kalloc/kaBufferReset.h"                    // kaBufferReset
#include "kjson/kjFree.h"                            // kjFree
}

#include "logMsg/logMsg.h"

#include "common/sem.h"
#include "common/string.h"
#include "apiTypesV2/HttpInfo.h"
#include "apiTypesV2/Subscription.h"
#include "mongoBackend/MongoGlobal.h"
#include "mongoBackend/mongoSubCache.h"
#include "ngsi10/SubscribeContextRequest.h"
#include "alarmMgr/alarmMgr.h"

#include "orionld/types/OrionldTenant.h"                    // OrionldTenant
#include "orionld/common/orionldState.h"                    // orionldState
#include "orionld/common/tenantList.h"                      // tenantList
#include "orionld/common/urlParse.h"                        // urlParse
#include "orionld/q/qBuild.h"                               // qBuild
#include "orionld/q/qRelease.h"                             // qRelease
#include "orionld/mongoc/mongocSubCachePopulateByTenant.h"  // mongocSubCachePopulateByTenant
#include "orionld/mongoc/mongocSubCountersUpdate.h"         // mongocSubCountersUpdate
#include "orionld/context/orionldContextFromUrl.h"          // orionldContextFromUrl

#include "cache/subCache.h"

using std::map;



/* ****************************************************************************
*
* subCacheState - maintains the state of the subscription cache
*/
volatile SubCacheState subCacheState = ScsIdle;


//
// The subscription cache maintains in memory all subscriptions of type ONCHANGE.
// The reason for this cache is to avoid a very slow mongo operation ... (Fermin)
//
// The 'mongo part' of the cache is implemented in mongoBackend/mongoSubCache.cpp/h and used in:
//   - MongoCommonUpdate.cpp               (in function addTriggeredSubscriptions_withCache)
//   - mongoSubscribeContext.cpp           (in function mongoSubscribeContext)
//   - mongoUnsubscribeContext.cpp         (in function mongoUnsubscribeContext)
//   - mongoUpdateContextSubscription.cpp  (in function mongoUpdateContextSubscription)
//   - contextBroker.cpp                   (to initialize and sybchronize)
//
// To manipulate the subscription cache, a semaphore is necessary, as various threads can be
// reading/inserting/removing subs at the same time.
// This semaphore is NOT optional, like the mongo request semaphore.
//
// Two functions have been added to common/sem.cpp|h for this: cacheSemTake/Give.
//


/* ****************************************************************************
*
* EntityInfo::EntityInfo -
*/
EntityInfo::EntityInfo
(
  const std::string&  _entityId,
  const std::string&  _entityType,
  const std::string&  _isPattern,
  bool                _isTypePattern
)
:
entityId(_entityId), entityType(_entityType), isTypePattern(_isTypePattern)
{
  isPattern    = (_isPattern == "true") || (_isPattern == "TRUE") || (_isPattern == "True");

  if (isPattern)
  {
    // FIXME P5: recomp error should be captured? have a look to other usages of regcomp()
    // in order to see how it works
    if (regcomp(&entityIdPattern, _entityId.c_str(), REG_EXTENDED) != 0)
    {
      alarmMgr.badInput(clientIp, "invalid regular expression for idPattern");
      isPattern = false;  // FIXME P6: this entity should not be let into the system. Must be stopped before.
                          //           Right here, best thing to do is simply to say it is not a regex
      entityIdPatternToBeFreed = false;
    }
    else
    {
      entityIdPatternToBeFreed = true;
    }
  }
  else
  {
    entityIdPatternToBeFreed = false;
  }

  if (isTypePattern)
  {
    // FIXME P5: recomp error should be captured? have a look to other usages of regcomp()
    // in order to see how it works
    if (regcomp(&entityTypePattern, _entityType.c_str(), REG_EXTENDED) != 0)
    {
      alarmMgr.badInput(clientIp, "invalid regular expression for typePattern");
      isTypePattern = false;  // FIXME P6: this entity should not be let into the system. Must be stopped before.
                          //           Right here, best thing to do is simply to say it is not a regex
      entityTypePatternToBeFreed = false;
    }
    else
    {
      entityTypePatternToBeFreed = true;
    }
  }
  else
  {
    entityTypePatternToBeFreed = false;
  }
}



/* ****************************************************************************
*
* EntityInfo::match -
*/
bool EntityInfo::match
(
  const std::string&  id,
  const std::string&  type
)
{
  bool matchedType = false;
  bool matchedId   = false;

  // Check id
  if (isPattern)
  {
    // REGEX-comparison this->entityIdPattern VS id
    matchedId =  (regexec(&entityIdPattern, id.c_str(), 0, NULL, 0) == 0);
  }
  else if (id == entityId)
  {
    matchedId =  true;
  }
  else
  {
    LM_T(LmtSubCacheMatch, ("No match due to Entity ID"));
    matchedId = false;
  }

  // short-circuit, optimization
  if (matchedId)
  {
    // Check type
    if (isTypePattern)
    {
      // REGEX-comparison this->entityTypePattern VS type
      matchedType = (regexec(&entityTypePattern, type.c_str(), 0, NULL, 0) == 0);
    }
    else if ((type != "")  && (entityType != "") && (entityType != type))
    {
      LM_T(LmtSubCacheMatch, ("No match due to Entity Type"));
      matchedType = false;
    }
    else
    {
      matchedType = true;
    }
  }

  return matchedId && matchedType;
}



/* ****************************************************************************
*
* EntityInfo::release -
*/
void EntityInfo::release(void)
{
  if (entityIdPatternToBeFreed)
  {
    regfree(&entityIdPattern);  // If regcomp fails it frees up itself (see glibc sources for details)
    entityIdPatternToBeFreed = false;
  }

  if (entityTypePatternToBeFreed)
  {
    regfree(&entityTypePattern);  // If regcomp fails it frees up itself (see glibc sources for details)
    entityTypePatternToBeFreed = false;
  }
}



/* ****************************************************************************
*
* SubCache -
*/
typedef struct SubCache
{
  CachedSubscription* head;
  CachedSubscription* tail;

  // Statistics counters
  int                 noOfRefreshes;
  int                 noOfInserts;
  int                 noOfRemoves;
  int                 noOfUpdates;
} SubCache;



/* ****************************************************************************
*
* subCache -
*/
static SubCache  subCache            = { NULL, NULL, 0, 0, 0, 0 };
bool             subCacheActive      = false;
bool             subCacheMultitenant = false;



/* ****************************************************************************
*
* subCacheHeadGet -
*/
CachedSubscription* subCacheHeadGet(void)
{
  return subCache.head;
}



/* ****************************************************************************
*
* subCacheInit -
*/
void subCacheInit(bool multitenant)
{
  subCacheMultitenant = multitenant;

  subCache.head   = NULL;
  subCache.tail   = NULL;

  subCacheStatisticsReset("subCacheInit");

  subCacheActive = true;
}



/* ****************************************************************************
*
* subCacheDisable -
*
*/
#ifdef UNIT_TEST
void subCacheDisable(void)
{
  subCacheActive = false;
}
#endif



/* ****************************************************************************
*
* subCacheItems -
*/
int subCacheItems(void)
{
  CachedSubscription* cSubP = subCache.head;
  int                 items = 0;

  while (cSubP != NULL)
  {
    ++items;
    cSubP = cSubP->next;
  }

  return items;
}



/* ****************************************************************************
*
* attributeMatch -
*/
static bool attributeMatch(CachedSubscription* cSubP, const std::vector<std::string>& attrV)
{
  if (cSubP->notifyConditionV.size() == 0)
  {
    return true;
  }

  for (unsigned int ncvIx = 0; ncvIx < cSubP->notifyConditionV.size(); ++ncvIx)
  {
    for (unsigned int aIx = 0; aIx < attrV.size(); ++aIx)
    {
      if (cSubP->notifyConditionV[ncvIx] == attrV[aIx])
      {
        return true;
      }
    }
  }

  return false;
}



/* ****************************************************************************
*
* servicePathMatch -
*/
static bool servicePathMatch(CachedSubscription* cSubP, char* servicePath)
{
  const char*  spath;

  // Special case. "/#" matches EVERYTHING
  if ((strlen(servicePath) == 2) && (servicePath[0] == '/') && (servicePath[1] == '#') && (servicePath[2] == 0))
  {
    return true;
  }

  //
  // Empty service path?
  //
  if ((strlen(servicePath) == 0) && (strlen(cSubP->servicePath) == 0))
  {
    return true;
  }

  //
  // Default service path for a subscription is "/".
  // So, if empty, set it to "/"
  //
  if (servicePath[0] == 0)
  {
    servicePath = (char*) "/";
  }

  spath = cSubP->servicePath;

  //
  // No wildcard - exact match
  //
  if (spath[strlen(spath) - 1] != '#')
  {
    return (strcmp(servicePath, cSubP->servicePath) == 0);
  }


  //
  // Wildcard - remove '#' and compare to the length of _servicePath.
  //            If equal upto the length of _servicePath, then it is a match
  //
  // Actually, there is more than one way to match here:
  // If we have a servicePath of the subscription as
  // "/a/b/#", then the following service-paths must match:
  //   1. /a/b
  //   2. /a/b/ AND /a/b/.+  (any path below /a/b/)
  //
  // What should NOT match is "/a/b2".
  //
  unsigned int len = strlen(spath) - 2;

  // 1. /a/b - (removing 2 chars from the cache-subscription removes "/#"

  if ((spath[len] == '/') && (strlen(servicePath) == len) && (strncmp(spath, servicePath, len) == 0))
  {
    return true;
  }

  // 2. /a/b/.+
  len = strlen(spath) - 1;
  if (strncmp(spath, servicePath, len) == 0)
  {
    return true;
  }

  return false;
}



/* ****************************************************************************
*
* subMatch -
*/
static bool subMatch
(
  CachedSubscription*              cSubP,
  const char*                      tenant,
  const char*                      servicePath,
  const char*                      entityId,
  const char*                      entityType,
  const std::vector<std::string>&  attrV
)
{
  //
  // Check to filter out due to tenant - only valid if Broker has started with -multiservice option
  //
  if (subCacheMultitenant == true)
  {
    if ((cSubP->tenant == NULL) || (tenant == NULL) || (cSubP->tenant[0] == 0) || (tenant[0] == 0))
    {
      if ((cSubP->tenant != NULL) && (cSubP->tenant[0] != 0))
      {
        // No match due to tenant I
        LM_T(LmtSubCacheMatch, ("No match due to tenant I"));
        return false;
      }

      if ((tenant != NULL) && (tenant[0] != 0))
      {
        // No match due to tenant II
        LM_T(LmtSubCacheMatch, ("No match due to tenant II"));
        return false;
      }
    }
    else if (strcmp(cSubP->tenant, tenant) != 0)
    {
      // No match due to tenant III
      LM_T(LmtSubCacheMatch, ("No match due to tenant III"));
      return false;
    }
  }

  if (servicePathMatch(cSubP, (char*) servicePath) == false)
  {
    // No match due to servicePath
    LM_T(LmtSubCacheMatch, ("No match due to servicePath"));
    return false;
  }


  //
  // If ONCHANGE and one of the attribute names in the scope vector
  // of the subscription has the same name as the incoming attribute. there is a match.
  // Additionaly, if the attribute list in cSubP is empty, there is a match (this is the
  // case of ONANYCHANGE subscriptions).
  //
  if (!attributeMatch(cSubP, attrV))
  {
    // No match due to attributes
    LM_T(LmtSubCacheMatch, ("No match due to attributes"));
    return false;
  }

  for (unsigned int ix = 0; ix < cSubP->entityIdInfos.size(); ++ix)
  {
    EntityInfo* eiP = cSubP->entityIdInfos[ix];

    if (eiP->match(entityId, entityType))
      return true;
  }

  // No match due to EntityInfo
  return false;
}



/* ****************************************************************************
*
* subCacheMatch -
*/
int subCacheMatch
(
  const char*                        tenant,
  const char*                        servicePath,
  const char*                        entityId,
  const char*                        entityType,
  const char*                        attr,
  std::vector<CachedSubscription*>*  subVecP
)
{
  CachedSubscription* cSubP   = subCache.head;
  int                 matches = 0;

  while (cSubP != NULL)
  {
    std::vector<std::string> attrV;

    attrV.push_back(attr);

    if (subMatch(cSubP, tenant, servicePath, entityId, entityType, attrV))
    {
      ++matches;
      subVecP->push_back(cSubP);
    }

    cSubP = cSubP->next;
  }

  return matches;
}



/* ****************************************************************************
*
* subCacheMatch -
*/
int subCacheMatch
(
  const char*                        tenant,
  const char*                        servicePath,
  const char*                        entityId,
  const char*                        entityType,
  const std::vector<std::string>&    attrV,
  std::vector<CachedSubscription*>*  subVecP
)
{
  CachedSubscription* cSubP   = subCache.head;
  int                 matches = 0;

  while (cSubP != NULL)
  {
    if (subMatch(cSubP, tenant, servicePath, entityId, entityType, attrV))
    {
      subVecP->push_back(cSubP);
      ++matches;
    }

    cSubP = cSubP->next;
  }

  return matches;
}



// -----------------------------------------------------------------------------
//
// subCacheItemStrip -
//
void subCacheItemStrip(CachedSubscription* cSubP)
{
  if (cSubP->subscriptionId != NULL)
  {
    free(cSubP->subscriptionId);
    cSubP->subscriptionId = NULL;
  }

  if (cSubP->description != NULL)
  {
    free(cSubP->description);
    cSubP->description = NULL;
  }

  if (cSubP->url != NULL)
  {
    free(cSubP->url);
    cSubP->url = NULL;
  }

  if (cSubP->protocolString != NULL)
  {
    free(cSubP->protocolString);
    cSubP->protocolString = NULL;
  }

  if (cSubP->ip != NULL)
  {
    free(cSubP->ip);
    cSubP->ip = NULL;
  }

  if (cSubP->rest != NULL)
  {
    free(cSubP->rest);
    cSubP->rest = NULL;
  }

  if (cSubP->tenant != NULL)
  {
    free(cSubP->tenant);
    cSubP->tenant = NULL;
  }

  if (cSubP->servicePath != NULL)
  {
    free(cSubP->servicePath);
    cSubP->servicePath = NULL;
  }

  if (cSubP->qText != NULL)
  {
    free(cSubP->qText);
    cSubP->qText = NULL;
  }

  for (unsigned int ix = 0; ix < cSubP->entityIdInfos.size(); ++ix)
  {
    cSubP->entityIdInfos[ix]->release();
    delete cSubP->entityIdInfos[ix];
  }

  cSubP->entityIdInfos.clear();

  while (cSubP->attributes.size() > 0)
  {
    cSubP->attributes.erase(cSubP->attributes.begin());
  }
  cSubP->attributes.clear();

  cSubP->notifyConditionV.clear();

  if (cSubP->qP != NULL)
  {
    qRelease(cSubP->qP);
    cSubP->qP = NULL;
  }

  if (cSubP->geoCoordinatesP != NULL)
  {
    kjFree(cSubP->geoCoordinatesP);
    cSubP->geoCoordinatesP = NULL;
  }

  for (int ix = 0; ix < (int) cSubP->httpInfo.notifierInfo.size(); ++ix)
  {
    if (cSubP->httpInfo.notifierInfo[ix] != NULL)
    {
      free(cSubP->httpInfo.notifierInfo[ix]);
      cSubP->httpInfo.notifierInfo[ix] = NULL;
    }
  }
}



/* ****************************************************************************
*
* subCacheItemDestroy -
*/
void subCacheItemDestroy(CachedSubscription* cSubP)
{
  subCacheItemStrip(cSubP);

  cSubP->next = NULL;
}



/* ****************************************************************************
*
* subCacheDestroy -
*/
void subCacheDestroy(void)
{
  CachedSubscription* cSubP  = subCache.head;

  if (subCache.head == NULL)
    return;

  while (cSubP->next)
  {
    CachedSubscription* prev = cSubP;

    cSubP = cSubP->next;
    subCacheItemDestroy(prev);
    delete prev;
  }

  subCacheItemDestroy(cSubP);
  delete cSubP;

  subCache.head  = NULL;
  subCache.tail  = NULL;
}



/* ****************************************************************************
*
* tenantMatch -
*/
bool tenantMatch(const char* tenant1, const char* tenant2)
{
  //
  // Removing complications with NULL, giving NULL tenants the value of an empty string;
  //
  tenant1 = (tenant1 == NULL)? "" : tenant1;
  tenant2 = (tenant2 == NULL)? "" : tenant2;

  return (strcmp(tenant1, tenant2) == 0);
}



/* ****************************************************************************
*
* subCacheItemLookup -
*
* FIXME P7: lookups would be A LOT faster if the subCache used a hash-table instead of
*           just a single linked link for the subscriptions.
*           The hash could be adding each char in 'tenant' and 'subscriptionId' to a 8-bit
*           integer and then we'd have a vector of 256 linked lists.
*
*           However, for matching against chang3ed entities, which is 99% of the usage of the cache,
*           nothing would be gained so ... let's not do this.
*/
CachedSubscription* subCacheItemLookup(const char* tenant, const char* subscriptionId)
{
  CachedSubscription* cSubP = subCache.head;

  while (cSubP != NULL)
  {
    if ((tenantMatch(tenant, cSubP->tenant)) && (strcmp(subscriptionId, cSubP->subscriptionId) == 0))
      return cSubP;

    cSubP = cSubP->next;
  }

  return NULL;
}



/* ****************************************************************************
*
* subCacheUpdateStatisticsIncrement -
*/
void subCacheUpdateStatisticsIncrement(void)
{
  ++subCache.noOfUpdates;
}



/* ****************************************************************************
*
* subCacheItemInsert -
*
* First of all, insertion is done at the end of the list, so,
* cSubP->next must ALWAYS be zero at insertion time
*
* Note that this is the insert function that *really inserts* the
* CachedSubscription in the list of CachedSubscriptions.
*
* All other subCacheItemInsert functions create the subscription and then
* calls this function.
*
* So, the subscription itself is untouched by this function, is it ONLY inserted
* in the list (only the 'next' field is modified).
*
*/
void subCacheItemInsert(CachedSubscription* cSubP)
{
  //
  // CachedSubscripion modification:
  // - Triggers
  // -
  //

  //
  // Triggers - FIXME: hardcoded all triggers to be always ON - needs to be implemented
  //
  int triggerArraySize = sizeof(cSubP->triggers) / sizeof(cSubP->triggers[0]);

  for (int ix = 0; ix < triggerArraySize; ++ix)
  {
    cSubP->triggers[ix] = true;
  }

  //
  // List Insertion Part
  //
  cSubP->next = NULL;

  ++subCache.noOfInserts;

  // First insertion?
  if ((subCache.head == NULL) && (subCache.tail == NULL))
  {
    subCache.head   = cSubP;
    subCache.tail   = cSubP;

    return;
  }

  subCache.tail->next  = cSubP;
  subCache.tail        = cSubP;
}



/* ****************************************************************************
*
* subCacheItemInsert - create a new sub, fill it in, and add it to cache
*
* Note that 'count', which is the counter of how many times a notification has been
* fired for a subscription is set to 0 or 1. It is set to 1 only if the subscription
* has made a notification to be triggered/fired upon creation-time of the subscription.
* UPDATE: Initial Notifications have been removed => always 0
*
* Who calls this function?
* ONLY mongoCreateSubscription (via a static function called "insertInCache")
* Meaning, it's only "old" stuff, either NGSIv2, or NGSI-LD using mongoBackend.
*
*/
bool subCacheItemInsert
(
  const char*                        tenant,
  const char*                        servicePath,
  const ngsiv2::HttpInfo&            httpInfo,
  const std::vector<ngsiv2::EntID>&  entIdVector,
  const std::vector<std::string>&    attributes,
  const std::vector<std::string>&    metadata,
  const std::vector<std::string>&    conditionAttrs,
  const char*                        subscriptionId,
  double                             expirationTime,
  double                             throttling,
  RenderFormat                       renderFormat,
  bool                               notificationDone,
  double                             lastNotificationTime,
  double                             lastNotificationSuccessTime,
  double                             lastNotificationFailureTime,
  StringFilter*                      stringFilterP,
  StringFilter*                      mdStringFilterP,
  const std::string&                 status,
  const std::string&                 name,
  const std::string&                 ldContext,
  const std::string&                 lang,
  const char*                        mqttUserName,
  const char*                        mqttPassword,
  const char*                        mqttVersion,
  int                                mqttQoS,
  const std::string&                 q,
  const std::string&                 geometry,
  const std::string&                 coords,
  const std::string&                 georel,
  const std::string&                 geoproperty,
  bool                               blacklist
)
{
  //
  // Add the subscription to the subscription cache.
  //
  CachedSubscription* cSubP = new CachedSubscription();

  //
  // First the non-complex values
  //
  cSubP->tenant                = (tenant[0] == 0)? NULL : strdup(tenant);
  cSubP->expirationTime        = expirationTime;
  cSubP->throttling            = throttling;
  cSubP->lastNotificationTime  = lastNotificationTime;
  cSubP->lastFailure           = lastNotificationFailureTime;
  cSubP->lastSuccess           = lastNotificationSuccessTime;
  cSubP->renderFormat          = renderFormat;
  cSubP->next                  = NULL;
  cSubP->count                 = 0;
  cSubP->status                = status;
  cSubP->url                   = NULL;
  cSubP->ip                    = NULL;
  cSubP->protocolString        = NULL;
  cSubP->rest                  = NULL;

  double now = getCurrentTime();
  if ((cSubP->expirationTime > 0) && (cSubP->expirationTime < now))
  {
    cSubP->status   = "expired";
    cSubP->isActive = false;
  }
  else if (cSubP->status == "")
    cSubP->status = "active";

  if (cSubP->status == "active")
    cSubP->isActive = true;
  else
    cSubP->isActive = false;


  cSubP->name                    = name;
  cSubP->expression.geoproperty  = geoproperty;

  if (ldContext != "")
  {
    cSubP->ldContext = ldContext;
    cSubP->lang      = lang;
    cSubP->contextP  = orionldContextFromUrl((char*) cSubP->ldContext.c_str(), NULL);

    if (cSubP->contextP == NULL)
    {
      LM_E(("Internal Error (%s: %s)", orionldState.pd.title, orionldState.pd.status));
      cSubP->contextP = orionldState.contextP;
    }
  }
  else
    cSubP->ldContext = "";

  // Counters and stats
  cSubP->lastNotificationTime  = 0;  // Or, does this come from DB ?
  cSubP->lastSuccess           = 0;  // Or, does this come from DB ?
  cSubP->lastFailure           = 0;  // Or, does this come from DB ?
  cSubP->consecutiveErrors     = 0;
  cSubP->lastErrorReason[0]    = 0;
  cSubP->dirty                 = 0;

  //
  // The three 'q's
  //
  cSubP->qText      = NULL;
  cSubP->qP         = NULL;

  bool validForV2 = true;
  bool isMq       = false;

  if (q != "")
  {
    if (orionldState.apiVersion == NGSI_LD_V1)
    {
      // FIXME: Instead of calling qBuild here, I should pass the pointer from pCheckSubscription
      if (cSubP->qP != NULL)
        qRelease(cSubP->qP);
      cSubP->qP = qBuild(q.c_str(), &cSubP->qText, &validForV2, &isMq, true);  // cSubP->qText needs real allocation

      if (cSubP->qText != NULL)
        cSubP->qText = strdup(cSubP->qText);
      else
      {
        delete cSubP;
        return false;
      }
    }
    else
      cSubP->qText = (char*) strdup(q.c_str());

    if (validForV2 == false)
    {
      cSubP->expression.q           = "P;!P";
      cSubP->expression.mq          = "P.P;!P.P";
    }
    else if (isMq)
    {
      cSubP->expression.mq          = cSubP->qText;
      cSubP->expression.q           = "P;!P";
    }
    else
    {
      cSubP->expression.q           = cSubP->qText;
      cSubP->expression.mq          = "P.P;!P.P";
    }
  }


  cSubP->expression.geometry    = geometry;
  cSubP->expression.coords      = coords;
  cSubP->expression.georel      = georel;
  cSubP->blacklist              = blacklist;
  cSubP->httpInfo               = httpInfo;
  cSubP->notifyConditionV       = conditionAttrs;
  cSubP->attributes             = attributes;
  cSubP->metadata               = metadata;


  //
  // servicePath and subscriptionId need strdup - done after possible failure (qBuild)
  //
  cSubP->servicePath           = strdup(servicePath);
  cSubP->subscriptionId        = strdup(subscriptionId);

  //
  // IP, port and rest
  //
  cSubP->url = strdup(httpInfo.url.c_str());
  urlParse(cSubP->url, &cSubP->protocolString, &cSubP->ip, &cSubP->port, &cSubP->rest);

  if (cSubP->protocolString != NULL)
  {
    cSubP->protocol       = protocolFromString(cSubP->protocolString);
    cSubP->protocolString = strdup(cSubP->protocolString);
  }

  if (cSubP->ip != NULL)
    cSubP->ip = strdup(cSubP->ip);

  if (cSubP->rest != NULL)
    cSubP->rest = strdup(cSubP->rest);

  //
  // String filters
  //
  std::string  errorString;
  if (stringFilterP != NULL)
  {
    //
    // NOTE (for both 'q' and 'mq' string filters)
    //   Here, the cached subscription should have a String Filter but if 'fill()' fails, it won't.
    //   The subscription is already in mongo and hopefully this erroneous situation is fixed
    //   once the sub-cache is refreshed.
    //
    //   This 'but' should be minimized once the issue 2082 gets implemented.
    //   [ Only reason for fill() to fail (apart from out-of-memory) seems to be an invalid regex ]
    //
    cSubP->expression.stringFilter.fill(stringFilterP, &errorString);
  }

  if (mdStringFilterP != NULL)
  {
    cSubP->expression.mdStringFilter.fill(mdStringFilterP, &errorString);
  }



  //
  // Convert all EntIds to EntityInfo
  //
  for (unsigned int ix = 0; ix < entIdVector.size(); ++ix)
  {
    const ngsiv2::EntID* eIdP = &entIdVector[ix];
    std::string          isPattern      = (eIdP->id   == "")? "true" : "false";
    bool                 isTypePattern  = (eIdP->type == "");
    std::string          id             = (eIdP->id   == "")? eIdP->idPattern   : eIdP->id;
    std::string          type           = (eIdP->type == "")? eIdP->typePattern : eIdP->type;

    EntityInfo* eP = new EntityInfo(id, type, isPattern, isTypePattern);

    cSubP->entityIdInfos.push_back(eP);
  }

  //
  // Insert the subscription in the cache
  //
  subCacheItemInsert(cSubP);
  return true;
}



/* ****************************************************************************
*
* subCacheStatisticsGet -
*/
void subCacheStatisticsGet
(
  int*   refreshes,
  int*   inserts,
  int*   removes,
  int*   updates,
  int*   items,
  char*  list,
  int    listSize
)
{
  *refreshes = subCache.noOfRefreshes;
  *inserts   = subCache.noOfInserts;
  *removes   = subCache.noOfRemoves;
  *updates   = subCache.noOfUpdates;
  *items     = subCacheItems();

  CachedSubscription* cSubP = subCache.head;

  //
  // NOTE
  //   If the listBuffer is big enough to hold the entire list of cached subscriptions,
  //   subCacheStatisticsGet returns that list for the response of "GET /cache/statistics".
  //   Minimum size of the list is set to 128 bytes and the needed size is detected later.
  //   If the list-buffer is not big enough to hold the entire list, a warning text is showed instead.
  //
  //
  *list = 0;
  if (listSize > 128)
  {
    while (cSubP != NULL)
    {
      char          msg[256];
      unsigned int  bytesLeft = listSize - strlen(list);

      snprintf(msg, sizeof(msg), "%s", cSubP->subscriptionId);

      //
      // If "msg" and ", " has no room in the "list", then no list is shown.
      // (the '+ 2' if for the string ", ")
      //
      if (strlen(msg) + 2 > bytesLeft)
      {
        snprintf(list, listSize, "too many subscriptions");
        break;
      }

      if (list[0] != 0)
        strcat(list, ", ");

      strcat(list, msg);

      cSubP = cSubP->next;
    }
  }
  else
  {
    snprintf(list, listSize, "too many subscriptions");
  }
}



/* ****************************************************************************
*
* subCacheStatisticsReset -
*/
void subCacheStatisticsReset(const char* by)
{
  subCache.noOfRefreshes  = 0;
  subCache.noOfInserts    = 0;
  subCache.noOfRemoves    = 0;
  subCache.noOfUpdates    = 0;
}



/* ****************************************************************************
*
* subCacheItemRemove -
*/
int subCacheItemRemove(CachedSubscription* cSubP)
{
  CachedSubscription* current = subCache.head;
  CachedSubscription* prev    = NULL;

  while (current != NULL)
  {
    if (current == cSubP)
    {
      // Removing first item ?
      if (cSubP == subCache.head)
      {
        subCache.head = cSubP->next;
      }

      // Removing last item?
      if (cSubP == subCache.tail)
      {
        subCache.tail = prev;
      }

      // Removing middle item?
      if (prev != NULL)
      {
        prev->next = cSubP->next;
      }

      ++subCache.noOfRemoves;

      subCacheItemDestroy(cSubP);
      delete cSubP;

      return 0;
    }

    prev    = current;
    current = current->next;
  }

  LM_E(("Runtime Error (item to remove from sub-cache not found)"));
  return -1;
}


#if 0
// -----------------------------------------------------------------------------
//
// subCacheDebug -
//
void subCacheDebug(const char* prefix, const char* title)
{
  CachedSubscription* subP = subCache.head;

  LM_T(LmtSubCacheDebug, ("%s%s", prefix, title));
  while (subP != NULL)
  {
    LM_T(LmtSubCacheDebug, ("%s  * Subscription %s:",       prefix, subP->subscriptionId));
    LM_T(LmtSubCacheDebug, ("%s    - lastNotification: %f", prefix, subP->lastNotificationTime));
    LM_T(LmtSubCacheDebug, ("%s    - lastSuccess:      %f", prefix, subP->lastSuccess));
    LM_T(LmtSubCacheDebug, ("%s    - lastFailure:      %f", prefix, subP->lastFailure));
    LM_T(LmtSubCacheDebug, ("%s    - timesSent:        %d", prefix, subP->count));
    LM_T(LmtSubCacheDebug, ("%s", prefix));

    subP = subP->next;
  }
}
#endif



/* ****************************************************************************
*
* subCacheRefresh -
*
* WARNING
*  The cache semaphore must be taken before this function is called:
*    cacheSemTake(__FUNCTION__, "Reason");
*  And released after subCacheRefresh finishes, of course.
*/
void subCacheRefresh(bool refresh)
{
  // subCacheDebug("KZ", "------------- BEFORE REFRESH ------------------------");

  LM_T(LmtSubCache, ("Refreshing sub-cache"));

  // Recreate the subCache for the default tenant
  if (experimental)
    mongocSubCachePopulateByTenant(&tenant0, refresh);
  else
  {
    // Empty the cache
    subCacheDestroy();
    mongoSubCacheRefresh(tenant0.mongoDbName);
  }

  // Recreate the subCache for each and every tenant
  OrionldTenant* tenantP = tenantList;
  while (tenantP != NULL)
  {
    if (experimental)
      mongocSubCachePopulateByTenant(tenantP, refresh);
    else
      mongoSubCacheRefresh(tenantP->mongoDbName);

    tenantP = tenantP->next;
  }

  ++subCache.noOfRefreshes;

  // subCacheDebug("KZ", "------------- AFTER REFRESH ------------------------");
}



/* ****************************************************************************
*
* CachedSubSaved -
*/
typedef struct CachedSubSaved
{
  double   lastNotificationTime;
  int64_t  count;
  double   lastFailure;
  double   lastSuccess;
  bool     ngsild;
} CachedSubSaved;



/* ****************************************************************************
*
* subCacheSync -
*
* 1. Save subscriptionId, lastNotificationTime, count, lastFailure, and lastSuccess for all items in cache (savedSubV)
* 2. Refresh cache (count set to 0)
* 3. Compare lastNotificationTime/lastFailure/lastSuccess in savedSubV with the new cache-contents and:
*    3.1 Update cache-items where 'saved lastNotificationTime' > 'cached lastNotificationTime'
*    3.2 Remember this more correct lastNotificationTime (must be flushed to mongo) -
*        by clearing out (set to 0) those lastNotificationTimes that are newer in cache
*    Same same with lastFailure and lastSuccess.
* 4. Update 'count' for each item in savedSubV where non-zero
* 5. Update 'lastNotificationTime/lastFailure/lastSuccess' for each item in savedSubV where non-zero
* 6. Free the vector created in step 1 - savedSubV
*
* NOTE
*   This function runs in a separate thread and it allocates temporal objects (in savedSubV).
*   If the broker dies when this function is executing, all these temporal objects will be reported
*   as memory leaks.
*   We see this in our valgrind tests, where we force the broker to die.
*   This is of course not a real leak, we only see this as a leak as the function hasn't finished to
*   execute until the point where the temporal objects are deleted (See '6. Free the vector savedSubV').
*   To fix this little problem, we have created a variable 'subCacheState' that is set to ScsSynchronizing while
*   the sub-cache synchronization is working.
*   In serviceRoutines/exitTreat.cpp this variable is checked and if iot is set to ScsSynchronizing, then a
*   sleep for a few seconds is performed before the broker exits (this is only for DEBUG compilations).
*/
void subCacheSync(void)
{
  std::map<std::string, CachedSubSaved*> savedSubV;

  cacheSemTake(__FUNCTION__, "Synchronizing subscription cache");
  subCacheState = ScsSynchronizing;


  //
  // 1. Save subscriptionId, lastNotificationTime, count, lastFailure, and lastSuccess for all items in cache
  //
  CachedSubscription* cSubP = subCache.head;

  while (cSubP != NULL)
  {
    //
    // FIXME P7: For some reason, sometimes the same subscription is found twice in the cache (Issue 2216)
    //           Once the issue 2216 is fixed, this if-block must be removed.
    //
    if (savedSubV[cSubP->subscriptionId] != NULL)
    {
      cSubP = cSubP->next;
      continue;
    }

    CachedSubSaved* cssP       = new CachedSubSaved();

    cssP->lastNotificationTime = cSubP->lastNotificationTime;
    cssP->count                = cSubP->count;       // This count is later pushed ($inc) to DB - needs to go to cache as well
    cssP->lastFailure          = cSubP->lastFailure;
    cssP->lastSuccess          = cSubP->lastSuccess;
    cssP->ngsild               = (cSubP->ldContext != "")? true : false;

    savedSubV[cSubP->subscriptionId] = cssP;

    cSubP = cSubP->next;
  }


  //
  // 2. Refresh cache (count set to 0)
  //
  LM_T(LmtSubCacheSync, ("================================= Refreshing subscription cache ====================="));
  subCacheRefresh(true);


  //
  // 3. Compare lastNotificationTime/lastFailure/lastSuccess in savedSubV with the new cache-contents
  //
  cSubP = subCache.head;
  while (cSubP != NULL)
  {
    CachedSubSaved* cssP = savedSubV[cSubP->subscriptionId];

    if (cssP != NULL)
    {
      if (cssP->lastNotificationTime < cSubP->lastNotificationTime)
      {
        // cssP->lastNotificationTime is older than what's currently in DB => throw away
        cssP->lastNotificationTime = 0;
      }

      if (cssP->lastFailure < cSubP->lastFailure)
      {
        // cssP->lastFailure is older than what's currently in DB => throw away
        cssP->lastFailure = 0;
      }

      if (cssP->lastSuccess < cSubP->lastSuccess)
      {
        // cssP->lastSuccess is older than what's currently in DB => throw away
        cssP->lastSuccess = 0;
      }
    }

    cSubP = cSubP->next;
  }


  //
  // 4. Update 'count' for each item in savedSubV where non-zero
  // 5. Update 'lastNotificationTime/lastFailure/lastSuccess' for each item in savedSubV where non-zero
  //
  cSubP = subCache.head;
  while (cSubP != NULL)
  {
    CachedSubSaved* cssP = savedSubV[cSubP->subscriptionId];

    if (cssP != NULL)
    {
      const char* tenant = (cSubP->tenant == NULL)? "" : cSubP->tenant;
      if (experimental == true)
        mongocSubCountersUpdate(tenant, cSubP, cssP->count, cssP->lastNotificationTime, cssP->lastFailure, cssP->lastSuccess, false, cssP->ngsild);
      else
      {
        mongoSubCountersUpdate(tenant,
                               cSubP->subscriptionId,
                               cssP->count,
                               cssP->lastNotificationTime,
                               cssP->lastFailure,
                               cssP->lastSuccess,
                               cssP->ngsild);
      }

      // Timestamps are put back into sub cache
      if (cssP->lastFailure          != 0) cSubP->lastFailure           = cssP->lastFailure;
      if (cssP->lastSuccess          != 0) cSubP->lastSuccess           = cssP->lastSuccess;
      if (cssP->lastNotificationTime != 0) cSubP->lastNotificationTime  = cssP->lastNotificationTime;

      // Here the delta (just $inc'ed to DB) is also inc'ed to subCache
      cSubP->dbCount += cssP->count;
      cSubP->count    = 0;
    }

    cSubP = cSubP->next;
  }


  //
  // 6. Free the vector savedSubV
  //
  for (std::map<std::string, CachedSubSaved*>::iterator it = savedSubV.begin(); it != savedSubV.end(); ++it)
  {
    delete it->second;
  }
  savedSubV.clear();


  subCacheState = ScsIdle;
  cacheSemGive(__FUNCTION__, "Synchronizing subscription cache");
}



/* ****************************************************************************
*
* subCacheRefresherThread -
*/
static void* subCacheRefresherThread(void* vP)
{
  extern int subCacheInterval;

  orionldStateInit(NULL);
  while (1)
  {
    sleep(subCacheInterval);

    subCacheSync();

    kaBufferReset(&orionldState.kalloc, true);  // Should be inside orionldStateRelease ... Right?
    orionldStateRelease();
  }

  return NULL;
}



/* ****************************************************************************
*
* subCacheStart -
*/
void subCacheStart(void)
{
  pthread_t  tid;
  int        ret;

  // Populate subscription cache from database
  subCacheRefresh(false);

  ret = pthread_create(&tid, NULL, subCacheRefresherThread, NULL);

  if (ret != 0)
  {
    LM_E(("Runtime Error (error creating thread: %d)", ret));
    return;
  }
  pthread_detach(tid);
}



extern bool noCache;
/* ****************************************************************************
*
* subCacheItemNotificationErrorStatus -
*
* This function marks a subscription to be erroneous, i.e. notifications are
* not working.
* A timestamp for this last failure is set for the sub-item in the sub-cache  and
* the consecutive number of notification errors for the subscription is incremented.
*
* If 'errors' == 0, then the subscription is marked as non-erroneous.
*
* This function is not used if "experimental" as it is only called from "old legacy code"
*/
void subCacheItemNotificationErrorStatus(const std::string& tenant, const std::string& subscriptionId, int errors, bool ngsild)
{
  struct timespec ts;
  kTimeGet(&ts);
  double kNow = ts.tv_sec + ts.tv_nsec / 1000000000.0;

  if (noCache)
  {
    // The field 'count' has already been taken care of. Set to 0 in the calls to mongoSubCountersUpdate()

    if (errors == 0)
      mongoSubCountersUpdate(tenant, subscriptionId, 0, kNow, -1, kNow, ngsild);  // lastFailure == -1
    else
      mongoSubCountersUpdate(tenant, subscriptionId, 0, kNow, kNow, -1, ngsild);  // lastSuccess == -1, count == 0

    return;
  }

  cacheSemTake(__FUNCTION__, "Looking up an item for lastSuccess/Failure");

  CachedSubscription* subP = subCacheItemLookup(tenant.c_str(), subscriptionId.c_str());

  if (subP == NULL)
  {
    cacheSemGive(__FUNCTION__, "Looking up an item for lastSuccess/Failure");
    const char* errorString = "intent to update error status of non-existing subscription";

    alarmMgr.badInput(clientIp, errorString);
    LM_W(("no sub found (subId: '%s') - counters/timestamps lost", subscriptionId.c_str()));
    return;
  }

  LM_T(LmtSubCacheStats, ("%s: Setting lastNotificationTime to %f (old value in cache: %f)", subP->subscriptionId, kNow));
  subP->lastNotificationTime = kNow;

  if (errors == 0)
  {
    subP->lastSuccess  = kNow;
    LM_T(LmtSubCacheStats, ("%s: Setting lastSuccess to %f (in cache)", subP->subscriptionId, subP->lastSuccess));
  }
  else
  {
    subP->lastFailure  = kNow;
    LM_T(LmtSubCacheStats, ("%s: Setting lastFailure to %f (in cache)", subP->subscriptionId, subP->lastFailure));
  }

  cacheSemGive(__FUNCTION__, "Looking up an item for lastSuccess/Failure");
}
