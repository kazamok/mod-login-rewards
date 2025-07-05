#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "Config.h"
#include "World.h"
#include "WorldSessionMgr.h"
#include "GameTime.h"
#include "SharedDefines.h"
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <ctime>

// 전역 변수 선언
// 모듈 설정 값
bool g_loginRewardsEnabled = false;
uint32 g_loginRewardsDailyGoldAmount = 0;
uint32 g_loginRewardsDailyResetHourUTC = 0;
std::string g_loginRewardsAnnounceMessage = "";
bool g_loginRewardsShowAnnounceMessage = false;

// 플레이어별 마지막 보상 지급 시간 저장 (GUID -> Timestamp)
std::unordered_map<ObjectGuid::LowType, time_t> g_playerLastRewardTime;

// 데이터 파일 경로
const std::string PLAYER_LAST_REWARD_FILE = "logs/login_rewards/player_last_reward.csv";

// 로그 디렉토리가 존재하는지 확인하고, 없으면 생성하는 함수
void EnsureLoginRewardsLogDirectory()
{
    std::filesystem::path logDir = "logs/login_rewards";
    if (!std::filesystem::exists(logDir))
    {
        std::filesystem::create_directories(logDir);
        LOG_INFO("module", "[접속 보상] 로그 디렉토리 생성: {}", logDir.string());
    }
}

// 플레이어 마지막 보상 시간 데이터를 파일에서 로드하는 함수
void LoadPlayerLastRewardData()
{
    EnsureLoginRewardsLogDirectory();
    std::ifstream infile(PLAYER_LAST_REWARD_FILE);
    if (!infile.is_open())
    {
        LOG_INFO("module", "[접속 보상] 플레이어 마지막 보상 데이터 파일을 찾을 수 없습니다. 새로 생성합니다.");
        return;
    }

    std::string line;
    // 헤더 스킵
    std::getline(infile, line);

    while (std::getline(infile, line))
    {
        std::stringstream ss(line);
        std::string guidStr, timestampStr;
        if (std::getline(ss, guidStr, ',') && std::getline(ss, timestampStr, ','))
        {
            try
            {
                ObjectGuid::LowType guid = std::stoul(guidStr);
                time_t timestamp = static_cast<time_t>(std::stoll(timestampStr));
                g_playerLastRewardTime[guid] = timestamp;
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("module", "[접속 보상] 플레이어 마지막 보상 데이터 파싱 오류: {} ({})", line, e.what());
            }
        }
    }
    infile.close();
    LOG_INFO("module", "[접속 보상] 플레이어 마지막 보상 데이터 로드 완료. {}개 항목.", g_playerLastRewardTime.size());
}

// 플레이어 마지막 보상 시간 데이터를 파일에 저장하는 함수
void SavePlayerLastRewardData()
{
    EnsureLoginRewardsLogDirectory();
    std::ofstream outfile(PLAYER_LAST_REWARD_FILE);
    if (!outfile.is_open())
    {
        LOG_ERROR("module", "[접속 보상] 플레이어 마지막 보상 데이터 파일을 열 수 없습니다: {}", PLAYER_LAST_REWARD_FILE);
        return;
    }

    outfile << "PlayerGUID,LastRewardTimestamp\n"; // 헤더
    for (const auto& pair : g_playerLastRewardTime)
    {
        outfile << pair.first << "," << pair.second << "\n";
    }
    outfile.close();
    LOG_INFO("module", "[접속 보상] 플레이어 마지막 보상 데이터 저장 완료. {}개 항목.", g_playerLastRewardTime.size());
}

