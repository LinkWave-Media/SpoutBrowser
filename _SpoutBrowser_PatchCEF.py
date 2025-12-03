# This script is called by _SpoutBrowser_generate_solution.bat
#   to apply SpoutBrowser modifications to the CEF binary directory sources.
# This script is based on https://github.com/chromiumembedded/cef/blob/master/tools/make_distrib.py

import os, sys

#---------------------------------------------------------------------
# Get paths from script arguments
if __name__ != '__main__' or len(sys.argv) != 3:
    sys.exit(1)

source_dir = sys.argv[1]  # SpoutBrowser (cef forked) working copy root
cef_root   = sys.argv[2]  # Downloaded CEF binary root directory

print("Configuring SpoutBrowser solution\n  SpoutBrowser root: %s\n  CEF binary: %s" % (source_dir, cef_root))


#---------------------------------------------------------------------
# Import needed utils from cef python tools directory
sys.path.append(os.path.join(source_dir, r"tools"))
from file_util import eval_file, copy_file
from make_cmake import process_cmake_template


#---------------------------------------------------------------------
# Read the variables from cef_paths2.gypi
cef_paths2 = eval_file(os.path.join(source_dir, 'cef_paths2.gypi'))
cef_paths2 = cef_paths2['variables']


# We modify file lists manually (to leave cef_paths2.gypi unchanged):
#  remove unused "views" source files
cef_paths2['cefclient_sources_browser'] = [
    subpath
    for subpath in cef_paths2['cefclient_sources_browser'] if not (
        '_views.' in subpath or
        '/views_' in subpath
    )
]
#  replace the Resource file
cef_paths2['cefclient_sources_resources_win_rc'].remove(
    'tests/cefclient/win/cefclient.rc'
)
cef_paths2['cefclient_sources_resources_win_rc'] += [
    'tests/cefclient/spout_browser/SpoutBrowser_cefclient.rc',
    'tests/cefclient/spout_browser/SpoutBrowser.ico',
]
#  add Spout TextureSender sources
cef_paths2['cefclient_sources_win'] += [
    'tests/cefclient/spout_browser/SpoutBrowser_TextureSender.cc',
    'tests/cefclient/spout_browser/SpoutBrowser_TextureSender.h',
]


#---------------------------------------------------------------------
# Now patch the CEF binary directory sources with our modifications

# Transfer source files
file_sections = (
    'shared_sources_browser',
    'shared_sources_common',
    'shared_sources_renderer',
    'shared_sources_win',
    'cefclient_sources_browser',
    'cefclient_sources_common',
    'cefclient_sources_renderer',
    'cefclient_sources_win',
    'cefclient_sources_resources_win',
    'cefclient_sources_resources_win_rc',
)
os.makedirs(
    os.path.join(cef_root, 'tests/cefclient/spout_browser'), 
    exist_ok=True
)
for section in file_sections:
    print("=== Transfer: %s" % (section,))
    for subpath in cef_paths2[section]:
        copy_file(
            os.path.join(source_dir, subpath),
            os.path.join(cef_root,   subpath),
            False
        )

# Configure CMake scripts ('.in')
cmake_script_paths = (
    'CMakeLists.txt',
    'cmake/cef_variables.cmake',
    'tests/cefclient/CMakeLists.txt',
)
for subpath in cmake_script_paths:
    process_cmake_template(
        os.path.join(source_dir, subpath + '.in'),
        os.path.join(cef_root,   subpath),
        cef_paths2
    )
