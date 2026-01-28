# ============================================================================
# 跨平台通用CMakeLists.txt配置文件
# 支持Qt5/Qt6、MFC、多种编译配置、可执行文件/动态库等
# ============================================================================

cmake_minimum_required(VERSION 3.16)

# ============================================================================
# 项目配置区域
# ============================================================================
project(BigFileViewer VERSION 1.0.0 LANGUAGES CXX C)

# ============================================================================
# 全局开关配置（在这里统一管理所有功能开关）
# ============================================================================
option(ENABLE_QT "启用Qt支持" ON)
option(USE_QT6 "使用Qt6（OFF则使用Qt5）" ON)
option(ENABLE_MFC "启用MFC支持（仅Windows）" OFF)
option(BUILD_SHARED_LIBS "构建动态库（OFF则构建静态库或可执行文件）" OFF)
option(BUILD_EXECUTABLE "构建可执行文件（OFF则构建库）" ON)
option(ENABLE_QT_TRANSLATION "启用Qt翻译支持" ON)
option(ENABLE_TESTING "启用单元测试" OFF)
option(ENABLE_INSTALLER "启用安装程序打包" OFF)
option(ENABLE_PCH "启用预编译头" OFF)
option(ENABLE_UNITY_BUILD "启用Unity构建以加速编译" OFF)
option(ENABLE_LTO "启用链接时优化（LTO）" OFF)
option(ENABLE_ASAN "启用地址消毒器（Address Sanitizer）" OFF)
option(ENABLE_TSAN "启用线程消毒器（Thread Sanitizer）" OFF)
option(STRIP_RELEASE "Release模式下移除调试符号" ON)
option(USE_GBK_ENCODING "源码使用GBK编码（OFF则使用UTF-8）" OFF)
option(ENABLE_PLATFORM_LIBS "启用平台特定库（通常Qt项目不需要）" OFF)
option(ENABLE_LICENSE_SYSTEM "启用RSA许可证验证系统（需要OpenSSL）" OFF)

# 第三方依赖管理选项
option(ENABLE_FETCHCONTENT "启用FetchContent自动下载依赖" OFF)
option(ENABLE_VCPKG "启用vcpkg包管理器支持" OFF)
option(ENABLE_CONAN "启用Conan包管理器支持" OFF)

# ============================================================================
# BigFileViewer 功能分层选项
# ============================================================================
option(ENABLE_PRO_FEATURES "启用 Pro 版功能（过滤模板、工作区、统计等）" ON)  # ★ 测试模式默认开启
option(ENABLE_REMOTE_FEATURES "启用 Enterprise 远程功能（SFTP/SSH、SMTP、集成）需要 libssh2" OFF)  # ★ 测试模式改为关闭（避免 libssh2 编译问题）

# 代码质量工具选项
option(ENABLE_CLANG_TIDY "启用Clang-Tidy静态分析" OFF)
option(ENABLE_CLANG_FORMAT "启用Clang-Format代码格式化检查" OFF)
option(ENABLE_CPPCHECK "启用CppCheck静态分析" OFF)

# ============================================================================
# C++标准设置
# ============================================================================
set(CMAKE_CXX_STANDARD 17)              # 设置C++标准版本
set(CMAKE_CXX_STANDARD_REQUIRED ON)     # 要求必须支持该标准
set(CMAKE_CXX_EXTENSIONS OFF)           # 禁用编译器扩展

# ============================================================================
# 构建类型配置（支持Debug、Release、RelWithDebInfo、MinSizeRel）
# ============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "构建类型" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

# ============================================================================
# 输出目录配置
# ============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)  # 可执行文件输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)  # 动态库输出目录（.so, .dylib）
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)  # 静态库输出目录（.a, .lib）

# 禁用 Visual Studio 生成器自动添加配置名称子目录（Debug/Release）
# 使所有配置的输出都在同一目录下
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})

# ============================================================================
# Qt配置区域
# ============================================================================
if(ENABLE_QT)
    # 允许用户指定Qt安装路径
    
    if(USE_QT6)
        if(APPLE)
            set(QT_DIR "/Users/buzhiliang/Qt/6.10.1/macos" CACHE PATH "Qt安装路径")
        else()
            set(QT_DIR "E:/QT/6.10.1/msvc2022_64" CACHE PATH "Qt安装路径（例如：C:/Qt/6.5.0/msvc2019_64）")
        endif()
    else()
        set(QT_DIR "D:/env/msvc2019_64" CACHE PATH "Qt安装路径（例如：C:/Qt/6.5.0/msvc2019_64）")
    endif()
    
    if(QT_DIR)
        list(APPEND CMAKE_PREFIX_PATH ${QT_DIR})
    endif()
    
    # 自动处理Qt的MOC、UIC、RCC
    set(CMAKE_AUTOMOC ON)               # 自动运行moc（Meta-Object Compiler）
    set(CMAKE_AUTOUIC ON)               # 自动运行uic（UI Compiler）
    set(CMAKE_AUTORCC ON)               # 自动运行rcc（Resource Compiler）
    set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/ui)  # UI文件搜索路径
    
    # 选择Qt版本
    if(USE_QT6)
        find_package(Qt6 COMPONENTS Core Widgets Gui Concurrent Network LinguistTools Quick Qml QuickControls2 Sql QUIET)
        if(Qt6_FOUND)
            set(QT_VERSION_MAJOR 6)
            message(STATUS "找到Qt6: ${Qt6_VERSION}")
            # 可选：尝试查找 Multimedia 模块（如果不存在则跳过）
            # 保存 Qt6_FOUND 的值，防止可选模块的查找改变它
            set(Qt6_FOUND_BACKUP ${Qt6_FOUND})
            find_package(Qt6 COMPONENTS Multimedia QUIET)
            set(Qt6_FOUND ${Qt6_FOUND_BACKUP})
            if(Qt6Multimedia_FOUND)
                message(STATUS "找到Qt6Multimedia模块")
            else()
                message(STATUS "未找到Qt6Multimedia模块，继续构建（某些多媒体功能可能不可用）")
            endif()
        else()
            message(FATAL_ERROR "未找到Qt6！请检查QT_DIR设置或安装Qt6")
        endif()
    else()
        find_package(Qt5 COMPONENTS Core Widgets Gui Concurrent Network LinguistTools Sql QUIET)
        if(Qt5_FOUND)
            set(QT_VERSION_MAJOR 5)
            message(STATUS "找到Qt5: ${Qt5_VERSION}")
        else()
            message(FATAL_ERROR "未找到Qt5！请检查QT_DIR设置或安装Qt5")
        endif()
    endif()
