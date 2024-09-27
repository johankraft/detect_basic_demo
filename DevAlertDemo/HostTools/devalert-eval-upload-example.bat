rem Example script for uploading DevAlert data from a serial terminal log to a DevAlert evaluation account
echo off

python devalertserial.py --upload sandbox teraterm.log | devalerthttps.exe store-trace
