# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/git/esp-idf/components/bootloader/subproject"
  "D:/git/ESPIDF-UDP2DMX/build/bootloader"
  "D:/git/ESPIDF-UDP2DMX/build/bootloader-prefix"
  "D:/git/ESPIDF-UDP2DMX/build/bootloader-prefix/tmp"
  "D:/git/ESPIDF-UDP2DMX/build/bootloader-prefix/src/bootloader-stamp"
  "D:/git/ESPIDF-UDP2DMX/build/bootloader-prefix/src"
  "D:/git/ESPIDF-UDP2DMX/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/git/ESPIDF-UDP2DMX/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/git/ESPIDF-UDP2DMX/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
