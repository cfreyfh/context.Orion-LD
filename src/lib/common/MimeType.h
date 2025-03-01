#ifndef SRC_LIB_COMMON_MIMETYPE_H_
#define SRC_LIB_COMMON_MIMETYPE_H_

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
#include <string>



/* ****************************************************************************
*
* DEFAULT_MIMETYPE - 
*/
#define DEFAULT_MIMETYPE              JSON
#define DEFAULT_MIMETYPE_AS_STRING    "JSON"



/* ****************************************************************************
*
* MimeType - 
*/
typedef enum MimeType
{
  NOMIMETYPEGIVEN  = 0,
  NOMIMETYPE       = 1,
  JSON             = 2,
  TEXT             = 3,
  HTML             = 4,
  JSONLD           = 5,
  GEOJSON          = 6,
  MERGEPATCHJSON   = 7
} MimeType;



/* ****************************************************************************
*
* mimeTypeToLongString - 
*/
extern const char* mimeTypeToLongString(MimeType mimeType);



/* ****************************************************************************
*
* mimeTypeToString - 
*/
extern const char* mimeTypeToString(MimeType mimeType);



/* ****************************************************************************
*
* stringToMimeType
*/
extern MimeType stringToMimeType(const std::string& s);
extern MimeType longStringToMimeType(const char* s);

#endif  // SRC_LIB_COMMON_MIMETYPE_H_