endif()

# ============================================================================
# MFC配置（仅Windows）
# ============================================================================
if(ENABLE_MFC AND WIN32)
    set(CMAKE_MFC_FLAG 2)               # 1=静态链接MFC, 2=动态链接MFC
    add_definitions(-D_AFXDLL)          # 定义MFC宏
    # 注意：设置CMAKE_MFC_FLAG后，链接器会自动链接MFC库（mfc140.lib等）
    # 因此不需要手动调用target_link_libraries添加MFC库
    message(STATUS "MFC支持已启用（动态链接）")
endif()

# ============================================================================
# 编译器特定配置
# ============================================================================
if(MSVC)
    # Visual Studio编译器选项
    add_compile_options(/W4)            # 警告级别4
    add_compile_options(/MP)            # 多进程编译
    #add_compile_options("/utf-8")

    # 源码编码设置
    if(USE_GBK_ENCODING)
        # GBK编码支持（中文Windows环境常用）
        add_compile_options(/source-charset:gbk)    # 源文件使用GBK编码
        add_compile_options(/execution-charset:gbk) # 执行字符集使用GBK
        message(STATUS "使用GBK编码编译")
    else()
        # UTF-8编码支持（推荐，跨平台）
        add_compile_options(/utf-8)     # 源文件和执行字符集都使用UTF-8
        message(STATUS "使用UTF-8编码编译")
    endif()
    
    if(ENABLE_MFC)
        add_compile_options(/EHsc)      # 启用C++异常处理
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC/Clang编译器选项
    add_compile_options(-Wall -Wextra)  # 启用警告
    
    # GCC/Clang的编码设置
    if(USE_GBK_ENCODING)
        add_compile_options(-finput-charset=GBK)    # 输入文件使用GBK
        add_compile_options(-fexec-charset=GBK)     # 执行字符集使用GBK
        message(STATUS "使用GBK编码编译")
    else()
        add_compile_options(-finput-charset=UTF-8)  # 输入文件使用UTF-8
        add_compile_options(-fexec-charset=UTF-8)   # 执行字符集使用UTF-8
        message(STATUS "使用UTF-8编码编译")
    endif()
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)     # 调试信息，无优化
    else()
        add_compile_options(-O3)        # 最高优化级别
        if(STRIP_RELEASE)
            add_link_options(-s)        # 移除符号表
        endif()
    endif()
endif()

# ============================================================================
# 链接时优化（LTO）
# ============================================================================
if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "链接时优化（LTO）已启用")
    else()
        message(WARNING "LTO不支持: ${ipo_error}")
    endif()
endif()

