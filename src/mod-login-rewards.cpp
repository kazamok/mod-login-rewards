
/*
mod-login-rewards.cpp */

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
bool g_loginRewardsShowModuleStatus = false;
uint32 g_loginRewardsDailyGoldAmount = 0;
uint32 g_loginRewardsDailyResetHourKST = 0;
uint32 g_loginRewardsRewardDelaySeconds = 0;
std::string g_loginRewardsAnnounceMessage = "";
bool g_loginRewardsShowAnnounceMessage = false;

// 계정별 마지막 보상 지급 정보 (시간 및 캐릭터 이름)
struct RewardInfo
{
    time_t timestamp;
    std::string characterName;
};
std::unordered_map<uint32, RewardInfo> g_accountLastRewardInfo;

// 플레이어 로그인 시간 추적 (GUID, 로그인 시간)
std::unordered_map<uint32, time_t> g_playerLoginTimes;
std::mutex g_playerLoginTimesMutex; // g_playerLoginTimes 접근 제어를 위한 뮤텍스

// 데이터 파일 경로
const std::string ACCOUNT_LAST_REWARD_FILE = "logs/login_rewards/account_last_reward.csv";

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

// 타임스탬프를 YYYY-MM-DD HH:MM:SS 형식의 문자열로 변환하는 함수
std::string Time_tToString(time_t time)
{
    char buffer[20];
    struct tm* tm_info = std::localtime(&time);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buffer);
}

// YYYY-MM-DD 형식의 날짜 문자열을 반환하는 함수
std::string GetCurrentDateString()
{
    char buffer[11];
    time_t now = time(0);
    struct tm* tm_info = std::localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", tm_info);
    return std::string(buffer);
}

// YYYY-MM-DD HH:MM:SS 형식의 문자열을 time_t로 변환하는 함수
time_t StringToTime_t(const std::string& time_str)
{
    struct tm tm_info = {};
    std::stringstream ss(time_str);
    ss >> std::get_time(&tm_info, "%Y-%m-%d %H:%M:%S");
    return std::mktime(&tm_info);
}

// 계정 마지막 보상 시간 데이터를 파일에서 로드하는 함수 (상태 파일)
void LoadAccountLastRewardData()
{
    EnsureLoginRewardsLogDirectory();
    std::ifstream infile(ACCOUNT_LAST_REWARD_FILE);
    if (!infile.is_open())
    {
        LOG_INFO("module", "[접속 보상] 계정 마지막 보상 데이터 파일을 찾을 수 없습니다. 새로 생성합니다.");
        return;
    }

    std::string line;
    // 헤더 스킵
    std::getline(infile, line);

    while (std::getline(infile, line))
    {
        std::stringstream ss(line);
        std::string accountIdStr, charName, timestampStr;
        if (std::getline(ss, accountIdStr, ',') && std::getline(ss, charName, ',') && std::getline(ss, timestampStr))
        {
            try
            {
                uint32 accountId = std::stoul(accountIdStr);
                time_t timestamp = StringToTime_t(timestampStr);
                g_accountLastRewardInfo[accountId] = { timestamp, charName };
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("module", "[접속 보상] 계정 마지막 보상 데이터 파싱 오류: {} ({})", line, e.what());
            }
        }
    }
    infile.close();
    LOG_INFO("module", "[접속 보상] 계정 마지막 보상 데이터 로드 완료. {}개 항목.", g_accountLastRewardInfo.size());
}

// 플레이어 마지막 보상 시간 데이터를 파일에 저장하는 함수 (상태 파일)
void SaveAccountLastRewardData()
{
    EnsureLoginRewardsLogDirectory();
    std::ofstream outfile(ACCOUNT_LAST_REWARD_FILE);
    if (!outfile.is_open())
    {
        LOG_ERROR("module", "[접속 보상] 계정 마지막 보상 데이터 파일을 열 수 없습니다: {}", ACCOUNT_LAST_REWARD_FILE);
        return;
    }

    outfile << "AccountID,CharacterName,LastRewardTimestamp" << std::endl; // 헤더
    for (const auto& pair : g_accountLastRewardInfo)
    {
        outfile << pair.first << "," << pair.second.characterName << "," << Time_tToString(pair.second.timestamp) << std::endl;
    }
    outfile.close();
    LOG_INFO("module", "[접속 보상] 계정 마지막 보상 데이터 저장 완료. {}개 항목.", g_accountLastRewardInfo.size());
}

