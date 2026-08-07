/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2024 PointerThere — GPLv3
 */

#pragma once
#include <drogon/HttpController.h>

namespace pt::controllers {

class ListController : public drogon::HttpController<ListController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ListController::getList,      "/api/list",                  drogon::Get);
    ADD_METHOD_TO(ListController::getFeatured,  "/api/list/featured",         drogon::Get);
    ADD_METHOD_TO(ListController::getMovements, "/api/list/movements",        drogon::Get);
    ADD_METHOD_TO(ListController::getLevel,     "/api/list/{id}",             drogon::Get);
    ADD_METHOD_TO(ListController::getLevelRecs, "/api/list/{id}/records",     drogon::Get);
    METHOD_LIST_END

    void getList     (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&, int id = 0);
    void getFeatured (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void getMovements(const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void getLevel    (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&, long long id);
    void getLevelRecs(const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&, long long id);
};

}
