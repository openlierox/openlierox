find_package(SDL2)

set(player_SRCS Audio_Scope.cpp
                Audio_Scope.h
                Music_Player.cpp
                Music_Player.h
                player.cpp)

if(SDL2_FOUND)
    message(STATUS "SDL2 library located, player demo is available to be built in the /player directory")

    add_executable(gme_player ${player_SRCS})
    target_include_directories(gme_player PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/gme
    )
    set_property(TARGET gme_player PROPERTY CXX_STANDARD_REQUIRED ON)
    set_property(TARGET gme_player PROPERTY CXX_STANDARD 11)
    target_link_libraries(gme_player PRIVATE ${SDL2_LIBRARIES} gme::gme)
    # Is not to be installed though
else()
    message(STATUS "** SDL library not found, disabling player demo build")
endif()
