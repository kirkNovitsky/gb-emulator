# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.9

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/kirk/gb-emu

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/kirk/gb-emu/build

# Include any dependencies generated for this target.
include basics/CMakeFiles/bitfields.dir/depend.make

# Include the progress variables for this target.
include basics/CMakeFiles/bitfields.dir/progress.make

# Include the compile flags for this target's objects.
include basics/CMakeFiles/bitfields.dir/flags.make

basics/CMakeFiles/bitfields.dir/bitfields.c.o: basics/CMakeFiles/bitfields.dir/flags.make
basics/CMakeFiles/bitfields.dir/bitfields.c.o: ../basics/bitfields.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/kirk/gb-emu/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object basics/CMakeFiles/bitfields.dir/bitfields.c.o"
	cd /home/kirk/gb-emu/build/basics && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/bitfields.dir/bitfields.c.o   -c /home/kirk/gb-emu/basics/bitfields.c

basics/CMakeFiles/bitfields.dir/bitfields.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/bitfields.dir/bitfields.c.i"
	cd /home/kirk/gb-emu/build/basics && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/kirk/gb-emu/basics/bitfields.c > CMakeFiles/bitfields.dir/bitfields.c.i

basics/CMakeFiles/bitfields.dir/bitfields.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/bitfields.dir/bitfields.c.s"
	cd /home/kirk/gb-emu/build/basics && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/kirk/gb-emu/basics/bitfields.c -o CMakeFiles/bitfields.dir/bitfields.c.s

basics/CMakeFiles/bitfields.dir/bitfields.c.o.requires:

.PHONY : basics/CMakeFiles/bitfields.dir/bitfields.c.o.requires

basics/CMakeFiles/bitfields.dir/bitfields.c.o.provides: basics/CMakeFiles/bitfields.dir/bitfields.c.o.requires
	$(MAKE) -f basics/CMakeFiles/bitfields.dir/build.make basics/CMakeFiles/bitfields.dir/bitfields.c.o.provides.build
.PHONY : basics/CMakeFiles/bitfields.dir/bitfields.c.o.provides

basics/CMakeFiles/bitfields.dir/bitfields.c.o.provides.build: basics/CMakeFiles/bitfields.dir/bitfields.c.o


# Object files for target bitfields
bitfields_OBJECTS = \
"CMakeFiles/bitfields.dir/bitfields.c.o"

# External object files for target bitfields
bitfields_EXTERNAL_OBJECTS =

basics/bitfields: basics/CMakeFiles/bitfields.dir/bitfields.c.o
basics/bitfields: basics/CMakeFiles/bitfields.dir/build.make
basics/bitfields: basics/CMakeFiles/bitfields.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/kirk/gb-emu/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable bitfields"
	cd /home/kirk/gb-emu/build/basics && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/bitfields.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
basics/CMakeFiles/bitfields.dir/build: basics/bitfields

.PHONY : basics/CMakeFiles/bitfields.dir/build

basics/CMakeFiles/bitfields.dir/requires: basics/CMakeFiles/bitfields.dir/bitfields.c.o.requires

.PHONY : basics/CMakeFiles/bitfields.dir/requires

basics/CMakeFiles/bitfields.dir/clean:
	cd /home/kirk/gb-emu/build/basics && $(CMAKE_COMMAND) -P CMakeFiles/bitfields.dir/cmake_clean.cmake
.PHONY : basics/CMakeFiles/bitfields.dir/clean

basics/CMakeFiles/bitfields.dir/depend:
	cd /home/kirk/gb-emu/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/kirk/gb-emu /home/kirk/gb-emu/basics /home/kirk/gb-emu/build /home/kirk/gb-emu/build/basics /home/kirk/gb-emu/build/basics/CMakeFiles/bitfields.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : basics/CMakeFiles/bitfields.dir/depend
