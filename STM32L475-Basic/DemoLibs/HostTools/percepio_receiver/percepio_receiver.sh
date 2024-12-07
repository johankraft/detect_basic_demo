#!/bin/bash

# Update these paths to match your setup.
export ALERTS_DIR=$HOME/Shared/da/alerts_data
export DEVICE_OUTPUT_LOGFILE=./device.log

python3 percepio_receiver.py --upload file --folder $ALERTS_DIR --eof wait $DEVICE_OUTPUT_LOGFILE
