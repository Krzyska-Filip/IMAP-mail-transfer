cmake_minimum_required(VERSION 3.28)
project(imap_mail_transfer C)

set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

add_executable(imap_mail_transfer main.c)

target_link_libraries(imap_mail_transfer PRIVATE CURL::libcurl)
target_include_directories(imap_mail_transfer PRIVATE ${CURL_INCLUDE_DIRS})
target_link_libraries(imap_mail_transfer PRIVATE ${CURL_LIBRARIES})