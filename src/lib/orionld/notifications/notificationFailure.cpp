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
#include <string.h>                                                 // strncpy

#include "logMsg/logMsg.h"                                          // LM_*

#include "cache/CachedSubscription.h"                               // CachedSubscription

#include "orionld/common/orionldState.h"                            // promNotifications, promNotificationsFailed
#include "orionld/prometheus/promCounterIncrease.h"                 // promCounterIncrease
#include "orionld/mongoc/mongocSubCountersUpdate.h"                 // mongocSubCountersUpdate
#include "orionld/notifications/notificationFailure.h"              // Own interface



// -----------------------------------------------------------------------------
//
// notificationFailure -
//
void notificationFailure(CachedSubscription* subP, const char* errorReason, double notificationTime)
{
  LM_T(LmtNotificationStats, ("%s: notification failure (timestamp: %f)", subP->subscriptionId, notificationTime));
  bool forcedToPause = false;

  subP->lastNotificationTime  = notificationTime;
  subP->lastFailure           = notificationTime;
  subP->consecutiveErrors    += 1;
  subP->count                += 1;
  subP->dirty                += 1;

  strncpy(subP->lastErrorReason, errorReason, sizeof(subP->lastErrorReason) - 1);

  // Force the subscription into "paused" due to too many consecutive errors
  if (subP->consecutiveErrors >= 3)
  {
    LM_T(LmtNotificationStats, ("%s: force the subscription into PAUSE due to 3 consecutive errors", subP->subscriptionId));
    subP->isActive = false;
    subP->status   = "paused";
    forcedToPause  = true;
  }

  promCounterIncrease(promNotifications);
  promCounterIncrease(promNotificationsFailed);

  LM_T(LmtNotificationStats, ("%s: dirty: %d, cSubCounters: %d", subP->subscriptionId, subP->dirty, cSubCounters));

  //
  // Flush to DB?
  // - If forcedToPause
  // - If subP->dirty (number of counter updates since last flush) >= cSubCounters
  //   - AND cSubCounters != 0
  //
  if (((cSubCounters != 0) && (subP->dirty >= cSubCounters)) || (forcedToPause == true))
  {
    LM_T(LmtNotificationStats, ("%s: Calling mongocSubCountersUpdate", subP->subscriptionId));
    mongocSubCountersUpdate(subP->tenant, subP, subP->count, subP->lastNotificationTime, subP->lastFailure, subP->lastSuccess, forcedToPause, true);
    subP->dirty    = 0;
    subP->dbCount += subP->count;
    subP->count    = 0;
  }
  else
    LM_T(LmtNotificationStats, ("%s: Not calling mongocSubCountersUpdate (cSubCounters: %d, dirty: %d, forcedToPause: %s)",
                                subP->subscriptionId,
                                cSubCounters,
                                subP->dirty,
                                (forcedToPause == true)? "true" : "false"));
}
