cmake_minimum_required(VERSION 3.20)
project(Synth1PatchCoreStandalone LANGUAGES CXX)

# Build the same maintained core without JUCE, GTest, FetchContent or a host.
include("${CMAKE_CURRENT_LIST_DIR}/../../../../../cmake/Synth1PatchCore.cmake")
