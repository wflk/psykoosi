# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

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
CMAKE_SOURCE_DIR = /home/mike/psykoosi

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/mike/psykoosi

# Include any dependencies generated for this target.
include depends/capstone/CMakeFiles/test_ppc.dir/depend.make

# Include the progress variables for this target.
include depends/capstone/CMakeFiles/test_ppc.dir/progress.make

# Include the compile flags for this target's objects.
include depends/capstone/CMakeFiles/test_ppc.dir/flags.make

depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.o: depends/capstone/CMakeFiles/test_ppc.dir/flags.make
depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.o: depends/capstone/tests/test_ppc.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/mike/psykoosi/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.o"
	cd /home/mike/psykoosi/depends/capstone && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/test_ppc.dir/tests/test_ppc.c.o   -c /home/mike/psykoosi/depends/capstone/tests/test_ppc.c

depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/test_ppc.dir/tests/test_ppc.c.i"
	cd /home/mike/psykoosi/depends/capstone && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/mike/psykoosi/depends/capstone/tests/test_ppc.c > CMakeFiles/test_ppc.dir/tests/test_ppc.c.i

depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/test_ppc.dir/tests/test_ppc.c.s"
	cd /home/mike/psykoosi/depends/capstone && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/mike/psykoosi/depends/capstone/tests/test_ppc.c -o CMakeFiles/test_ppc.dir/tests/test_ppc.c.s

depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.o.requires:
.PHONY : depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.o.requires

depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.o.provides: depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.o.requires
	$(MAKE) -f depends/capstone/CMakeFiles/test_ppc.dir/build.make depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.o.provides.build
.PHONY : depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.o.provides

depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.o.provides.build: depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.o

# Object files for target test_ppc
test_ppc_OBJECTS = \
"CMakeFiles/test_ppc.dir/tests/test_ppc.c.o"

# External object files for target test_ppc
test_ppc_EXTERNAL_OBJECTS =

depends/capstone/test_ppc: depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.o
depends/capstone/test_ppc: depends/capstone/CMakeFiles/test_ppc.dir/build.make
depends/capstone/test_ppc: depends/capstone/libcapstone.a
depends/capstone/test_ppc: depends/capstone/CMakeFiles/test_ppc.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking C executable test_ppc"
	cd /home/mike/psykoosi/depends/capstone && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/test_ppc.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
depends/capstone/CMakeFiles/test_ppc.dir/build: depends/capstone/test_ppc
.PHONY : depends/capstone/CMakeFiles/test_ppc.dir/build

depends/capstone/CMakeFiles/test_ppc.dir/requires: depends/capstone/CMakeFiles/test_ppc.dir/tests/test_ppc.c.o.requires
.PHONY : depends/capstone/CMakeFiles/test_ppc.dir/requires

depends/capstone/CMakeFiles/test_ppc.dir/clean:
	cd /home/mike/psykoosi/depends/capstone && $(CMAKE_COMMAND) -P CMakeFiles/test_ppc.dir/cmake_clean.cmake
.PHONY : depends/capstone/CMakeFiles/test_ppc.dir/clean

depends/capstone/CMakeFiles/test_ppc.dir/depend:
	cd /home/mike/psykoosi && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/mike/psykoosi /home/mike/psykoosi/depends/capstone /home/mike/psykoosi /home/mike/psykoosi/depends/capstone /home/mike/psykoosi/depends/capstone/CMakeFiles/test_ppc.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : depends/capstone/CMakeFiles/test_ppc.dir/depend

