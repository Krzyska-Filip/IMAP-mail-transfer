cmake_minimum_required(VERSION 3.28)
project(imap_mail_transfer C)

set(CMAKE_C_STANDARD 11)

find_package(CURL REQUIRED)
find_package(Curses REQUIRED)

add_executable(imap_mail_transfer main.c)

target_link_libraries(imap_mail_transfer PRIVATE CURL::libcurl ${CURSES_LIBRARIES})
target_include_directories(imap_mail_transfer PRIVATE ${CURSES_INCLUDE_DIRS})