# ============================================================================
# 消毒器配置（用于调试内存/线程问题）
# ============================================================================
if(ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
    message(STATUS "地址消毒器（ASan）已启用")
endif()

if(ENABLE_TSAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
    message(STATUS "线程消毒器（TSan）已启用")
endif()

# ============================================================================
# 代码质量工具配置
# ============================================================================

# Clang-Tidy静态代码分析
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        # 可以在项目根目录创建.clang-tidy文件来配置规则
        set(CMAKE_CXX_CLANG_TIDY 
            ${CLANG_TIDY_EXE};
            -checks=*,-fuchsia-*,-google-*,-zircon-*,-abseil-*,-modernize-use-trailing-return-type;
            -warnings-as-errors=*;
        )
        message(STATUS "Clang-Tidy已启用: ${CLANG_TIDY_EXE}")
        message(STATUS "  可以创建.clang-tidy文件自定义规则")
    else()
        message(WARNING "未找到clang-tidy，请安装后重新配置")
    endif()
endif()

# Clang-Format代码格式化检查
if(ENABLE_CLANG_FORMAT)
    find_program(CLANG_FORMAT_EXE NAMES clang-format)
    if(CLANG_FORMAT_EXE)
        # 查找所有需要格式化的文件
        file(GLOB_RECURSE ALL_CXX_SOURCE_FILES
            ${CMAKE_SOURCE_DIR}/*.cpp
            ${CMAKE_SOURCE_DIR}/*.cc
            ${CMAKE_SOURCE_DIR}/*.cxx
            ${CMAKE_SOURCE_DIR}/include/*.h
            ${CMAKE_SOURCE_DIR}/include/*.hpp
        )
        
        # 添加格式化检查目标
        add_custom_target(format-check
            COMMAND ${CLANG_FORMAT_EXE}
            --dry-run
            --Werror
            ${ALL_CXX_SOURCE_FILES}
            COMMENT "检查代码格式"
        )
        
        # 添加自动格式化目标
        add_custom_target(format
            COMMAND ${CLANG_FORMAT_EXE}
            -i
            ${ALL_CXX_SOURCE_FILES}
            COMMENT "自动格式化代码"
        )
        
        message(STATUS "Clang-Format已启用: ${CLANG_FORMAT_EXE}")
        message(STATUS "  运行 'make format-check' 检查格式")
        message(STATUS "  运行 'make format' 自动格式化")
        message(STATUS "  可以创建.clang-format文件自定义格式")
    else()
        message(WARNING "未找到clang-format，请安装后重新配置")
    endif()
endif()

# CppCheck静态代码分析
if(ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXE NAMES cppcheck)
    if(CPPCHECK_EXE)
        set(CMAKE_CXX_CPPCHECK 
            ${CPPCHECK_EXE};
            --enable=warning,style,performance,portability;
            --inline-suppr;
            --suppress=missingIncludeSystem;
            --error-exitcode=1;
        )
        message(STATUS "CppCheck已启用: ${CPPCHECK_EXE}")
    else()
        message(WARNING "未找到cppcheck，请安装后重新配置")
    endif()
endif()

# ============================================================================
# 第三方依赖管理配置
# ============================================================================

# vcpkg包管理器支持
if(ENABLE_VCPKG)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
        message(STATUS "vcpkg已启用: $ENV{VCPKG_ROOT}")
        message(STATUS "  使用vcpkg.json或在CMakeLists.txt中使用find_package查找包")
    else()
        message(WARNING "未设置VCPKG_ROOT环境变量，请安装vcpkg并设置环境变量")
        message(STATUS "  示例: export VCPKG_ROOT=/path/to/vcpkg")
    endif()
endif()

# Conan包管理器支持
if(ENABLE_CONAN)
    # 检查是否存在conanfile.txt或conanfile.py
    if(EXISTS "${CMAKE_SOURCE_DIR}/conanfile.txt" OR EXISTS "${CMAKE_SOURCE_DIR}/conanfile.py")
        # 自动运行conan install
        if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
            message(STATUS "下载conan.cmake...")
            file(DOWNLOAD
                "https://raw.githubusercontent.com/conan-io/cmake-conan/0.18.1/conan.cmake"
                "${CMAKE_BINARY_DIR}/conan.cmake"
                TLS_VERIFY ON
            )
        endif()
        
        include(${CMAKE_BINARY_DIR}/conan.cmake)
        
        conan_cmake_configure(
            REQUIRES fmt/9.1.0 spdlog/1.11.0
            GENERATORS cmake_find_package
            OPTIONS ${CONAN_OPTIONS}
        )
        
        conan_cmake_autodetect(settings)
        
        conan_cmake_install(
            PATH_OR_REFERENCE .
            BUILD missing
            REMOTE conancenter
            SETTINGS ${settings}
        )
        
        message(STATUS "Conan已启用")
        message(STATUS "  在conanfile.txt或conanfile.py中配置依赖")
    else()
        message(WARNING "未找到conanfile.txt或conanfile.py")
        message(STATUS "  请创建conanfile.txt配置依赖")
    endif()
endif()

# FetchContent自动下载依赖
if(ENABLE_FETCHCONTENT)
    include(FetchContent)
    message(STATUS "FetchContent已启用")
    message(STATUS "  可以使用FetchContent_Declare添加依赖")
    
    # 示例：下载常用库（可以根据需要取消注释）
    
    # # 示例1：下载GoogleTest测试框架
    # if(ENABLE_TESTING)
    #     FetchContent_Declare(
    #         googletest
    #         GIT_REPOSITORY https://github.com/google/googletest.git
    #         GIT_TAG v1.14.0
    #     )
    #     FetchContent_MakeAvailable(googletest)
    #     message(STATUS "  GoogleTest已通过FetchContent下载")
    # endif()
    
    # # 示例2：下载spdlog日志库
    # FetchContent_Declare(
    #     spdlog
    #     GIT_REPOSITORY https://github.com/gabime/spdlog.git
    #     GIT_TAG v1.12.0
    # )
    # FetchContent_MakeAvailable(spdlog)
    # message(STATUS "  spdlog已通过FetchContent下载")
    
    # # 示例3：下载fmt格式化库
    # FetchContent_Declare(
    #     fmt
    #     GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    #     GIT_TAG 10.1.1
    # )
    # FetchContent_MakeAvailable(fmt)
    # message(STATUS "  fmt已通过FetchContent下载")
    
    # # 示例4：下载nlohmann/json
    # FetchContent_Declare(
    #     json
    #     GIT_REPOSITORY https://github.com/nlohmann/json.git
    #     GIT_TAG v3.11.2
    # )
    # FetchContent_MakeAvailable(json)
    # message(STATUS "  nlohmann/json已通过FetchContent下载")
endif()

# Qt UI文件
if(ENABLE_QT)
    file(GLOB_RECURSE UI_FILES 
        "${CMAKE_SOURCE_DIR}/*.ui"
        "${CMAKE_SOURCE_DIR}/*.ui"
    )
    
    # Qt资源文件
    file(GLOB_RECURSE QRC_FILES 
        "${CMAKE_SOURCE_DIR}/*.qrc"
        "${CMAKE_SOURCE_DIR}/*.qrc"
    )
    
    # =========================================================================
    # Qt 自动化国际化 (i18n) 配置
    # =========================================================================
    # 使用 qt_add_translations 或 qt_create_translation 实现自动化：
    # - 自动运行 lupdate 扫描源文件中的 tr() 字符串
    # - 自动创建/更新 .ts 翻译文件
    # - 自动编译 .ts -> .qm
    # - 将 .qm 嵌入到可执行文件资源中
    # =========================================================================
    if(ENABLE_QT_TRANSLATION)
        message(STATUS "正在配置自动化国际化 (i18n)...")
        
        # 定义翻译文件路径（存放在 translations 目录）
        set(TRANSLATIONS_DIR "${CMAKE_SOURCE_DIR}/translations")
        file(MAKE_DIRECTORY ${TRANSLATIONS_DIR})
        
        # 定义要生成的翻译文件
        set(TS_FILES
            "${TRANSLATIONS_DIR}/BigFileViewer_en_US.ts"
            "${TRANSLATIONS_DIR}/BigFileViewer_zh_CN.ts"
            "${TRANSLATIONS_DIR}/BigFileViewer_zh_TW.ts"
            "${TRANSLATIONS_DIR}/BigFileViewer_ja_JP.ts"
            "${TRANSLATIONS_DIR}/BigFileViewer_ko_KR.ts"
            "${TRANSLATIONS_DIR}/BigFileViewer_ru_RU.ts"
            "${TRANSLATIONS_DIR}/BigFileViewer_de_DE.ts"
            "${TRANSLATIONS_DIR}/BigFileViewer_fr_FR.ts"
            "${TRANSLATIONS_DIR}/BigFileViewer_es_ES.ts"
            "${TRANSLATIONS_DIR}/BigFileViewer_pt_BR.ts"
            "${TRANSLATIONS_DIR}/BigFileViewer_it_IT.ts"
            "${TRANSLATIONS_DIR}/BigFileViewer_ar_SA.ts"
            "${TRANSLATIONS_DIR}/BigFileViewer_hi_IN.ts"
        )
        
        # 收集需要扫描的源文件（用于 lupdate）
        # 注意：这里直接使用 glob，因为 SOURCE_FILES 变量可能还未定义
        file(GLOB_RECURSE I18N_SOURCE_FILES
            "${CMAKE_SOURCE_DIR}/src/*.cpp"
            "${CMAKE_SOURCE_DIR}/src/*.h"
            "${CMAKE_SOURCE_DIR}/src/*.hpp"
            "${CMAKE_SOURCE_DIR}/src/*.ui"
            "${CMAKE_SOURCE_DIR}/src/*.qml"
        )
        
        message(STATUS "  翻译文件: ${TS_FILES}")
        message(STATUS "  扫描源文件数量: ${CMAKE_SOURCE_DIR}/src/*.cpp, *.h, *.qml")
    endif()
endif()

# Windows资源文件（应用程序图标等）
# 注意：如果 app_icon.ico 实际上是 PNG 文件（重命名的），需要使用工具转换为真正的 ICO 格式
# 可以使用 ImageMagick: magick convert app_icon.png app_icon.ico
# 或在线工具: https://icoconvert.com/
if(WIN32)
    # 直接使用 src/logo.rc 文件（包含应用程序图标）
    set(RC_FILES "${CMAKE_SOURCE_DIR}/src/logo.rc")
    if(EXISTS "${RC_FILES}")
        # 检查.ico文件是否存在
        set(ICON_FILE "${CMAKE_SOURCE_DIR}/src/app_icon.ico")
        if(EXISTS "${ICON_FILE}")
            # 暂时禁用 .rc 文件 - 需要将 PNG 转换为真正的 ICO 格式
            # message(STATUS "Windows资源文件: ${RC_FILES}")
            #message(STATUS "注意: app_icon.ico 是 PNG 格式，需要转换为 ICO 格式才能嵌入到 EXE")
            #message(STATUS "临时禁用 Windows 资源文件，请使用 ImageMagick 或在线工具转换图标")
            #set(RC_FILES "")  # 临时禁用
        endif()
    else()
        message(WARNING "未找到Windows资源文件: ${RC_FILES}")
        set(RC_FILES "")
    endif()
endif()
if(WIN32)
    set(HEAD_DIR "CommonFile-64")
    set(OUTPUT_DIR "-64")
else()
    set(HEAD_DIR "CommonFile")
    set(OUTPUT_DIR "")
 endif() 
 set(COMMON_SPACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../CommonSpace") # 公共空间目录

# ============================================================================
# BuildConfig.h 生成（功能开关宏定义）
# ============================================================================
# 根据选项设置 C++ 宏
if(ENABLE_PRO_FEATURES)
    set(BFV_PRO_FEATURES 1)
    message(STATUS "Pro 版功能已启用")
else()
    unset(BFV_PRO_FEATURES)
    message(STATUS "Pro 版功能已禁用")
endif()

if(ENABLE_REMOTE_FEATURES)
    set(BFV_REMOTE_FEATURES 1)
    # 远程功能需要 Pro 功能
    if(NOT ENABLE_PRO_FEATURES)
        message(WARNING "ENABLE_REMOTE_FEATURES 需要 ENABLE_PRO_FEATURES，自动启用 Pro 功能")
        set(ENABLE_PRO_FEATURES ON CACHE BOOL "启用 Pro 版功能" FORCE)
        set(BFV_PRO_FEATURES 1)
    endif()
    message(STATUS "Enterprise 远程功能已启用")
else()
    unset(BFV_REMOTE_FEATURES)
    message(STATUS "Enterprise 远程功能已禁用")
endif()

# 生成 BuildConfig.h
configure_file(
    "${CMAKE_SOURCE_DIR}/src/BuildConfig.h.in"
    "${CMAKE_BINARY_DIR}/BuildConfig.h"
    @ONLY
)

# 确保生成的头文件可以被找到
include_directories(${CMAKE_BINARY_DIR})

# ============================================================================
# Enterprise 远程功能依赖（mbedTLS + libssh2）
# ============================================================================
if(ENABLE_REMOTE_FEATURES)
    include(FetchContent)
    
    message(STATUS "正在配置远程功能依赖（mbedTLS + libssh2）...")
    
    # 保存全局输出目录设置（避免影响第三方库）
    set(_SAVED_CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
    set(_SAVED_CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG})
    set(_SAVED_CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE})
    set(_SAVED_CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO})
    set(_SAVED_CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL})
    
    # 取消全局输出目录设置（让第三方库使用默认目录）
    unset(CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    unset(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG)
    unset(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE)
    unset(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO)
    unset(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL)
    
    # 1. 拉取 mbedTLS（纯 C 库，无外部依赖）
    FetchContent_Declare(mbedtls
        GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
        GIT_TAG        v3.6.0
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
    )
    # mbedTLS 构建选项
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
    
    # 先拉取并构建 mbedTLS
    FetchContent_MakeAvailable(mbedtls)
    
    # 获取 mbedTLS 路径
    FetchContent_GetProperties(mbedtls SOURCE_DIR MBEDTLS_SOURCE_DIR BINARY_DIR MBEDTLS_BINARY_DIR)
    
    # 设置 libssh2 查找 MbedTLS 所需的变量
    set(MBEDTLS_INCLUDE_DIR "${MBEDTLS_SOURCE_DIR}/include" CACHE PATH "" FORCE)
    set(MBEDTLS_LIBRARY mbedtls CACHE STRING "" FORCE)
    set(MBEDCRYPTO_LIBRARY mbedcrypto CACHE STRING "" FORCE)
    set(MBEDX509_LIBRARY mbedx509 CACHE STRING "" FORCE)
    
    # 2. 拉取 libssh2
    FetchContent_Declare(libssh2
        GIT_REPOSITORY https://github.com/libssh2/libssh2.git
        GIT_TAG        libssh2-1.11.1
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
    )
    # libssh2 构建选项
    set(CRYPTO_BACKEND "mbedTLS" CACHE STRING "" FORCE)
    set(BUILD_SHARED_LIBS_SAVED ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(LIBSSH2_NO_EXAMPLES ON CACHE BOOL "" FORCE)
    
    # 手动 populate 并添加 subdirectory
    FetchContent_GetProperties(libssh2)
    if(NOT libssh2_POPULATED)
        FetchContent_Populate(libssh2)
        
        # 读取 libssh2/src/CMakeLists.txt 并禁用 export/install 导出
        set(_libssh2_src_cmake "${libssh2_SOURCE_DIR}/src/CMakeLists.txt")
        file(READ "${_libssh2_src_cmake}" _content)
        
        # 替换 export(TARGETS ... 行（将整个 export 调用注释掉）
        string(REPLACE 
            "export(TARGETS \${_libssh2_export} NAMESPACE \"\${PROJECT_NAME}::\" FILE \"\${PROJECT_NAME}-targets.cmake\")"
            "# export(TARGETS \${_libssh2_export} NAMESPACE \"\${PROJECT_NAME}::\" FILE \"\${PROJECT_NAME}-targets.cmake\")  # Disabled by BigFileViewer"
            _content "${_content}")
        
        # 替换 export(PACKAGE ...) 行
        string(REPLACE 
            "export(PACKAGE \${PROJECT_NAME})"
            "# export(PACKAGE \${PROJECT_NAME})  # Disabled by BigFileViewer"
            _content "${_content}")
        
        # 替换 install(EXPORT ...) - 这是三行的命令
        string(REPLACE 
            "install(EXPORT \"\${PROJECT_NAME}-targets\"\n  NAMESPACE \"\${PROJECT_NAME}::\"\n  DESTINATION \"\${CMAKE_INSTALL_LIBDIR}/cmake/\${PROJECT_NAME}\")"
            "# install(EXPORT \"\${PROJECT_NAME}-targets\"\n#   NAMESPACE \"\${PROJECT_NAME}::\"\n#   DESTINATION \"\${CMAKE_INSTALL_LIBDIR}/cmake/\${PROJECT_NAME}\")  # Disabled by BigFileViewer"
            _content "${_content}")
        
        # 写回文件
        file(WRITE "${_libssh2_src_cmake}" "${_content}")
        
        add_subdirectory(${libssh2_SOURCE_DIR} ${libssh2_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
    
    # 恢复 BUILD_SHARED_LIBS
    set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_SAVED} CACHE BOOL "" FORCE)
    
    # 设置 libssh2 包含目录
    set(LIBSSH2_SOURCE_DIR ${libssh2_SOURCE_DIR})
    
    message(STATUS "  mbedTLS 源目录: ${MBEDTLS_SOURCE_DIR}")
    message(STATUS "  libssh2 源目录: ${LIBSSH2_SOURCE_DIR}")
    message(STATUS "  mbedTLS 和 libssh2 配置完成")
    
    # 恢复全局输出目录设置（用于主项目目标）
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${_SAVED_CMAKE_ARCHIVE_OUTPUT_DIRECTORY})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${_SAVED_CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${_SAVED_CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${_SAVED_CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${_SAVED_CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL})
endif()

# ============================================================================
# 源文件收集
# ============================================================================
# 收集源文件
file(GLOB_RECURSE SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.cc"
    "${CMAKE_SOURCE_DIR}/src/*.cxx"
)

# 收集头文件
file(GLOB_RECURSE HEADER_FILES
    "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/src/*.hpp"
)

# 注意：旧的 QWidgets 文件（MainWindow, SearchBar, AdvancedFilterWidget, 
# RegistrationDialog, CompactLineDelegate）已被删除，QML 版本不再需要这些文件

message(STATUS "源文件数量: ${SOURCE_FILES}")
message(STATUS "头文件数量: ${HEADER_FILES}")

# ============================================================================
# 包含目录设置
# ============================================================================
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}         # 用于Qt生成的文件
)

# ============================================================================
# 创建目标（可执行文件或库）
# ============================================================================
set(TARGET_NAME ${PROJECT_NAME})

if(BUILD_EXECUTABLE)
    # 创建可执行文件
    if(WIN32 AND ENABLE_QT)
        # Windows下Qt应用程序（无控制台窗口）
        add_executable(${TARGET_NAME} WIN32 
            ${SOURCE_FILES} 
            ${HEADER_FILES} 
            ${UI_FILES} 
            ${QRC_FILES}
            ${RC_FILES}  # Windows资源文件（应用程序图标）
        )
    elseif(APPLE AND ENABLE_QT)
        # macOS下Qt应用程序（Bundle）
        add_executable(${TARGET_NAME} MACOSX_BUNDLE 
            ${SOURCE_FILES} 
            ${HEADER_FILES} 
            ${UI_FILES} 
            ${QRC_FILES}
        )        
        # 设置 macOS Bundle 属性
        set_target_properties(${TARGET_NAME} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/deploy/Info.plist"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.bigfileread.BigFileViewer"
            MACOSX_BUNDLE_BUNDLE_NAME "BigFileViewer"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        )
    else()
        # 普通可执行文件
        add_executable(${TARGET_NAME} 
            ${SOURCE_FILES} 
            ${HEADER_FILES} 
            ${UI_FILES} 
            ${QRC_FILES}
        )
    endif()
else()
    # 创建库（动态库或静态库）
    if(BUILD_SHARED_LIBS)
        add_library(${TARGET_NAME} SHARED 
            ${SOURCE_FILES} 
            ${HEADER_FILES} 
            ${UI_FILES} 
            ${QRC_FILES}
        )
        message(STATUS "构建动态库")
    else()
        add_library(${TARGET_NAME} STATIC 
            ${SOURCE_FILES} 
            ${HEADER_FILES} 
            ${UI_FILES} 
            ${QRC_FILES}
        )
        message(STATUS "构建静态库")
    endif()
endif()

# ============================================================================
# Unity构建（加速编译）
# ============================================================================
if(ENABLE_UNITY_BUILD)
    set_target_properties(${TARGET_NAME} PROPERTIES UNITY_BUILD ON)
    message(STATUS "Unity构建已启用")
endif()

# ============================================================================
# 预编译头
# ============================================================================
if(ENABLE_PCH AND EXISTS "${CMAKE_SOURCE_DIR}/src/pch.h")
    target_precompile_headers(${TARGET_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/pch.h")
    message(STATUS "预编译头已启用")
endif()
#=============================================================================
# 添加头文件目录
#=============================================================================
target_include_directories(${PROJECT_NAME} PRIVATE
    # 当前项目目录
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    
    # 生成目录 (AUTOMOC生成的文件)
    ${CMAKE_CURRENT_BINARY_DIR}
)

# 注意：外部依赖库已移除，BigFileViewer 仅依赖 Qt

# ============================================================================
# Qt链接库
# ============================================================================
if(ENABLE_QT)
    if(QT_VERSION_MAJOR EQUAL 6)
        target_link_libraries(${TARGET_NAME} PRIVATE 
            Qt6::Core 
            Qt6::Widgets 
            Qt6::Gui
            Qt6::Concurrent
            Qt6::Network
            Qt6::Quick
            Qt6::Qml
            Qt6::QuickControls2
            Qt6::Sql
        )
        # Optional: Qt Multimedia for sound notifications
        if(TARGET Qt6::Multimedia)
            target_link_libraries(${TARGET_NAME} PRIVATE Qt6::Multimedia)
            target_compile_definitions(${TARGET_NAME} PRIVATE HAVE_QT_MULTIMEDIA)
            message(STATUS "Qt6::Multimedia 已启用")
        else()
            message(STATUS "Qt6::Multimedia 未找到 - 声音通知功能将禁用")
        endif()
    else()
        target_link_libraries(${TARGET_NAME} PRIVATE 
            Qt5::Core 
            Qt5::Widgets 
            Qt5::Gui
            Qt5::Concurrent
            Qt5::Network
            Qt5::Sql
        )
    endif()
    
    # =========================================================================
    # 远程功能库链接（libssh2 + mbedTLS）
    # =========================================================================
    if(ENABLE_REMOTE_FEATURES)
        # 添加 libssh2 包含目录
        target_include_directories(${TARGET_NAME} PRIVATE 
            ${LIBSSH2_SOURCE_DIR}/include
            ${CMAKE_BINARY_DIR}/_deps/libssh2-build/src  # 生成的 libssh2_config.h
        )
        
        # 直接链接生成的库文件（避免 CMake 目标传播依赖问题）
        set(_MBEDTLS_LIB_DIR "${CMAKE_BINARY_DIR}/_deps/mbedtls-build/library")
        set(_LIBSSH2_LIB_DIR "${CMAKE_BINARY_DIR}/_deps/libssh2-build/src")
        
        target_link_libraries(${TARGET_NAME} PRIVATE 
            "${_LIBSSH2_LIB_DIR}/libssh2.lib"
            "${_MBEDTLS_LIB_DIR}/mbedtls.lib"
            "${_MBEDTLS_LIB_DIR}/mbedcrypto.lib"
            "${_MBEDTLS_LIB_DIR}/mbedx509.lib"
        )
        
        # 添加依赖关系确保先构建这些库
        add_dependencies(${TARGET_NAME} libssh2_static mbedtls mbedcrypto mbedx509)
        
        # Windows 需要 Winsock 和 bcrypt 库（mbedTLS/libssh2 需要）
        if(WIN32)
            target_link_libraries(${TARGET_NAME} PRIVATE ws2_32 bcrypt tdh advapi32)
        endif()
        
        message(STATUS "已链接 libssh2 和 mbedTLS 库")
    endif()
    
    # =========================================================================
    # =========================================================================
    # Qt 翻译处理 (自动生成 .ts 文件)
    # =========================================================================
    if(ENABLE_QT_TRANSLATION AND TS_FILES)
        message(STATUS "配置自动化翻译构建...")
        
        if(QT_VERSION_MAJOR EQUAL 6)
            qt6_create_translation(QM_FILES
                ${I18N_SOURCE_FILES}
                ${TS_FILES}
            )
        else()
            qt5_create_translation(QM_FILES
                ${I18N_SOURCE_FILES}
                ${TS_FILES}
            )
        endif()
        
        message(STATUS "  .ts 文件: ${TS_FILES}")
        message(STATUS "  .qm 文件: ${QM_FILES}")
        add_custom_target(translations ALL DEPENDS ${QM_FILES})
        add_dependencies(${TARGET_NAME} translations)
        
        set(TRANSLATIONS_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/translations")
        file(MAKE_DIRECTORY ${TRANSLATIONS_OUTPUT_DIR})
        
        foreach(QM_FILE ${QM_FILES})
            get_filename_component(QM_FILENAME ${QM_FILE} NAME)
            add_custom_command(TARGET translations POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${QM_FILE}"
                    "${TRANSLATIONS_OUTPUT_DIR}/${QM_FILENAME}"
                COMMENT "复制翻译文件: ${QM_FILENAME}"
            )
        endforeach()
        
        # macOS: 在应用构建完成后，将翻译文件复制到 .app/Contents/Resources
        if(APPLE AND BUILD_EXECUTABLE)
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory 
                    "$<TARGET_BUNDLE_CONTENT_DIR:${TARGET_NAME}>/Resources/translations"
                COMMENT "创建 macOS app Resources/translations 目录"
            )
            foreach(QM_FILE ${QM_FILES})
                get_filename_component(QM_FILENAME ${QM_FILE} NAME)
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${QM_FILE}"
                        "$<TARGET_BUNDLE_CONTENT_DIR:${TARGET_NAME}>/Resources/translations/${QM_FILENAME}"
                    COMMENT "复制翻译文件到 macOS app: ${QM_FILENAME}"
                )
            endforeach()
        endif()
        
        install(FILES ${QM_FILES} DESTINATION translations OPTIONAL)
        
        message(STATUS "✓ 自动化翻译配置完成")
        message(STATUS "  - .ts 文件将在构建时自动创建/更新")
        message(STATUS "  - .qm 文件将复制到: ${TRANSLATIONS_OUTPUT_DIR}")
    endif()
endif()

# ============================================================================
# OpenSSL配置（RSA许可证验证）
# ============================================================================
if(ENABLE_LICENSE_SYSTEM)
    message(STATUS "许可证验证系统已启用，正在查找OpenSSL...")
    
    # Windows用户注意：
    # 1. 如果使用vcpkg: vcpkg install openssl:x64-windows
    # 2. 如果手动安装: 从 https://slproweb.com/products/Win32OpenSSL.html 下载
    #    并设置 OPENSSL_ROOT_DIR 环境变量指向安装目录
    # 3. Qt 6.5+ 通常自带 OpenSSL，可以尝试使用 Qt 的 OpenSSL 路径
    
    # Windows: 尝试常见的 OpenSSL 安装路径
    if(WIN32)
        set(OPENSSL_HINTS
            "C:/Program Files/OpenSSL-Win64"
            "C:/Program Files/OpenSSL"
            "C:/OpenSSL-Win64"
            "C:/OpenSSL"
            "$ENV{OPENSSL_ROOT_DIR}"
            "${QT_DIR}/../../../Tools/OpenSSL/Win_x64"  # Qt 自带的 OpenSSL
        )
        
        # 设置搜索路径
        foreach(hint ${OPENSSL_HINTS})
            if(EXISTS "${hint}")
                list(APPEND CMAKE_PREFIX_PATH "${hint}")
                set(OPENSSL_ROOT_DIR "${hint}" CACHE PATH "OpenSSL root directory" FORCE)
                message(STATUS "  尝试 OpenSSL 路径: ${hint}")
                break()
            endif()
        endforeach()
    endif()
    
    # 查找 OpenSSL 包
    find_package(OpenSSL REQUIRED)
    
    if(OpenSSL_FOUND)
        message(STATUS "找到OpenSSL: ${OPENSSL_VERSION}")
        message(STATUS "  OpenSSL Include: ${OPENSSL_INCLUDE_DIR}")
        message(STATUS "  OpenSSL Libraries: ${OPENSSL_LIBRARIES}")
        
        # 定义宏以在代码中启用许可证功能
        target_compile_definitions(${TARGET_NAME} PRIVATE ENABLE_LICENSE_SYSTEM)
        
        # 链接 OpenSSL 库到目标
        target_link_libraries(${TARGET_NAME} PRIVATE 
            OpenSSL::SSL 
            OpenSSL::Crypto
        )
        
        # 添加 OpenSSL 头文件目录（通常不需要，find_package 会自动处理）
        target_include_directories(${TARGET_NAME} PRIVATE ${OPENSSL_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "未找到OpenSSL！请安装OpenSSL并设置OPENSSL_ROOT_DIR环境变量")
    endif()
else()
    message(STATUS "许可证验证系统已禁用 (设置 -DENABLE_LICENSE_SYSTEM=ON 启用)")
endif()

# ============================================================================
# 第三方库链接（FetchContent/vcpkg/conan下载的库）
# ============================================================================
# 如果使用了FetchContent、vcpkg或conan，在这里链接下载的库
# 示例：
# if(TARGET spdlog)
#     target_link_libraries(${TARGET_NAME} PRIVATE spdlog::spdlog)
# endif()
# if(TARGET fmt)
#     target_link_libraries(${TARGET_NAME} PRIVATE fmt::fmt)
# endif()
# if(TARGET nlohmann_json)
#     target_link_libraries(${TARGET_NAME} PRIVATE nlohmann_json::nlohmann_json)
# endif()

# 使用find_package查找已安装的库（适用于vcpkg/系统库）
# 示例：
# find_package(Boost COMPONENTS system filesystem QUIET)
# if(Boost_FOUND)
#     target_link_libraries(${TARGET_NAME} PRIVATE Boost::system Boost::filesystem)
# endif()

# ============================================================================
# 平台特定链接库（可选，通常Qt项目不需要）
# ============================================================================
if(ENABLE_PLATFORM_LIBS)
    if(WIN32)
        # Windows平台特定库（用于Win32 API开发）
        # Qt项目通常不需要这些库，因为Qt已经封装了这些功能
        target_link_libraries(${TARGET_NAME} PRIVATE 
            user32      # 用户界面API（窗口、消息、输入等）
            gdi32       # 图形设备接口（绘图、字体等）
            shell32     # Shell功能（文件操作、快捷方式等）
            comctl32    # 通用控件库
            comdlg32    # 通用对话框库
        )
        message(STATUS "已链接Windows平台库")
    elseif(UNIX AND NOT APPLE)
        # Linux平台特定库
        target_link_libraries(${TARGET_NAME} PRIVATE 
            pthread     # POSIX线程库（使用std::thread时需要）
            dl          # 动态链接库（使用dlopen等时需要）
            rt          # 实时扩展库（某些时钟函数需要）
        )
        
        # X11支持（用于原生窗口系统，Qt程序不需要）
        find_package(X11)
        if(X11_FOUND)
            target_link_libraries(${TARGET_NAME} PRIVATE ${X11_LIBRARIES})
            message(STATUS "已链接X11库")
        endif()
        message(STATUS "已链接Linux平台库")
    elseif(APPLE)
        # macOS平台特定框架（用于原生macOS开发，Qt程序不需要）
        target_link_libraries(${TARGET_NAME} PRIVATE 
            "-framework Cocoa"      # macOS图形界面和应用程序框架
            "-framework AppKit"     # macOS应用程序工具包
            "-framework Foundation"  # 基础框架
        )
        message(STATUS "已链接macOS平台框架")
    endif()
else()
    # 即使不启用平台特定库，某些基础库仍然可能需要
    if(UNIX AND NOT APPLE)
        # Linux下的多线程支持（C++11 std::thread需要）
        find_package(Threads)
        if(Threads_FOUND)
            target_link_libraries(${TARGET_NAME} PRIVATE Threads::Threads)
        endif()
    endif()
    message(STATUS "平台特定库已禁用（仅链接必需的基础库）")
endif()

# ============================================================================
# 安装配置（可以暂时不管）
# ============================================================================
if(ENABLE_INSTALLER)
    if(BUILD_EXECUTABLE)
        install(TARGETS ${TARGET_NAME} 
            RUNTIME DESTINATION bin
            BUNDLE DESTINATION .
        )
    else()
        install(TARGETS ${TARGET_NAME}
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            RUNTIME DESTINATION bin
        )
        # 安装头文件
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ 
            DESTINATION include
            FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
        )
    endif()
    
    # CPack配置（用于创建安装包）
    set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_VENDOR "YourCompany")
    
    if(WIN32)
        set(CPACK_GENERATOR "NSIS;ZIP")
    elseif(APPLE)
        set(CPACK_GENERATOR "DragNDrop;TGZ")
    else()
        set(CPACK_GENERATOR "DEB;RPM;TGZ")
    endif()
    
    include(CPack)
    message(STATUS "安装程序打包已启用")
endif()

# ============================================================================
# 测试配置
# ============================================================================
if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "单元测试已启用")
endif()

# ============================================================================
# 输出配置信息
# ============================================================================
message(STATUS "==================================================")
message(STATUS "项目配置信息:")
message(STATUS "  项目名称: ${PROJECT_NAME}")
message(STATUS "  版本: ${PROJECT_VERSION}")
message(STATUS "  构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++标准: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  源码编码: ${USE_GBK_ENCODING}")
if(USE_GBK_ENCODING)
    message(STATUS "    -> 使用GBK编码")
else()
    message(STATUS "    -> 使用UTF-8编码")
endif()
message(STATUS "  Qt支持: ${ENABLE_QT}")
if(ENABLE_QT)
    message(STATUS "    -> Qt版本: Qt${QT_VERSION_MAJOR}")
    message(STATUS "    -> Qt翻译: ${ENABLE_QT_TRANSLATION}")
endif()
message(STATUS "  MFC支持: ${ENABLE_MFC}")
if(BUILD_EXECUTABLE)
    message(STATUS "  构建目标: 可执行文件")
else()
    if(BUILD_SHARED_LIBS)
        message(STATUS "  构建目标: 动态库")
    else()
        message(STATUS "  构建目标: 静态库")
    endif()
endif()
message(STATUS "  平台特定库: ${ENABLE_PLATFORM_LIBS}")
message(STATUS "  输出目录: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "")
message(STATUS "第三方依赖管理:")
message(STATUS "  FetchContent: ${ENABLE_FETCHCONTENT}")
message(STATUS "  vcpkg: ${ENABLE_VCPKG}")
message(STATUS "  Conan: ${ENABLE_CONAN}")
message(STATUS "")
message(STATUS "代码质量工具:")
message(STATUS "  Clang-Tidy: ${ENABLE_CLANG_TIDY}")
message(STATUS "  Clang-Format: ${ENABLE_CLANG_FORMAT}")
message(STATUS "  CppCheck: ${ENABLE_CPPCHECK}")
message(STATUS "==================================================")