#include "stdafx.h"
#include "IR.hpp"
#include "GameConfig.hpp"

static void PopulateScoreJSON(nlohmann::json& json, const ScoreIndex& score, const BeatmapSettings& map)
{
    json["score"] = {
        {"score", score.score},
        {"crit", score.crit},
        {"near", score.almost},
        {"early", score.early},
        {"late", score.late},
        {"combo", score.combo},
        {"error", score.miss},
        {"gauge", score.gauge},
        {"options", {
            {"gaugeType", score.gaugeType},
            {"gaugeOpt", score.gaugeOption},
            {"mirror", score.mirror},
            {"random", score.random},
            {"autoFlags", score.autoFlags}
        }},
        {"timestamp", score.timestamp},
        {"windows", {
            {"perfect", score.hitWindowPerfect},
            {"good", score.hitWindowGood},
            {"hold", score.hitWindowHold},
            {"miss", score.hitWindowMiss},
            {"slam", score.hitWindowSlam}
        }}
    };

    json["chart"] = {
        {"chartHash", score.chartHash}, //this looks nasty in code but makes sense from a protocol perspective
        {"title", map.title},
        {"artist", map.artist},
        {"effector", map.effector},
        {"illustrator", map.illustrator},
        {"difficulty", map.difficulty},
        {"level", map.level},
        {"bpm", map.bpm}
    };
}

static cpr::Header CommonHeader()
{
    return cpr::Header{
        {"Authorization", "Bearer " + g_gameConfig.GetString(GameConfigKeys::IRToken)}, //would like to use cpr::Bearer but was added in a more recent version of cpr
        {"Content-Type", "application/json"}
    };
}

namespace IR {
    cpr::AsyncResponse PostScore(const ScoreIndex& score, const BeatmapSettings& map)
    {
        String host = g_gameConfig.GetString(GameConfigKeys::IRBaseURL) + "/scores";

        nlohmann::json json;

        PopulateScoreJSON(json, score, map);

        return cpr::PostAsync(cpr::Url{host},
                              CommonHeader(),
                              cpr::Body{json.dump()});
    }

    cpr::AsyncResponse Heartbeat()
    {
        String host = g_gameConfig.GetString(GameConfigKeys::IRBaseURL);

        return cpr::GetAsync(cpr::Url{host},
                             CommonHeader());
    }

    cpr::AsyncResponse ChartTracked(String chartHash)
    {
        String host = g_gameConfig.GetString(GameConfigKeys::IRBaseURL) + "/charts/" + chartHash;

        return cpr::GetAsync(cpr::Url{host},
                             CommonHeader());
    }

    cpr::AsyncResponse Record(String chartHash)
    {
        String host = g_gameConfig.GetString(GameConfigKeys::IRBaseURL) + "/charts/" + chartHash + "/record";

        return cpr::GetAsync(cpr::Url{host},
                             CommonHeader());
    }

    cpr::AsyncResponse Leaderboard(String chartHash, String mode, int n)
    {
        String host = g_gameConfig.GetString(GameConfigKeys::IRBaseURL) + "/charts/" + chartHash + "/leaderboard";

        return cpr::GetAsync(cpr::Url{host},
                             cpr::Header{{"Authorization", "Bearer " + g_gameConfig.GetString(GameConfigKeys::IRToken)}}, //can't give the json header here so whatever
                             cpr::Parameters{{"mode", mode},
                                             {"n", std::to_string(n)}});
    }

    cpr::AsyncResponse PostReplay(String identifier, String replayPath)
    {
        String host = g_gameConfig.GetString(GameConfigKeys::IRBaseURL) + "/replays";

        return cpr::PostAsync(cpr::Url{host},
                              cpr::Header{{"Authorization", "Bearer " + g_gameConfig.GetString(GameConfigKeys::IRToken)}}, //can't give the json header here so whatever
                              cpr::Multipart{{"identifier", identifier},
                                             {"replay", cpr::File{replayPath}}});
    }

    bool ValidateReturn(const nlohmann::json& json)
    {
        if(json.find("statusCode") == json.end()) return false;
        if(json["statusCode"] < 20 || json["statusCode"] > 59) return false;

        if(json.find("description") == json.end()) return false;

        if(json["statusCode"] == 20)
        {
            if(json.find("body") == json.end()) return false;
        }

        return true;
    }

    //note: this only confirms that the response is well-formed - responses other than 20 will not have most information, so this needs to be beared in mind too
    //e.g., this function returning true does not mean that body will exist, because status may not be 20.
    //it also does not validate each score's structure at this time - possible todo?
    bool ValidatePostScoreReturn(const nlohmann::json& json)
    {
        if(!ValidateReturn(json)) return false;

        if(json["statusCode"] == 20)
        {
            if(json.find("body") == json.end()) return false;

            if(json["body"].find("adjacentAbove") == json["body"].end()) return false;
            if(!json["body"]["adjacentAbove"].is_array()) return false;

            if(json["body"].find("adjacentBelow") == json["body"].end()) return false;
            if(!json["body"]["adjacentAbove"].is_array()) return false;

            if(json["body"].find("adjacentBelow") == json["body"].end()) return false;
            if(!json["body"]["adjacentAbove"].is_array()) return false;

            if(json["body"].find("isPB") == json["body"].end()) return false;
            if(!json["body"]["isPB"].is_boolean()) return false;

            if(json["body"].find("isServerRecord") == json["body"].end()) return false;
            if(!json["body"]["isServerRecord"].is_boolean()) return false;

            if(json["body"].find("score") == json["body"].end()) return false;
            if(!json["body"]["score"].is_object()) return false;

            if(json["body"].find("serverRecord") == json["body"].end()) return false;
            if(!json["body"]["serverRecord"].is_object()) return false;
        }

        return true;

    }

}
