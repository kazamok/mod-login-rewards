# Login Rewards Module (mod-login-rewards)

## 1. 모듈 소개

`mod-login-rewards`는 아제로스코어(AzerothCore) 월드 서버에서 플레이어가 매일 접속할 때마다 설정된 골드 보상을 자동으로 지급하는 모듈입니다. 이 모듈은 플레이어의 꾸준한 접속을 장려하고, 서버 활성화를 촉진하며, 플레이어들에게 지속적인 동기 부여를 제공하는 데 기여합니다.

이 모듈은 아제로스코어의 핵심 파일을 수정하지 않는 **완전한 플러그인 형태**로 제작되어, 기존 서버 환경에 영향을 주지 않으면서 독립적으로 작동하도록 설계되었습니다.

## 2. 주요 기능

*   **일일 접속 보상**: 매일 특정 시간(UTC 기준)을 기점으로, 해당 날짜에 처음 접속한 플레이어에게 보상을 지급합니다.
*   **골드 보상**: 설정된 양의 골드를 플레이어에게 자동으로 지급합니다.
*   **플레이어별 기록 관리**: 각 플레이어가 마지막으로 보상을 받은 시간을 파일(`player_last_reward.csv`)에 기록하여 중복 지급을 방지하고, 보상 이력을 관리합니다.
*   **자동 지급**: 플레이어가 서버에 로그인할 때 보상 자격을 자동으로 확인하고 지급합니다.
*   **보상 알림**: 보상 지급 시 플레이어에게 개인 메시지를 통해 보상 내역을 안내합니다.
*   **독립적인 설정**: `worldserver.conf` 파일을 직접 수정하지 않고, 모듈 자체의 설정 파일(`mod-login-rewards.conf.dist`)을 통해 모듈의 활성화 여부, 보상 금액, 초기화 시간, 안내 메시지 등을 유연하게 제어할 수 있습니다.
*   **콘솔 로깅**: 모듈의 작동 상태, 보상 지급 내역, 데이터 로드/저장 과정 등을 서버 콘솔에 상세하게 기록하여 운영자가 모듈의 작동 상태를 쉽게 파악할 수 있도록 돕습니다.

## 3. 설치 방법

1.  **모듈 파일 배치**: 이 모듈의 모든 파일을 아제로스코어 소스 코드의 `modules/mod-login-rewards/` 경로에 배치합니다.
    (예시: `C:/azerothcore/modules/mod-login-rewards/`)

2.  **아제로스코어 빌드**: 아제로스코어 프로젝트를 다시 빌드합니다. 이 과정에서 `mod-login-rewards` 모듈이 함께 컴파일되고, 필요한 설정 파일이 설치 경로로 복사됩니다.
    (빌드 과정은 사용자의 환경에 따라 다를 수 있습니다. 일반적으로 `cmake --build .` 명령을 사용합니다.)

3.  **설정 파일 준비**: 빌드 완료 후, 아제로스코어 설치 디렉토리의 `configs/modules/` 폴더에 `mod-login-rewards.conf.dist` 파일이 생성됩니다. 이 파일은 모듈의 설정 파일로 사용됩니다.

    *   **설정 파일 경로 예시**: `C:/BUILD/bin/RelWithDebInfo/configs/modules/mod-login-rewards.conf.dist`

## 4. 설정 (Configuration)

`mod-login-rewards.conf.dist` 파일에서 다음 옵션들을 설정할 수 있습니다.

```ini
#----------------------------------------------------------
# Login Rewards Module Settings
#----------------------------------------------------------
#
# Enable Login Rewards Module (0 = disabled, 1 = enabled)
LoginRewards.Enable = 1

# Daily gold amount to reward (in Copper)
LoginRewards.DailyGoldAmount = 100000

# Daily reward reset hour (UTC, 0-23)
LoginRewards.DailyResetHourUTC = 0

# Message to send to player upon receiving reward
LoginRewards.AnnounceMessage = "일일 접속 보상으로 %gold%골드를 받았습니다!"

# Show announce message (0 = disabled, 1 = enabled)
LoginRewards.ShowAnnounceMessage = 1
```

*   `LoginRewards.Enable`: 모듈의 기능을 전체적으로 켜거나 끕니다. 기본값은 `1` (활성화)입니다.
*   `LoginRewards.DailyGoldAmount`: 매일 지급할 골드 보상 금액을 **구리(Copper) 단위**로 설정합니다. (예: `100000`은 1골드)
*   `LoginRewards.DailyResetHourUTC`: 일일 보상이 초기화되는 시간을 **UTC 기준 0시부터 23시까지**로 설정합니다. (예: `0`은 UTC 자정)
*   `LoginRewards.AnnounceMessage`: 플레이어가 보상을 받을 때 개인 챗창에 표시될 메시지를 설정합니다. 메시지 내에서 `%gold%` 플레이스홀더를 사용하면 실제 지급된 골드(금화 단위)로 자동 치환됩니다.
*   `LoginRewards.ShowAnnounceMessage`: `LoginRewards.AnnounceMessage`의 표시 여부를 설정합니다. 기본값은 `1` (표시)입니다.

## 5. 사용 방법

1.  **월드 서버 실행**: 설정이 완료된 후 월드 서버를 실행합니다.

2.  **보상 지급 확인**: 플레이어가 서버에 로그인할 때마다 모듈이 보상 자격을 확인하고, 조건이 충족되면 `LoginRewards.DailyGoldAmount`에 설정된 골드를 지급합니다. 보상 지급 시 `LoginRewards.AnnounceMessage`가 플레이어에게 표시됩니다.

3.  **데이터 파일 확인**: 플레이어별 마지막 보상 지급 기록은 `logs/login_rewards/player_last_reward.csv` 파일에 저장됩니다. 이 파일은 서버 시작 시 로드되고, 보상 지급 시 업데이트되며, 서버 종료 시 저장됩니다.

    *   **데이터 파일 경로 예시**: `C:/BUILD/bin/RelWithDebInfo/logs/login_rewards/player_last_reward.csv`

4.  **콘솔 로그 확인**: 월드 서버 콘솔에서 모듈의 작동 상태, 보상 지급 내역, 데이터 로드/저장 과정 등을 확인할 수 있습니다.

## 6. 향후 개선 사항

*   주간, 월간, 또는 연속 접속 보상 기능 추가.
*   골드 외에 아이템, 경험치, 명예 점수 등 다양한 보상 유형 지원.
*   보상 지급 시 플레이어에게 시각적 효과 또는 사운드 재생.
*   인게임 명령어를 통한 보상 수동 지급 또는 설정 변경 기능.

---