// 일일 보상 로그를 파일에 기록하는 함수
void LogRewardToFile(uint32 accountId, const std::string& charName, time_t rewardTime)
{
    EnsureLoginRewardsLogDirectory();
    std::string dailyLogFilename = "logs/login_rewards/reward_log_" + GetCurrentDateString() + ".csv";
    
    bool fileExists = std::filesystem::exists(dailyLogFilename);

    std::ofstream outfile(dailyLogFilename, std::ios_base::app);
    if (!outfile.is_open())
    {
        LOG_ERROR("module", "[접속 보상] 일일 로그 파일을 열 수 없습니다: {}", dailyLogFilename);
        return;
    }

    if (!fileExists)
    {
        outfile << "AccountID,CharacterName,RewardTimestamp" << std::endl;
    }

    outfile << accountId << "," << charName << "," << Time_tToString(rewardTime) << std::endl;
    outfile.close();
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
    g_loginRewardsShowModuleStatus = true;
    g_loginRewardsDailyGoldAmount = 100000; // 10골드
    g_loginRewardsDailyResetHourKST = 0;
    g_loginRewardsAnnounceMessage = "|cffFF69B4[일일접속보상]|r 일일 접속 보상으로 %gold%골드를 받았습니다!";
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
                else if (key == "LoginRewards.ShowModuleStatus")
                {
                    g_loginRewardsShowModuleStatus = (value == "1");
                }
                else if (key == "LoginRewards.DailyGoldAmount")
                {
                    try { g_loginRewardsDailyGoldAmount = std::stoul(value); }
                    catch (const std::exception& e) { LOG_ERROR("module", "[접속 보상] 잘못된 골드 양 설정: {} ({})", value, e.what()); }
                }
                else if (key == "LoginRewards.DailyResetHourKST")
                {
                    try { g_loginRewardsDailyResetHourKST = std::stoul(value); }
                    catch (const std::exception& e) { LOG_ERROR("module", "[접속 보상] 잘못된 리셋 시간 설정: {} ({})", value, e.what()); }
                }
                else if (key == "LoginRewards.RewardDelaySeconds")
                {
                    try { g_loginRewardsRewardDelaySeconds = std::stoul(value); }
                    catch (const std::exception& e) { LOG_ERROR("module", "[접속 보상] 잘못된 지연 시간 설정: {} ({})", value, e.what()); }
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
            LoadAccountLastRewardData(); // 서버 시작 시 계정 데이터 로드
        }
    }

    void OnShutdown() override
    {
        if (g_loginRewardsEnabled)
        {
            LOG_INFO("module", "[접속 보상] 모듈 비활성화됨. 데이터 저장 중...");
            SaveAccountLastRewardData(); // 서버 종료 시 계정 데이터 저장
        }
    }
};

// 플레이어 이벤트를 처리하는 클래스
class mod_login_rewards_player : public PlayerScript
{
private:
    // OnUpdate 이벤트를 1초에 한 번씩만 실행하기 위한 타이머
    std::unordered_map<uint32, uint32> _updateTimers; // <PlayerGUID, Milliseconds>
    std::mutex _updateTimerMutex;

public:
    mod_login_rewards_player() : PlayerScript("mod_login_rewards_player") { }

    void OnPlayerLogin(Player* player) override
    {
        if (!g_loginRewardsEnabled)
            return;

        // 모듈 활성화 상태 메시지 표시
        if (g_loginRewardsShowModuleStatus)
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cffFF69B4[일일접속보상]|r 이 서버는 접속 보상 모듈이 활성화되어 있습니다.");
        }

