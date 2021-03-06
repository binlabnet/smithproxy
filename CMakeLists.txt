cmake_minimum_required(VERSION 2.8)

SET(SOCLE_DIR "socle")
SET(SOCLE_COMMON_DIR "socle/common")
SET(SMITHD_DIR "src/service/smithd")

project(smithproxy CXX)

SET(CMAKE_SOURCE_DIR "src/")
SET(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")

include_directories ("${SOCLE_DIR}")
include_directories ("${SOCLE_COMMON_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/src/")
include_directories ("${PROJECT_SOURCE_DIR}/src/ext")
include_directories ("${SMITHD_DIR}")

add_subdirectory(${SOCLE_DIR} socle_lib)
add_subdirectory(${SOCLE_COMMON_DIR} socle_common_lib)

if(UNIX)
    IF(NOT CMAKE_BUILD_TYPE)
        SET(CMAKE_BUILD_TYPE Debug)
    ENDIF(NOT CMAKE_BUILD_TYPE)

    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-psabi -std=c++17 -g -O3 ")
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O3")
    SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -s -DBUILD_RELEASE")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
    SET(CMAKE_AR  "gcc-ar")
    SET(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
    SET(CMAKE_CXX_ARCHIVE_FINISH   true)

    # detect Alpine - and disable backtrace_* function use
    if(EXISTS "/etc/alpine-release")
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLIBC_MUSL")
    endif()
endif()


if(EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(
            COMMAND git rev-parse --abbrev-ref HEAD
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE SX_GIT_BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
            COMMAND git log -1 --format=%h
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE SX_GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
            COMMAND git describe --tags --dirty
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE SX_GIT_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

else(EXISTS "${PROJECT_SOURCE_DIR}/.git")
    set(SX_GIT_BRANCH "")
    set(SX_GIT_COMMIT_HASH "")
    set(SX_GIT_VERSION "")

endif(EXISTS "${PROJECT_SOURCE_DIR}/.git")

message(STATUS "Git current smithproxy branch: ${SX_GIT_BRANCH}")
message(STATUS "Git commit smithrproxy hash: ${SX_GIT_COMMIT_HASH}")
message(STATUS "Git commit smithrproxy version: ${SX_GIT_VERSION}")

message(STATUS "Generating smithproxy_version.h")

configure_file(
        ${CMAKE_SOURCE_DIR}/smithproxy_version.h.in
        ${PROJECT_SOURCE_DIR}/src/smithproxy_version.h
)

add_executable(smithproxy
        src/main.cpp
        src/proxy/mitmhost.cpp
        src/proxy/mitmproxy.cpp
        src/cfgapi.cpp
        src/policy/policy.cpp
        src/service/daemon.cpp
        src/proxy/socks5/sockshostcx.cpp
        src/proxy/socks5/socksproxy.cpp
        src/cli/cmdserver.cpp
        src/shm/shmauth.cpp
        src/inspect/dns.cpp
        src/policy/inspectors.cpp
        src/policy/addrobj.cpp
        src/service/srvutils.cpp
        src/staticcontent.cpp
        src/policy/authfactory6.cpp
        src/smithlog.cpp
        src/proxy/filterproxy.cpp
        src/service/dnsupd/smithdnsupd.cpp
        src/policy/loadb.cpp
        src/service/smithproxy.hpp
        src/service/smithproxy.cpp

        src/ext/libcidr/cidr.cpp
        src/ext/cxxopts/cxxopts.hpp
        src/ext/nltemplate/nltemplate.cpp

        src/policy/authfactory.hpp
        src/policy/authfactory4.cpp
        src/inspect/sxsignature.hpp
        src/inspect/pyinspector.hpp
        src/inspect/sigfactory.hpp
        src/async/asyncsocket.hpp
        src/async/asyncdns.hpp
        src/inspect/dnsinspector.hpp
        src/inspect/dnsinspector.cpp
        src/cli/clihelp.hpp
        src/cli/clihelp.cpp src/cli/cligen.hpp src/cli/cligen.cpp)

add_executable(smithd
        ${SMITHD_DIR}/smithd.cpp
        ${SMITHD_DIR}/smithdcx.cpp

        src/service/daemon.cpp
        src/smithlog.cpp
        src/inspect/dns.cpp src/ext/libcidr/cidr.cpp
        src/policy/addrobj.cpp src/async/asyncocsp.hpp)

add_executable(smithdc
        ${SMITHD_DIR}/smithdc.cpp
        ${SMITHD_DIR}/smithdcx.cpp
        src/smithlog.cpp)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}")

find_package (LibConfig REQUIRED)
if (LIBCONFIGPP_FOUND)
  include_directories(${LIBCONFIGPP_INCLUDE_DIRS})
  target_link_libraries (smithproxy ${LIBCONFIGPP_LIBRARIES})
  target_link_libraries (smithd ${LIBCONFIGPP_LIBRARIES})
endif (LIBCONFIGPP_FOUND)

find_package (LibCli REQUIRED)
if (LIBCLI_FOUND)
  include_directories(${LIBCLI_INCLUDE_DIR})
  target_link_libraries (smithproxy ${LIBCLI_LIBRARY})
  target_link_libraries (smithd ${LIBCLI_LIBRARY})
endif (LIBCLI_FOUND)

find_package(PythonLibs 3 REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})


target_link_libraries(smithproxy socle_lib pthread ssl crypto rt unwind ${PYTHON_LIBRARIES})
target_link_libraries(smithd socle_lib pthread ssl crypto rt unwind)
target_link_libraries(smithdc socle_lib pthread ssl crypto rt unwind)

# taken from http://public.kitware.com/Bug/view.php?id=12646
function(install_if_not_exists src dest)
  if(NOT IS_ABSOLUTE "${src}")
    set(src "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
  endif()
  get_filename_component(src_name "${src}" NAME)
  if (NOT IS_ABSOLUTE "${dest}")
    set(dest "${CMAKE_INSTALL_PREFIX}/${dest}")
  endif()
  install(CODE "
    if(NOT EXISTS \"\$ENV{DESTDIR}${dest}/${src_name}\")
      #file(INSTALL \"${src}\" DESTINATION \"${dest}\")
      message(STATUS \"Installing: \$ENV{DESTDIR}${dest}/${src_name}\")
      execute_process(COMMAND \${CMAKE_COMMAND} -E copy \"${src}\"
                      \"\$ENV{DESTDIR}${dest}/${src_name}\"
                      RESULT_VARIABLE copy_result
                      ERROR_VARIABLE error_output)
      if(copy_result)
        message(FATAL_ERROR \${error_output})
      endif()
    else()
      message(STATUS \"Skipping  : \$ENV{DESTDIR}${dest}/${src_name}\")
    endif()
  ")
endfunction(install_if_not_exists)


macro(InstallSymlink _filepath _sympath)
    get_filename_component(_symname ${_sympath} NAME)
    get_filename_component(_installdir ${_sympath} PATH)

    if (BINARY_PACKAGING_MODE)
        execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink
                        ${_filepath}
                        ${CMAKE_CURRENT_BINARY_DIR}/${_symname})
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${_symname}
                DESTINATION ${_installdir})
    else ()
        # scripting the symlink installation at install time should work
        # for CMake 2.6.x and 2.8.x
        install(CODE "
            if (\"\$ENV{DESTDIR}\" STREQUAL \"\")
                execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
                                ${_filepath}
                                ${_installdir}/${_symname})
            else ()
                execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
                                ${_filepath}
                                \$ENV{DESTDIR}/${_installdir}/${_symname})
            endif ()
        ")
    endif ()
endmacro(InstallSymlink)

if(UNIX)
    SET(CMAKE_INSTALL_PREFIX /usr)
    install(TARGETS smithproxy DESTINATION bin)
    install(TARGETS smithd DESTINATION bin)
    install(FILES man/smithproxy.1 DESTINATION share/man/man1)
    install_if_not_exists(etc/smithproxy.cfg /etc/smithproxy)
    install_if_not_exists(etc/users.cfg /etc/smithproxy)
    install_if_not_exists(etc/users.key /etc/smithproxy)
    install_if_not_exists(etc/smithproxy.startup.cfg /etc/smithproxy)
    install_if_not_exists(etc/smithd.cfg /etc/smithproxy)
    install(FILES etc/smithproxy.startup.sh DESTINATION /etc/smithproxy 
                PERMISSIONS 
                    OWNER_READ OWNER_WRITE OWNER_EXECUTE 
                    GROUP_READ GROUP_EXECUTE 
                    WORLD_READ WORLD_EXECUTE 
    )
    install(FILES etc/smithproxy.init DESTINATION /etc/init.d RENAME smithproxy 
            PERMISSIONS 
                    OWNER_READ OWNER_WRITE OWNER_EXECUTE 
                    GROUP_READ GROUP_EXECUTE 
                    WORLD_READ WORLD_EXECUTE 
    )
    install(FILES etc/smithproxy_cli DESTINATION bin 
            PERMISSIONS 
                    OWNER_READ OWNER_WRITE OWNER_EXECUTE 
                    GROUP_READ GROUP_EXECUTE 
                    WORLD_READ WORLD_EXECUTE 
    )
    
    file(GLOB def_certs "etc/certs/default/*.pem")
    install(FILES ${def_certs} DESTINATION /etc/smithproxy/certs/default)
    
    # create CA dir
    install(DIRECTORY DESTINATION /etc/smithproxy/certs/ca)
    InstallSymlink(/etc/ssl/certs /etc/smithproxy/certs/ca/default)
    
    install(FILES man/TESTING_README.txt DESTINATION share/smithproxy/docs)
    
    # backend install in /usr/share/smithproxy/infra/bend/
    install(DIRECTORY src/infra/bend DESTINATION share/smithproxy/infra)
    # install infra/
    file(GLOB infra_py "src/infra/*.py" EXCLUDE "src/infra/smithdog.py")
    install(FILES ${infra_py} DESTINATION share/smithproxy/infra)
    
    file(GLOB infra_exe_py "src/infra/smithdog.py")
    install(FILES ${infra_exe_py} DESTINATION share/smithproxy/infra 
            PERMISSIONS 
                    OWNER_READ OWNER_WRITE OWNER_EXECUTE 
                    GROUP_READ GROUP_EXECUTE 
                    WORLD_READ WORLD_EXECUTE 
    )

    install(DIRECTORY src/infra/sslca DESTINATION share/smithproxy/infra)

    file(GLOB sslca_makecerts "src/infra/sslca/makecerts.py")
    install(FILES ${sslca_makecerts} DESTINATION share/smithproxy/infra/sslca
            PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
            )


    install(DIRECTORY src/infra/authtools DESTINATION share/smithproxy/infra)
    file(GLOB sx_auth_tools "src/infra/authtools/sx_*.py")
    install(FILES ${sx_auth_tools} DESTINATION share/smithproxy/infra/authtools
            PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
            )

    InstallSymlink(/usr/share/smithproxy/infra/sslca/makecerts.py /usr/bin/sx_regencerts)
    InstallSymlink(/usr/share/smithproxy/infra/authtools/sx_passwd.py /usr/bin/sx_passwd)

    # portal installation
    file(GLOB sx_www_cgi "src/infra/portal/cgi-bin/auth*.py")
    install(FILES ${sx_www_cgi} DESTINATION share/smithproxy/www/portal/cgi-bin
            PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
            )
    file(GLOB sx_www_cgi_nox "src/infra/portal/cgi-bin/util.py")
    install(FILES ${sx_www_cgi_nox} DESTINATION share/smithproxy/www/portal/cgi-bin)

    file(GLOB sx_www "src/infra/portal/*.*")
    # install(DIRECTORY infra/portal DESTINATION share/smithproxy/www)
    install(FILES ${sx_www} DESTINATION share/smithproxy/www/portal/)


    install(DIRECTORY etc/msg DESTINATION /etc/smithproxy)
    
    # message: edit defaults and add to init.d to start at boot!
    install(CODE "MESSAGE(\" +----------------------------------------------------------------------------------------+\")")
    install(CODE "MESSAGE(\" | Installation complete!                                                                 |\")")
    install(CODE "MESSAGE(\" +----------------------------------------------------------------------------------------|\")")
    install(CODE "MESSAGE(\" |   Hints for minimal setup:                                                             |\")")
    install(CODE "MESSAGE(\" |     1:Edit /etc/smithproxy/smithproxy.startup.cfg                                      |\")")
    install(CODE "MESSAGE(\" |       -  change interface heading to the LAN/internal network you want to inspect.     |\")")
    install(CODE "MESSAGE(\" |     2:Make smithproxy start on boot                                                    |\")")
    install(CODE "MESSAGE(\" |       -  Debian:  update-rc.d smithproxy defaults                                      |\")")
    install(CODE "MESSAGE(\" |     3:Enable smithproxy CLI                                                            |\")")
    install(CODE "MESSAGE(\" |       -  add /usr/bin/smithproxy_cli to /etc/shells, make special user for it, use ssh |\")")
    install(CODE "MESSAGE(\" +----------------------------------------------------------------------------------------+\")")
    
    SET(CPACK_GENERATOR "DEB")
    SET(MAJOR_VERSION 0)
    SET(MINOR_VERSION 8)
    SET(PATCH_VERSION 3)
    SET(CPACK_PACKAGE_VERSION_MAJOR "${MAJOR_VERSION}")
    SET(CPACK_PACKAGE_VERSION_MINOR "${MINOR_VERSION}")
    SET(CPACK_PACKAGE_VERSION_PATCH "${PATCH_VERSION}")
    
    SET(CPACK_PACKAGE_DESCRIPTION "transparent proxy with ssl inspection features")
    SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "tranparent proxy based on TPROXY with SSL inspection, authentication and other features")
    SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ales Stibal <astib@mag0.net>")
    SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libstdc++6 (>= 4.9.0), libssl1.0.0 (>=1.0.0), libconfig++9v5, libcli1.9 (>=1.9.6), iptables ,python-ldap, python-pylibconfig2, python-pyparsing, python-posix-ipc, python-soappy, python-m2crypto")
    INCLUDE(CPack)

endif()

