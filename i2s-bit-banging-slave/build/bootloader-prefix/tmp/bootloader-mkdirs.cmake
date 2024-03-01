# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Espressif5.0/Espressif/frameworks/esp-idf-v5.0.4/components/bootloader/subproject"
  "C:/Users/ychabchoub/FW/master-slave-working-1/i2s-bit-banging-slave/build/bootloader"
  "C:/Users/ychabchoub/FW/master-slave-working-1/i2s-bit-banging-slave/build/bootloader-prefix"
  "C:/Users/ychabchoub/FW/master-slave-working-1/i2s-bit-banging-slave/build/bootloader-prefix/tmp"
  "C:/Users/ychabchoub/FW/master-slave-working-1/i2s-bit-banging-slave/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/ychabchoub/FW/master-slave-working-1/i2s-bit-banging-slave/build/bootloader-prefix/src"
  "C:/Users/ychabchoub/FW/master-slave-working-1/i2s-bit-banging-slave/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/ychabchoub/FW/master-slave-working-1/i2s-bit-banging-slave/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/ychabchoub/FW/master-slave-working-1/i2s-bit-banging-slave/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
