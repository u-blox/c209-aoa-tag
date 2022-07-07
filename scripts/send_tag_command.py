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

    parser.add_argument(
        "--address",
        dest="address",
        required=False,
        help="Mac of tag to send command to",
    )

    args = parser.parse_args()
    res = None
    flatten_results = []
    if (args.address == None):
        tags = ble_list_uart_devices(timeout=10.0)
        print("Found {0} AoA tags".format(len(tags)))
        print(tags)
        res = ble_send_at_commands(list(tags.values()), args.commands)
    else:
        res = ble_send_at_commands(args.address, args.commands)

    if isinstance(res[0], list):
        flatten_results = [x for xs in res for x in xs]
        print(flatten_results)
    else:
        flatten_results = res

    num_errors = 0
    for reply in flatten_results:
        if "ERROR" in reply:
            num_errors = num_errors + 1

    if num_errors > 0:
        print("{0} tag returned an ERROR".format(num_errors))
    else:
        print("All tags replied successfully")
