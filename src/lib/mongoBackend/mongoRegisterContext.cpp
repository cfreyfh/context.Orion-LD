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
* Author: Fermin Galan Marquez
*/
#include <string>
#include <map>

#include "mongo/client/dbclient.h"

#include "logMsg/logMsg.h"
#include "logMsg/traceLevels.h"

#include "orionld/common/orionldState.h"             // orionldState
#include "orionld/types/OrionldTenant.h"             // OrionldTenant

#include "common/globals.h"
#include "common/statistics.h"
#include "common/sem.h"
#include "common/defaultValues.h"
#include "alarmMgr/alarmMgr.h"
#include "ngsi/StatusCode.h"
#include "ngsi9/RegisterContextRequest.h"
#include "ngsi9/RegisterContextResponse.h"

#include "mongoBackend/MongoGlobal.h"
#include "mongoBackend/connectionOperations.h"
#include "mongoBackend/MongoCommonRegister.h"
#include "mongoBackend/dbConstants.h"
#include "mongoBackend/safeMongo.h"
#include "mongoBackend/mongoRegisterContext.h"



/* ****************************************************************************
*
* USING
*/
using mongo::BSONObj;
using mongo::OID;



/* ****************************************************************************
*
* mongoRegisterContext - 
*/
HttpStatusCode mongoRegisterContext
(
  RegisterContextRequest*              requestP,
  RegisterContextResponse*             responseP,
  const std::string&                   fiwareCorrelator,
  OrionldTenant*                       tenantP,
  const std::string&                   servicePath
)
{
  bool         reqSemTaken;

  if (tenantP == NULL)
    tenantP = orionldState.tenantP;

  // FIXME P4: See issue #3078
  std::string  sPath = (servicePath == "")? SERVICE_PATH_ROOT : servicePath;

  reqSemTake(__FUNCTION__, "ngsi9 register request", SemWriteOp, &reqSemTaken);

  /* Check if new registration */
  if (requestP->registrationId.isEmpty())
  {
    HttpStatusCode result = processRegisterContext(requestP, responseP, NULL, tenantP, sPath.c_str(), "JSON", fiwareCorrelator);

    reqSemGive(__FUNCTION__, "ngsi9 register request", reqSemTaken);
    return result;
  }

  /* It is not a new registration, so it must be an update */
  BSONObj      reg;
  std::string  err;
  OID          id;

  if (!safeGetRegId(requestP->registrationId.c_str(), &id, &(responseP->errorCode)))
  {
    reqSemGive(__FUNCTION__, "ngsi9 register request (safeGetRegId fail)", reqSemTaken);
    responseP->registrationId = requestP->registrationId;
    ++noOfRegistrationUpdateErrors;

    if (responseP->errorCode.code == SccContextElementNotFound)
    {
      // FIXME: doubt: invalid OID format?
      std::string details = std::string("invalid OID format: '") + requestP->registrationId.get() + "'";
      alarmMgr.badInput(clientIp, details);
    }
    else  // SccReceiverInternalError
    {
      LM_E(("Runtime Error (exception getting OID: %s)", responseP->errorCode.details.c_str()));
    }

    return SccOk;
  }

  const mongo::BSONObj bson = BSON("_id" << id << REG_SERVICE_PATH << sPath);

  if (!collectionFindOne(tenantP->registrations, bson, &reg, &err))
  {
    reqSemGive(__FUNCTION__, "ngsi9 register request", reqSemTaken);
    responseP->errorCode.fill(SccReceiverInternalError, err);
    ++noOfRegistrationUpdateErrors;

    return SccOk;
  }

  if (reg.isEmpty())
  {
    reqSemGive(__FUNCTION__, "ngsi9 register request (no registrations found)", reqSemTaken);
    responseP->errorCode.fill(SccContextElementNotFound,
                              std::string("registration id: /") + requestP->registrationId.get() + "/");
    responseP->registrationId = requestP->registrationId;
    ++noOfRegistrationUpdateErrors;

    return SccOk;
  }

  HttpStatusCode result = processRegisterContext(requestP, responseP, &id, tenantP, sPath.c_str(), "JSON", fiwareCorrelator);
  reqSemGive(__FUNCTION__, "ngsi9 register request", reqSemTaken);
  return result;
}
