
rem Update these paths to match your setup.
set ALERTS_DIR=X:\da\alert_data
set DEVICE_OUTPUT_LOGFILE=C:\da-tools\teraterm.log

python percepio_receiver.py --upload file --folder %ALERTS_DIR% --eof wait %DEVICE_OUTPUT_LOGFILE%
