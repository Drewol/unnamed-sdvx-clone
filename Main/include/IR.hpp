#pragma once
#include <Beatmap/MapDatabase.hpp>
#include <Beatmap/BeatmapObjects.hpp>

#ifdef Success
#undef Success
#endif

#ifdef BadRequest
#undef BadRequest
#endif

namespace IR
{
    struct ResponseState final
    {
        const static int Unused = 0;
        const static int Pending = 10;
        const static int Success = 20;
        const static int Accepted = 22;
        const static int BadRequest = 40;
        const static int Unauthorized = 41;
        const static int ChartRefused = 42;
        const static int Forbidden = 43;
        const static int NotFound = 44;
        const static int ServerError = 50;
        const static int RequestFailure = 60;

        inline const static std::initializer_list<std::pair<const char*, int>> Values = {
            {"Unused", IR::ResponseState::Unused},
            {"Pending", IR::ResponseState::Pending},
            {"Success", IR::ResponseState::Success},
            {"Accepted", IR::ResponseState::Accepted},
            {"BadRequest", IR::ResponseState::BadRequest},
            {"Unauthorized", IR::ResponseState::Unauthorized},
            {"ChartRefused", IR::ResponseState::ChartRefused},
            {"Forbidden", IR::ResponseState::Forbidden},
            {"NotFound", IR::ResponseState::NotFound},
            {"ServerError", IR::ResponseState::ServerError},
            {"RequestFailure", IR::ResponseState::RequestFailure},
        };

    private:
        ResponseState() = delete;
        ~ResponseState() = delete;
    };

    cpr::AsyncResponse PostScore(const ScoreIndex& score, const BeatmapSettings& map);
    cpr::AsyncResponse Heartbeat();
    cpr::AsyncResponse ChartTracked(String chartHash);
    cpr::AsyncResponse Record(String chartHash);
    cpr::AsyncResponse Leaderboard(String chartHash, String mode, int n);
    void PostReplay(String identifier, String replayPath);

    bool ValidateReturn(const nlohmann::json& json);
    bool ValidatePostScoreReturn(const nlohmann::json& json);
}
