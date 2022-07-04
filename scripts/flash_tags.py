import argparse, sys, subprocess
from multiprocessing import Process


def run_command(cmd):
    subprocess.run(cmd)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Tool to flash a list of COM ports using nRF DFU bootloader (nrfutil tool)."
    )

    parser.add_argument(
        "--com_ports",
        dest="com_ports",
        required=True,
        nargs="+",
        help="Serial port of the tags to be flashed.",
    )

    parser.add_argument(
        "--fw_file", dest="fw_file", required=True, help="FW File to be flashed"
    )

    args = parser.parse_args()
    flash_processes = []
    for port in args.com_ports:
        arg = "nrfutil dfu serial -pkg {0} -p {1} -b 115200 -fc 1".format(
            args.fw_file, port
        )
        print(arg)
        p = Process(target=run_command, args=(arg,))
        flash_processes.append(p)
        p.start()

    for process in flash_processes:
        process.join()

    print("Flashing finished!")
