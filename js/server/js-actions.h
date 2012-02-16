static string JS_server_actions = 
  "////////////////////////////////////////////////////////////////////////////////\n"
  "/// @brief JavaScript actions functions\n"
  "///\n"
  "/// @file\n"
  "///\n"
  "/// DISCLAIMER\n"
  "///\n"
  "/// Copyright 2010-2011 triagens GmbH, Cologne, Germany\n"
  "///\n"
  "/// Licensed under the Apache License, Version 2.0 (the \"License\");\n"
  "/// you may not use this file except in compliance with the License.\n"
  "/// You may obtain a copy of the License at\n"
  "///\n"
  "///     http://www.apache.org/licenses/LICENSE-2.0\n"
  "///\n"
  "/// Unless required by applicable law or agreed to in writing, software\n"
  "/// distributed under the License is distributed on an \"AS IS\" BASIS,\n"
  "/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
  "/// See the License for the specific language governing permissions and\n"
  "/// limitations under the License.\n"
  "///\n"
  "/// Copyright holder is triAGENS GmbH, Cologne, Germany\n"
  "///\n"
  "/// @author Dr. Frank Celler\n"
  "/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany\n"
  "////////////////////////////////////////////////////////////////////////////////\n"
  "\n"
  "var time = SYS_TIME;\n"
  "\n"
  "// -----------------------------------------------------------------------------\n"
  "// --SECTION--                                                  public functions\n"
  "// -----------------------------------------------------------------------------\n"
  "\n"
  "////////////////////////////////////////////////////////////////////////////////\n"
  "/// @addtogroup V8Json V8 JSON\n"
  "/// @{\n"
  "////////////////////////////////////////////////////////////////////////////////\n"
  "\n"
  "////////////////////////////////////////////////////////////////////////////////\n"
  "/// @brief returns a result of a query as documents\n"
  "///\n"
  "/// @FUN{queryResult(@FA{req}, @FA{res}, @FA{query})}\n"
  "///\n"
  "/// The functions returns the result of a query using pagination. It assumes\n"
  "/// that the request has the numerical parameters @LIT{blocksize} and @LIT{page}\n"
  "/// or @LIT{offset}. @LIT{blocksize} determines the maximal number of result\n"
  "/// documents to return. You can either specify @LIT{page} or @LIT{offset}.\n"
  "/// @LIT{page} is the number of pages to skip, i. e. @LIT{page} *\n"
  "/// @LIT{blocksize} documents. @LIT{offset} is the number of documents to skip.\n"
  "///\n"
  "/// If you are using pagination, than the query must be a sorted query.\n"
  "////////////////////////////////////////////////////////////////////////////////\n"
  "\n"
  "function queryResult (req, res, query) {\n"
  "  var result;\n"
  "  var offset = 0;\n"
  "  var page = 0;\n"
  "  var blocksize = 0;\n"
  "  var t;\n"
  "  var result;\n"
  "\n"
  "  t = time();\n"
  "\n"
  "  if (req.blocksize) {\n"
  "    blocksize = req.blocksize;\n"
  "\n"
  "    if (req.page) {\n"
  "      page = req.page;\n"
  "      offset = page * blocksize;\n"
  "      query = query.skip(offset).limit(blocksize);\n"
  "    }\n"
  "    else {\n"
  "      query = query.limit(blocksize);\n"
  "    }\n"
  "  }\n"
  "\n"
  "  result = query.toArray();\n"
  "\n"
  "  result = {\n"
  "    total : query.count(),\n"
  "    count : query.count(true),\n"
  "    offset : offset,\n"
  "    blocksize : blocksize,\n"
  "    page : page,\n"
  "    runtime : time() - t,\n"
  "    documents : result\n"
  "  };\n"
  "\n"
  "  res.responseCode = 200;\n"
  "  res.contentType = \"application/json\";\n"
  "  res.body = JSON.stringify(result);\n"
  "}\n"
  "\n"
  "////////////////////////////////////////////////////////////////////////////////\n"
  "/// @brief returns a result of a query as references\n"
  "///\n"
  "/// @FUN{queryReferences(@FA{req}, @FA{res}, @FA{query})}\n"
  "///\n"
  "/// The methods works like @FN{queryResult} but instead of the documents only\n"
  "/// document identifiers are returned.\n"
  "////////////////////////////////////////////////////////////////////////////////\n"
  "\n"
  "function queryReferences (req, res, query) {\n"
  "  var result;\n"
  "  var offset = 0;\n"
  "  var page = 0;\n"
  "  var blocksize = 0;\n"
  "  var t;\n"
  "  var result;\n"
  "\n"
  "  t = time();\n"
  "\n"
  "  if (req.blocksize) {\n"
  "    blocksize = req.blocksize;\n"
  "\n"
  "    if (req.page) {\n"
  "      page = req.page;\n"
  "      offset = page * blocksize;\n"
  "      query = query.skip(offset).limit(blocksize);\n"
  "    }\n"
  "    else {\n"
  "      query = query.limit(blocksize);\n"
  "    }\n"
  "  }\n"
  "\n"
  "  result = [];\n"
  "\n"
  "  while (query.hasNext()) {\n"
  "    result.push(query.nextRef());\n"
  "  }\n"
  "\n"
  "  result = {\n"
  "    total : query.count(),\n"
  "    count : query.count(true),\n"
  "    offset : offset,\n"
  "    blocksize : blocksize,\n"
  "    page : page,\n"
  "    runtime : time() - t,\n"
  "    references : result\n"
  "  };\n"
  "\n"
  "  res.responseCode = 200;\n"
  "  res.contentType = \"application/json\";\n"
  "  res.body = JSON.stringify(result);\n"
  "}\n"
  "\n"
  "////////////////////////////////////////////////////////////////////////////////\n"
  "/// @brief returns a result\n"
  "///\n"
  "/// @FUN{actionResult(@FA{req}, @FA{res}, @FA{code}, @FA{result})}\n"
  "///\n"
  "/// The functions returns a result object. @FA{code} is the status code\n"
  "/// to return.\n"
  "////////////////////////////////////////////////////////////////////////////////\n"
  "\n"
  "function actionResult (req, res, code, result) {\n"
  "  if (code == 204) {\n"
  "    res.responseCode = code;\n"
  "  }\n"
  "  else {\n"
  "    res.responseCode = code;\n"
  "    res.contentType = \"application/json\";\n"
  "    res.body = JSON.stringify(result);\n"
  "  }\n"
  "}\n"
  "\n"
  "////////////////////////////////////////////////////////////////////////////////\n"
  "/// @brief returns an error\n"
  "///\n"
  "/// @FUN{actionError(@FA{req}, @FA{res}, @FA{errorMessage})}\n"
  "///\n"
  "/// The functions returns an error message. The status code is 500 and the\n"
  "/// returned object is an array with an attribute @LIT{error} containing\n"
  "/// the error message @FA{errorMessage}.\n"
  "////////////////////////////////////////////////////////////////////////////////\n"
  "\n"
  "function actionError (req, res, err) {\n"
  "  res.responseCode = 500;\n"
  "  res.contentType = \"application/json\";\n"
  "  res.body = JSON.stringify({ 'error' : \"\" + err });\n"
  "}\n"
  "\n"
  "////////////////////////////////////////////////////////////////////////////////\n"
  "/// @}\n"
  "////////////////////////////////////////////////////////////////////////////////\n"
  "\n"
  "// Local Variables:\n"
  "// mode: outline-minor\n"
  "// outline-regexp: \"^\\\\(/// @brief\\\\|/// @addtogroup\\\\|// --SECTION--\\\\|/// @page\\\\|/// @}\\\\)\"\n"
  "// End:\n"
;