// 모듈 전용 설정 파일을 로드하고 파싱하는 함수
void LoadModuleSpecificConfig_LoginRewards()
{
    std::string configFilePath = "./configs/modules/mod-login-rewards.conf.dist";

    std::ifstream configFile;

    if (std::filesystem::exists(configFilePath))
    {
        configFile.open(configFilePath);
        LOG_INFO("module", "[접속 보상] 설정 파일 로드: {}", configFilePath);
    }
    else
    {
        LOG_ERROR("module", "[접속 보상] 설정 파일을 찾을 수 없습니다. 모듈이 비활성화됩니다.");
        g_loginRewardsEnabled = false;
        return;
    }

    if (!configFile.is_open())
    {
        LOG_ERROR("module", "[접속 보상] 설정 파일을 열 수 없습니다. 모듈이 비활성화됩니다.");
        g_loginRewardsEnabled = false;
        return;
    }

    // 기본값 설정
    g_loginRewardsEnabled = true;
    g_loginRewardsDailyGoldAmount = 100000; // 1골드
    g_loginRewardsDailyResetHourUTC = 0;
    g_loginRewardsAnnounceMessage = "일일 접속 보상으로 %gold%골드를 받았습니다!";
    g_loginRewardsShowAnnounceMessage = true;

    std::string line;
    while (std::getline(configFile, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string key;
        if (std::getline(iss, key, '='))
        {
            std::string value;
            if (std::getline(iss, value))
            {
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                if (key == "LoginRewards.Enable")
                {
                    g_loginRewardsEnabled = (value == "1");
                }
                else if (key == "LoginRewards.DailyGoldAmount")
                {
                    try { g_loginRewardsDailyGoldAmount = std::stoul(value); }
                    catch (const std::exception& e) { LOG_ERROR("module", "[접속 보상] 잘못된 골드 양 설정: {} ({})", value, e.what()); }
                }
                else if (key == "LoginRewards.DailyResetHourUTC")
                {
                    try { g_loginRewardsDailyResetHourUTC = std::stoul(value); }
                    catch (const std::exception& e) { LOG_ERROR("module", "[접속 보상] 잘못된 리셋 시간 설정: {} ({})", value, e.what()); }
                }
                else if (key == "LoginRewards.AnnounceMessage")
                {
                    if (value.length() >= 2 && value.front() == '"' && value.back() == '"')
                    {
                        g_loginRewardsAnnounceMessage = value.substr(1, value.length() - 2);
                    }
                    else
                    {
                        g_loginRewardsAnnounceMessage = value;
                    }
                }
                else if (key == "LoginRewards.ShowAnnounceMessage")
                {
                    g_loginRewardsShowAnnounceMessage = (value == "1");
                }
            }
        }
    }
    configFile.close();
}

// 월드 서버 이벤트를 처리하는 클래스
class mod_login_rewards_world : public WorldScript
{
public:
    mod_login_rewards_world() : WorldScript("mod_login_rewards_world") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        // 모듈 전용 설정 파일을 로드하고 파싱합니다.
        LoadModuleSpecificConfig_LoginRewards();
    }

    void OnStartup() override
    {
        if (g_loginRewardsEnabled)
        {
            LOG_INFO("module", "[접속 보상] 모듈 활성화됨.");
            LoadPlayerLastRewardData(); // 서버 시작 시 플레이어 데이터 로드
        }
    }

    void OnShutdown() override
    {
        if (g_loginRewardsEnabled)
        {
            LOG_INFO("module", "[접속 보상] 모듈 비활성화됨. 데이터 저장 중...");
            SavePlayerLastRewardData(); // 서버 종료 시 플레이어 데이터 저장
        }
    }
};

// 플레이어 이벤트를 처리하는 클래스
class mod_login_rewards_player : public PlayerScript
{
public:
    mod_login_rewards_player() : PlayerScript("mod_login_rewards_player") { }

    void OnPlayerLogin(Player* player) override
    {
        if (!g_loginRewardsEnabled)
            return;

        ObjectGuid::LowType playerGuidLow = player->GetGUID().GetCounter();
        time_t lastRewardTime = 0;

        // 플레이어의 마지막 보상 시간 가져오기
        auto it = g_playerLastRewardTime.find(playerGuidLow);
        if (it != g_playerLastRewardTime.end())
        {
            lastRewardTime = it->second;
        }

        // 현재 시간 (UTC)
        time_t currentTime = GameTime::GetGameTime().count();
        struct tm* currentTm = std::gmtime(&currentTime);

        // 마지막 보상 시간 (UTC)
        struct tm* lastRewardTm = std::gmtime(&lastRewardTime);

        bool canReceiveReward = false;

        if (lastRewardTime == 0) // 첫 접속이거나 기록이 없는 경우
        {
            canReceiveReward = true;
        }
        else
        {
            // 마지막 보상 날짜와 현재 날짜가 다른지 확인 (UTC 기준)
            if (currentTm->tm_year > lastRewardTm->tm_year ||
                (currentTm->tm_year == lastRewardTm->tm_year && currentTm->tm_mon > lastRewardTm->tm_mon) ||
                (currentTm->tm_year == lastRewardTm->tm_year && currentTm->tm_mon == lastRewardTm->tm_mon && currentTm->tm_mday > lastRewardTm->tm_mday))
            {
                // 날짜가 바뀌었으면 보상 가능
                canReceiveReward = true;
            }
            else if (currentTm->tm_year == lastRewardTm->tm_year && currentTm->tm_mon == lastRewardTm->tm_mon && currentTm->tm_mday == lastRewardTm->tm_mday)
            {
                // 같은 날짜인 경우, 리셋 시간을 기준으로 보상 가능 여부 판단
                if (currentTm->tm_hour >= g_loginRewardsDailyResetHourUTC && lastRewardTm->tm_hour < g_loginRewardsDailyResetHourUTC)
                {
                    canReceiveReward = true;
                }
            }
        }

        if (canReceiveReward)
        {
            // 골드 지급
            player->ModifyMoney(g_loginRewardsDailyGoldAmount);
            LOG_INFO("module", "[접속 보상] 플레이어 {} (GUID: {})에게 {} 골드 지급.", player->GetName(), playerGuidLow, g_loginRewardsDailyGoldAmount);

            // 마지막 보상 시간 업데이트
            g_playerLastRewardTime[playerGuidLow] = currentTime;

            // 메시지 전송
            if (g_loginRewardsShowAnnounceMessage)
            {
                std::string message = g_loginRewardsAnnounceMessage;
                // %gold% 플레이스홀더를 실제 골드 양으로 대체
                size_t pos = message.find("%gold%");
                if (pos != std::string::npos)
                {
                    std::stringstream gold_ss;
                    gold_ss << (g_loginRewardsDailyGoldAmount / 10000); // 구리 -> 골드 변환
                    message.replace(pos, 6, gold_ss.str());
                }
                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
            }
        }
    }
};

// 모든 스크립트를 추가하는 함수 (모듈 로드 시 호출됨)
void Addmod_login_rewardsScripts()
{
    new mod_login_rewards_world();
    new mod_login_rewards_player();
}
