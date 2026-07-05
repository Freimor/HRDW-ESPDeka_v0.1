# ESPDeka - offgrid LORA communicator (Guition JC4880P443C / ESP32-P4)
# Stage 1.1: RTOS bring-up + basic chat application.

cmake_minimum_required(VERSION 3.16)

# Extra components live in the top-level "components" directory (default),
# no additional EXTRA_COMPONENT_DIRS required.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(espdeka)