        uint32 guid = player->GetGUID().GetCounter();
        // 플레이어의 로그인 시간을 기록
        {
            std::lock_guard<std::mutex> lock(g_playerLoginTimesMutex);
            g_playerLoginTimes[guid] = GameTime::GetGameTime().count();
        }
        // OnUpdate 타이머 초기화
        {
            std::lock_guard<std::mutex> lock(_updateTimerMutex);
            _updateTimers[guid] = 0;
        }
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!g_loginRewardsEnabled)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        // 플레이어 로그인 시간 기록에서 제거
        {
            std::lock_guard<std::mutex> lock(g_playerLoginTimesMutex);
            g_playerLoginTimes.erase(guid);
        }
        // OnUpdate 타이머 기록에서 제거
        {
            std::lock_guard<std::mutex> lock(_updateTimerMutex);
            _updateTimers.erase(guid);
        }
    }

    void OnPlayerUpdate(Player* player, uint32 diff)
    {
        if (!g_loginRewardsEnabled)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        bool timeToCheck = false;

        // 1초마다 체크하기 위한 타이머 로직
        {
            std::lock_guard<std::mutex> lock(_updateTimerMutex);
            auto it = _updateTimers.find(guid);
            if (it == _updateTimers.end())
                return; // 타이머에 없으면 (이미 보상을 받았으면) 중단

            it->second += diff;
            if (it->second >= 1000)
            {
                it->second -= 1000;
                timeToCheck = true;
            }
        }

        if (!timeToCheck)
            return;

        // --- 여기서부터 1초에 한 번씩만 실행됨 ---

        uint32 accountId = player->GetSession()->GetAccountId();
        time_t loginTime = 0;

        {
            std::lock_guard<std::mutex> lock(g_playerLoginTimesMutex);
            auto it = g_playerLoginTimes.find(guid);
            if (it == g_playerLoginTimes.end())
                return; // 로그인 기록이 없으면 중단
            loginTime = it->second;
        }

        time_t currentTime = GameTime::GetGameTime().count();

        // 설정된 지연 시간이 지났는지 확인
        if (currentTime - loginTime < g_loginRewardsRewardDelaySeconds)
            return;

        // --- 보상 자격 확인 로직 ---
        time_t lastRewardTime = 0;
        auto it_reward = g_accountLastRewardInfo.find(accountId);
        if (it_reward != g_accountLastRewardInfo.end())
            lastRewardTime = it_reward->second.timestamp;

        struct tm* currentTm = std::localtime(&currentTime);
        struct tm* lastRewardTm = std::localtime(&lastRewardTime);
        bool canReceiveReward = false;

        if (lastRewardTime == 0)
        {
            canReceiveReward = true;
        }
        else
        {
            bool isNewDay = (currentTm->tm_year > lastRewardTm->tm_year) ||
                            (currentTm->tm_year == lastRewardTm->tm_year && currentTm->tm_yday > lastRewardTm->tm_yday);
            bool hasPassedResetHour = currentTm->tm_hour >= g_loginRewardsDailyResetHourKST;
            bool hadPassedResetHourLastTime = lastRewardTm->tm_hour >= g_loginRewardsDailyResetHourKST;

            if (isNewDay)
            {
                canReceiveReward = hasPassedResetHour;
            }
            else
            {
                if (hasPassedResetHour && !hadPassedResetHourLastTime)
                    canReceiveReward = true;
            }
        }

        if (canReceiveReward)
        {
            player->ModifyMoney(g_loginRewardsDailyGoldAmount);
            std::string charName = player->GetName().c_str();
            LOG_INFO("module", "[접속 보상] 계정 {} (캐릭터: {})에게 {} 골드 지급.", accountId, charName, g_loginRewardsDailyGoldAmount);

            g_accountLastRewardInfo[accountId] = { currentTime, charName };
            SaveAccountLastRewardData();
            LogRewardToFile(accountId, charName, currentTime);

            if (g_loginRewardsShowAnnounceMessage)
            {
                std::string message = g_loginRewardsAnnounceMessage;
                size_t pos = message.find("%gold%");
                if (pos != std::string::npos)
                {
                    std::stringstream gold_ss;
                    gold_ss << (g_loginRewardsDailyGoldAmount / 10000);
                    message.replace(pos, 6, gold_ss.str());
                }
                ChatHandler(player->GetSession()).SendSysMessage(message);
            }

            // 보상 처리 후 더 이상 체크하지 않도록 맵에서 제거
            {
                std::lock_guard<std::mutex> lock(g_playerLoginTimesMutex);
                g_playerLoginTimes.erase(guid);
            }
            {
                std::lock_guard<std::mutex> lock(_updateTimerMutex);
                _updateTimers.erase(guid);
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
