Import("env")

import os
import subprocess


def create_factory_bin(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    esptool = os.path.join(
        env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py"
    )

    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    app = os.path.join(build_dir, env.subst("${PROGNAME}.bin"))
    factory = os.path.join(build_dir, "factory.bin")

    cmd = [
        env.subst("$PYTHONEXE"),
        esptool,
        "--chip", "esp32c3",
        "merge_bin",
        "-o", factory,
        "0x0", bootloader,
        "0x8000", partitions,
        "0x10000", app,
    ]

    # Include LittleFS image if it was built (e.g. after "Upload Filesystem Image")
    littlefs = os.path.join(build_dir, "littlefs.bin")
    if os.path.isfile(littlefs):
        cmd += ["0x290000", littlefs]

    print(f"\nCreating factory binary...")
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"ERROR creating factory binary:\n{result.stderr}")
        return

    size = os.path.getsize(factory)
    print(f"Factory binary: {factory} ({size // 1024} KB)")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", create_factory_bin)
