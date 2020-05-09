cmake_minimum_required(VERSION 3.5)

# set the project name and version
project(telefoon VERSION 1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_BUILD_TYPE Debug)


set (LIBPATH /opt/rootfs-s5p4418/lib/arm-linux-gnueabihf)
set (USERLIBPATH /opt/rootfs-s5p4418/usr/lib/arm-linux-gnueabihf)
set (SYSROOTPATH /home/dig/nanoPiFire2A/tools/4.9.3/arm-cortexa9-linux-gnueabihf/sys-root/)
set (SYSROOTLIB ${SYSROOTPATH}/lib/arm-linux-gnueabihf)
set (SYSROOTUSERLIB ${SYSROOTPATH}/lib/arm-linux-gnueabihf)

add_subdirectory(src)



