# Tag helper scripts
Those are some scripts to help testing with many tags.

## Installing
`pip install -r requirements.txt`

## Usage
### Flash many tags at once connected over serial.

Check the usage with `python flash_tags.py --help`

Example: `python -u flash_tags.py --com_ports COMXX COMYY --fw_file NINA-B4-DF-TAG-SW-2-0.zip`

### Sending AT commands to tags over BLE

Check the usage with `python send_tag_command.py --help`

Example: `python -u send_tag_command.py --address E2:72:10:01:FC:0D --commands ATI9 AT+ADVINT=20`