#pragma once

#include <vector>
#include "document.h"
#include "search_server.h"
#include "request_queue.h"
#include <string>
#include <execution>
#include <algorithm>
#include <list>

std::vector<std::vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const std::vector<std::string>& queries);

std::list<Document> ProcessQueriesJoined(
        const SearchServer& search_server,
        const std::vector<std::string>& queries);