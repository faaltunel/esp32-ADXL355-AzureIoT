#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := ADXL355-AzureIoT
ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
$(info $$ROOT_DIR is [${ROOT_DIR}])

EXTRA_COMPONENT_DIRS := ${ROOT_DIR}/esp32-ADXL355/components/ADXL355 ${ROOT_DIR}/esp-azure/components/azure_iot

include $(IDF_PATH)/make/project.mk

