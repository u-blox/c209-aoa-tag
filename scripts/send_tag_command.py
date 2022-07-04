from ble_tag_control import ble_send_at_commands, ble_list_uart_devices
import argparse, sys

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Tool to send AT command to AoA Tags with the NUS Service"
    )

    parser.add_argument(
        "--commands",
        dest="commands",
        required=True,
        nargs="+",
        help="List of AT commands to send to all tags",
    )

    args = parser.parse_args()

    tags = ble_list_uart_devices(timeout=10.0)
    print("Found {0} AoA tags".format(len(tags)))
    print(tags)
    mac_addresses = []

    res = ble_send_at_commands(list(tags.values()), args.commands)
    flatten_results = [x for xs in res for x in xs]
    print(flatten_results)

    num_errors = 0
    for reply in flatten_results:
        if "ERROR" in reply:
            num_errors = num_errors + 1

    if num_errors > 0:
        print("{0} tag returned an ERROR".format(num_errors))
    else:
        print("All tags replied successfully")
