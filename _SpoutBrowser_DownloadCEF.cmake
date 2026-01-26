# This script is called by _SpoutBrowser_generate_solution.bat
#   to download and/or extract the CEF binary distribution.
# This script is based on https://github.com/chromiumembedded/cef-project/blob/master/cmake/DownloadCEF.cmake
# Following variables expected (and set by the calling .bat):
#   CEF_DISTRIBUTION    e.g. "cef_binary_139.0.28+g55ab8a8+chromium-139.0.7258.139_windows64"
#   CEF_DOWNLOAD_DIR    target directory, e.g. SpoutBrowser/_cef_binary

# test
#set(CEF_DISTRIBUTION "cef_binary_139.0.28+g55ab8a8+chromium-139.0.7258.139_windows64")
#set(CEF_DOWNLOAD_DIR "./_cef_binary")

if(NOT IS_DIRECTORY "${CEF_DOWNLOAD_DIR}/${CEF_DISTRIBUTION}")
  set(CEF_DOWNLOAD_FILENAME "${CEF_DISTRIBUTION}.tar.bz2")
  set(CEF_DOWNLOAD_PATH "${CEF_DOWNLOAD_DIR}/${CEF_DOWNLOAD_FILENAME}")
  if(NOT EXISTS "${CEF_DOWNLOAD_PATH}")
    set(CEF_DOWNLOAD_URL "https://cef-builds.spotifycdn.com/${CEF_DOWNLOAD_FILENAME}")
    string(REPLACE "+" "%2B" CEF_DOWNLOAD_URL_ESCAPED ${CEF_DOWNLOAD_URL})

    # Download the binary distribution.
    message(STATUS "Downloading ${CEF_DOWNLOAD_PATH}...")
    file(DOWNLOAD "${CEF_DOWNLOAD_URL_ESCAPED}" "${CEF_DOWNLOAD_PATH}"
        STATUS status)

    list(GET status 0 status_code)
    list(GET status 1 status_string)
    if(NOT status_code EQUAL 0)
      file(REMOVE "${CEF_DOWNLOAD_PATH}")  # Clean up the partial/failed file
      message(FATAL_ERROR "Failed to download CEF binary. Status ${status_code}: ${status_string}")
    endif()
  endif()

  # Extract the binary distribution.
  message(STATUS "Extracting ${CEF_DOWNLOAD_PATH}...")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xzf "${CEF_DOWNLOAD_FILENAME}"
    WORKING_DIRECTORY ${CEF_DOWNLOAD_DIR}
  )
endif()
