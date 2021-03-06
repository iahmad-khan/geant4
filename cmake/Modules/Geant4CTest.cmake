# - Basic setup for testing Geant4 using CMake/CTest
#

#-----------------------------------------------------------------------
# Configure CTest and relevant Geant4 settings, if required
#
if(GEANT4_ENABLE_TESTING)
  # - Core CTest
  enable_testing()
  include(CTest)

  # - Geant4_DIR is needed to locate GeantConfig.cmake file required
  # by tests and examples
  set(Geant4_DIR ${CMAKE_BINARY_DIR} CACHE PATH "Current build directory")

  # - Add datasets to testing environment
  geant4_get_datasetnames(_dslist)
  list(REMOVE_ITEM _dslist "G4ENSDFSTATE")
  foreach(_ds ${_dslist})
    geant4_get_dataset_property(${_ds} ENVVAR _dsenvvar)
    geant4_get_dataset_property(${_ds} BUILD_DIR _dspath)
    list(APPEND GEANT4_TEST_ENVIRONMENT ${_dsenvvar}=${_dspath})
  endforeach()
endif()

#-----------------------------------------------------------------------
# Add Unit Tests if required
#
if(GEANT4_BUILD_TESTS)
  file(GLOB_RECURSE files RELATIVE ${CMAKE_SOURCE_DIR} source/CMakeLists.txt)
  foreach( file ${files} )
    get_filename_component(path ${file} PATH)
    if(path MATCHES "/test$")
      add_subdirectory(${path})
    endif()
  endforeach()
endif()
