#pragma once
#include "stdafx.h"
#include "cpr/cpr.h"
#include "json.hpp"
#include <Beatmap/MapDatabase.hpp>
#include <Beatmap/BeatmapObjects.hpp>

namespace IR
{
    //honestly, this is really disgusting and i'd much rather have it as an enum, but this is just so much nicer to work with...
    inline nlohmann::json const ResponseState = {
        {"Unused", 0},
        {"Pending", 10},
        {"Success", 20},
        {"BadRequest", 40},
        {"Unauthorized", 41},
        {"ChartRefused", 42},
        {"Forbidden", 43},
        {"NotFound", 44},
        {"ServerError", 50},
        {"RequestFailure", 60}};

    cpr::AsyncResponse PostScore(const ScoreIndex& score, const BeatmapSettings& map);
    cpr::AsyncResponse Heartbeat();
    cpr::AsyncResponse ChartTracked(String chartHash);
    cpr::AsyncResponse Record(String chartHash);
    cpr::AsyncResponse Leaderboard(String chartHash, String mode, int n);
    cpr::AsyncResponse PostReplay(String identifier, String replayPath);

    bool ValidateReturn(const nlohmann::json& json);
    bool ValidatePostScoreReturn(const nlohmann::json& json);
}