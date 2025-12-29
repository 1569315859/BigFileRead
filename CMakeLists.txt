# ============================================================================
# CMakeLists.txt - 通用跨平台CMake配置文件
# 项目名称: BigFileRead
# 说明: 支持Ninja构建，桌面端自动引入Qt6库，非桌面端则不使用
# ============================================================================

# 设置CMake最低版本要求，3.16是Qt6的最低要求版本
cmake_minimum_required(VERSION 3.16)

# 定义项目名称、版本号和使用的编程语言
project(BigFileRead 
    VERSION 1.0.0           # 项目版本号
    LANGUAGES CXX           # 使用C++语言
    DESCRIPTION "大文件读取应用程序"  # 项目描述
)

# ============================================================================
# 编译器和语言标准设置
# ============================================================================

# 设置C++标准为C++17，这是现代C++的推荐标准
set(CMAKE_CXX_STANDARD 14)

# 强制要求使用指定的C++标准，如果编译器不支持则报错
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 禁用编译器特定扩展，确保代码的跨平台兼容性
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置默认构建类型为Release（如果用户未指定）
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    # 输出提示信息，告知用户使用默认构建类型
    message(STATUS "未指定构建类型，默认使用 Release")
    # 设置构建类型为Release，CACHE STRING表示缓存该变量
    set(CMAKE_BUILD_TYPE Release CACHE STRING "选择构建类型" FORCE)
    # 设置构建类型的可选值，供CMake GUI使用
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS 
        "Debug"             # 调试版本：包含调试信息，无优化
        "Release"           # 发布版本：完全优化，无调试信息
        "RelWithDebInfo"    # 带调试信息的发布版本：优化+调试信息
        "MinSizeRel"        # 最小体积发布版本：优化代码体积
    )
endif()

# ============================================================================
# 跨平台检测 - 判断是否为桌面平台
# ============================================================================

# 定义变量用于判断是否为桌面平台（Windows、macOS、Linux桌面环境）
# 桌面平台将启用Qt GUI支持，非桌面平台（嵌入式、移动端等）则不启用
set(IS_DESKTOP_PLATFORM FALSE)

# 检测Windows平台
if(WIN32)
    set(IS_DESKTOP_PLATFORM TRUE)
    message(STATUS "检测到桌面平台: Windows")
endif()

# 检测macOS平台（APPLE包含iOS，需要排除）
if(APPLE AND NOT IOS)
    set(IS_DESKTOP_PLATFORM TRUE)
    message(STATUS "检测到桌面平台: macOS")
endif()

# 检测Linux桌面平台（排除Android）
if(UNIX AND NOT APPLE AND NOT ANDROID)
    set(IS_DESKTOP_PLATFORM TRUE)
    message(STATUS "检测到桌面平台: Linux/Unix")
endif()

# 允许用户手动覆盖桌面平台检测结果
option(FORCE_DESKTOP_PLATFORM "强制启用桌面平台模式" OFF)
if(FORCE_DESKTOP_PLATFORM)
    set(IS_DESKTOP_PLATFORM TRUE)
    message(STATUS "用户强制启用桌面平台模式")
endif()

# 允许用户手动禁用桌面平台特性
option(DISABLE_DESKTOP_PLATFORM "强制禁用桌面平台模式" OFF)
if(DISABLE_DESKTOP_PLATFORM)
    set(IS_DESKTOP_PLATFORM FALSE)
    message(STATUS "用户强制禁用桌面平台模式")
endif()

# ============================================================================
# Qt6配置（仅桌面平台）
# ============================================================================

# 仅在桌面平台上查找和配置Qt6
if(IS_DESKTOP_PLATFORM)
    # 输出状态信息
    message(STATUS "桌面平台: 启用Qt6支持")
    
    # 启用Qt的自动MOC（元对象编译器），处理Q_OBJECT宏
    set(CMAKE_AUTOMOC ON)
    
    # 启用Qt的自动UIC（UI编译器），处理.ui文件
    set(CMAKE_AUTOUIC ON)
    
    # 启用Qt的自动RCC（资源编译器），处理.qrc资源文件
    set(CMAKE_AUTORCC ON)
    
    # 查找Qt6包，REQUIRED表示找不到则报错
    # COMPONENTS指定需要的Qt模块
    find_package(Qt6 REQUIRED COMPONENTS
        Core        # Qt核心模块：提供核心非GUI功能
        Gui         # Qt GUI模块：提供GUI功能的基础类
        Widgets     # Qt Widgets模块：提供桌面风格的UI组件
    )
    
    # 输出找到的Qt版本信息
    message(STATUS "找到Qt6版本: ${Qt6_VERSION}")
    
    # 定义预处理器宏，供源代码中条件编译使用
    add_compile_definitions(USE_QT_GUI=1)
    add_compile_definitions(QT_VERSION_MAJOR=6)
else()
    # 非桌面平台，禁用Qt GUI
    message(STATUS "非桌面平台: 禁用Qt GUI支持")
    add_compile_definitions(USE_QT_GUI=0)
endif()

# ============================================================================
# 源文件配置
# ============================================================================

