cmake_minimum_required(VERSION 2.8)
project( vision_server )

#list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/curl)
find_package( OpenCV REQUIRED )
#find_package( CURL REQUIRED )

set(CMAKE_CXX_FLAGS "-std=c++1y -Wformat=2 -Wall -Wextra -Werror -pedantic -g " CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 " CACHE STRING "" FORCE)

set(SOURCE_FILES

  src/vision_server.cpp
  src/Pipeline.cpp
  src/server.cpp
  src/mongoose/mongoose.cpp
  src/jsoncpp/jsoncpp.cpp
)

include_directories("${CMAKE_SOURCE_DIR}/src/mongoose" 
  "${CMAKE_SOURCE_DIR}/src/jsoncpp" 
  #${CURL_INCLUDE_DIR} 
  )

add_executable( vision_server ${SOURCE_FILES} )
target_link_libraries( vision_server ${OpenCV_LIBS} 
  #${CURL_LIBRARIES} 
  pthread )
