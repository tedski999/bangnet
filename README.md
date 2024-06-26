# bangnet

![Latest Build](https://github.com/tedski999/bangnet/actions/workflows/firmware_build.yml/badge.svg?branch=master&event=push)
![Latest Tag](https://github.com/tedski999/bangnet/actions/workflows/ota_patch_build.yml/badge.svg?branch=master&event=push)

Using networks of LoRa devices to triangulate the source of loud bangs. Designed and implemented in partial fulfilment of coursework for [CS7NS2 Internet of Things](https://teaching.scss.tcd.ie/module/cs7ns2-internet-of-things/).

# Outline
Against the backdrop of global climate change and frequent natural disasters, the BangNet project utilises advanced IoT technology and cloud computing capabilities with the goal of early detection of lightning-strike induced wildfires. For our IoT devices, we developed ESP32 firmware to detect thunder, upload timestamps and capture images, drive servomotors, communicate over LoRa and perform remote delta-encoded firmware patching. For our cloud application, we built an AWS-based platform that enables computation of precise lightning strike locations and fire risk assessment. The success of the project not only proves the application potential of IoT and cloud computing in the field of disaster prevention, but also has provided us with valuable experience in technological innovation and teamwork.

See [CONTRIBUTING.md](CONTRIBUTING.md) for tips on how to work on this project.

## Getting Started

### Prerequisites

```sh
git clone --recursive -b release/v5.1 https://github.com/espressif/esp-idf.git
./esp-idf/install.sh esp32
. ./esp-idf/export.sh
```

### Building Firmware

```sh
git clone --recursive https://github.com/tedski999/bangnet.git
espsecure.py generate_signing_key --version 1 --scheme ecdsa256 bangnet/firmware/secure_boot_signing_key.pem
ln -s ../secure_boot_signing_key.pem bangnet/firmware/sensor/secure_boot_signing_key.pem
ln -s ../secure_boot_signing_key.pem bangnet/firmware/camera/secure_boot_signing_key.pem
ln -s ../secure_boot_signing_key.pem bangnet/firmware/servo/secure_boot_signing_key.pem
idf.py --project-dir bangnet/firmware/sensor build
idf.py --project-dir bangnet/firmware/camera build
idf.py --project-dir bangnet/firmware/servo build
```

### Flashing Firmware

```sh
idf.py --port /dev/ttyUSBx --project-dir bangnet/firmware/sensor flash
idf.py --port /dev/ttyUSBy --project-dir bangnet/firmware/camera flash
idf.py --port /dev/ttyUSBz --project-dir bangnet/firmware/servo flash
# Attach to serial of sensor: idf.py --port /dev/ttyUSBx monitor
```

## References

Group project for [CS7NS2 Internet of Things](https://teaching.scss.tcd.ie/module/cs7ns2-internet-of-things/) \
Licensed under [GPLv3](LICENSE)