# 设置源文件目录
set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 设置头文件目录
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 使用GLOB_RECURSE递归搜索所有源文件
# 注意：GLOB在添加新文件后需要重新运行CMake
file(GLOB_RECURSE SOURCES
    "${SOURCE_DIR}/*.cpp"       # 所有C++源文件
    "${SOURCE_DIR}/*.c"         # 所有C源文件（如果有）
)

# 搜索所有头文件
file(GLOB_RECURSE HEADERS
    "${SOURCE_DIR}/*.h"         # 源目录下的头文件
    "${SOURCE_DIR}/*.hpp"       # 源目录下的HPP头文件
    "${INCLUDE_DIR}/*.h"        # 包含目录下的头文件
    "${INCLUDE_DIR}/*.hpp"      # 包含目录下的HPP头文件
)

# 仅桌面平台：搜索Qt相关文件
if(IS_DESKTOP_PLATFORM)
    # 搜索Qt UI文件
    file(GLOB_RECURSE UI_FILES
        "${SOURCE_DIR}/*.ui"    # UI设计文件
    )
    
    # 搜索Qt资源文件
    file(GLOB_RECURSE QRC_FILES
        "${SOURCE_DIR}/*.qrc"   # Qt资源文件
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/*.qrc"  # 资源目录下的qrc文件
    )
endif()

# ============================================================================
# 构建目标配置
# ============================================================================

# 根据平台类型创建不同类型的可执行文件
if(IS_DESKTOP_PLATFORM)
    # 桌面平台：创建GUI可执行文件
    # WIN32: Windows下创建GUI程序（不显示控制台窗口）
    # MACOSX_BUNDLE: macOS下创建应用程序包
    add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE
        ${SOURCES}          # C++源文件
        ${HEADERS}          # 头文件（用于IDE显示）
        ${UI_FILES}         # Qt UI文件
        ${QRC_FILES}        # Qt资源文件
    )
    
    # 链接Qt6库
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Core           # 链接Qt Core模块
        Qt6::Gui            # 链接Qt Gui模块
        Qt6::Widgets        # 链接Qt Widgets模块
    )
else()
    # 非桌面平台：创建控制台可执行文件
    add_executable(${PROJECT_NAME}
        ${SOURCES}          # C++源文件
        ${HEADERS}          # 头文件
    )
endif()

# 添加头文件搜索路径
target_include_directories(${PROJECT_NAME} PRIVATE
    ${SOURCE_DIR}           # 源文件目录
    ${INCLUDE_DIR}          # 头文件目录
    ${CMAKE_CURRENT_BINARY_DIR}  # 构建目录（包含生成的moc文件等）
)

# ============================================================================
# 编译选项配置（跨平台）
# ============================================================================

# 根据不同编译器设置警告和优化选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC和Clang编译器选项
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall               # 启用所有常见警告
        -Wextra             # 启用额外警告
        -Wpedantic          # 启用严格ISO C++合规性警告
        # 根据构建类型添加特定选项
        $<$<CONFIG:Debug>:-g3>              # Debug: 最大调试信息
        $<$<CONFIG:Debug>:-O0>              # Debug: 无优化
        $<$<CONFIG:Release>:-O3>            # Release: 最大优化
        $<$<CONFIG:RelWithDebInfo>:-O2>     # RelWithDebInfo: 中等优化
        $<$<CONFIG:RelWithDebInfo>:-g>      # RelWithDebInfo: 调试信息
        $<$<CONFIG:MinSizeRel>:-Os>         # MinSizeRel: 优化代码体积
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # MSVC编译器选项
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4                 # 启用4级警告（较高警告级别）
        /utf-8              # 使用UTF-8编码
        /permissive-        # 启用严格标准合规模式
        # 根据构建类型添加特定选项
        $<$<CONFIG:Debug>:/Zi>              # Debug: 完整调试信息
        $<$<CONFIG:Debug>:/Od>              # Debug: 禁用优化
        $<$<CONFIG:Release>:/O2>            # Release: 最大速度优化
        $<$<CONFIG:RelWithDebInfo>:/O2>     # RelWithDebInfo: 优化
        $<$<CONFIG:RelWithDebInfo>:/Zi>     # RelWithDebInfo: 调试信息
        $<$<CONFIG:MinSizeRel>:/O1>         # MinSizeRel: 优化代码体积
    )
endif()

# ============================================================================
# 安装配置
# ============================================================================

# 设置安装目录（可被用户覆盖）
# GNUInstallDirs提供跨平台的标准安装目录变量
include(GNUInstallDirs)

# 安装可执行文件到bin目录
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}     # 可执行文件安装位置
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}      # macOS Bundle安装位置
)

# ============================================================================
# 输出配置信息摘要
# ============================================================================

message(STATUS "========================================")
message(STATUS "项目配置摘要:")
message(STATUS "  项目名称: ${PROJECT_NAME}")
message(STATUS "  项目版本: ${PROJECT_VERSION}")
message(STATUS "  构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++标准: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  桌面平台: ${IS_DESKTOP_PLATFORM}")
if(IS_DESKTOP_PLATFORM)
    message(STATUS "  Qt版本: ${Qt6_VERSION}")
endif()
message(STATUS "  安装前缀: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "========================================")
