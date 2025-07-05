set(MODULE_NAME "mod-login-rewards")
set(MODULE_SRCS
    src/mod-login-rewards.cpp
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/mod-login-rewards.cpp")

# 설정 파일 설치
install(FILES
    conf/mod-login-rewards.conf.dist
    DESTINATION "${CMAKE_INSTALL_PREFIX}/conf"
)