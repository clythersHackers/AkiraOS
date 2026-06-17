# Goal

Create a clean, repeatable AkiraOS ESP32-S3 build environment in a container.

# Non-goals

Do not redesign AkiraOS.
Do not update Zephyr unless explicitly asked.
Do not edit generated build files.
Do not scan the whole Zephyr tree unless necessary.
Do not make broad refactors.

# Known target

Board: akiraconsole_esp32s3_procpu
Host: Fedora Linux
Container: VS Code devcontainer
Build command: ./build.sh -b akiraconsole

# Expected setup steps

1. Install required host packages.
2. Open repo in devcontainer.
3. Run west init -l .
4. Run west update.
5. Install Zephyr SDK v0.17.4.
6. Install WASI SDK version 24.
7. Export required environment variables.
8. Build native simulator first.
9. Then build ESP32-S3 target.

# Debugging rule

When a build fails:
1. Quote the first real compiler/config error.
2. Identify whether it is environment, dependency, board config, Zephyr API mismatch, or source error.
3. Propose one minimal change.
4. Do not attempt more than one fix at a time.