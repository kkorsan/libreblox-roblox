include_directories(${CMAKE_SOURCE_DIR}/engine/base/include)

if(ANDROID)
  include_directories(${CMAKE_SOURCE_DIR}/engine/base/include/rbx/Android)
endif(ANDROID)
