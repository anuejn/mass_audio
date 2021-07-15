#!/bin/bash

export ADF_PATH="$(pwd)/esp-adf/"
export IDF_PATH="$(pwd)/esp-adf/esp-idf"

virtualenv --python 3 --system-site-packages .venv
source .venv/bin/activate

# prepare esp-idf
"${IDF_PATH}/tools/idf_tools.py" --non-interactive install
"${IDF_PATH}/tools/idf_tools.py" --non-interactive install-python-env

. ./esp-adf/esp-idf/export.sh
