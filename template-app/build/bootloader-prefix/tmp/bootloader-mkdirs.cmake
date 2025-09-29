# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/SeHyun/esp/esp-idf-release-v4.4/components/bootloader/subproject"
  "C:/GItKraken/ReadMark_Embedded_Code/template-app/build/bootloader"
  "C:/GItKraken/ReadMark_Embedded_Code/template-app/build/bootloader-prefix"
  "C:/GItKraken/ReadMark_Embedded_Code/template-app/build/bootloader-prefix/tmp"
  "C:/GItKraken/ReadMark_Embedded_Code/template-app/build/bootloader-prefix/src/bootloader-stamp"
  "C:/GItKraken/ReadMark_Embedded_Code/template-app/build/bootloader-prefix/src"
  "C:/GItKraken/ReadMark_Embedded_Code/template-app/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/GItKraken/ReadMark_Embedded_Code/template-app/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
