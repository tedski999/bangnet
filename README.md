# bangnet

![Latest Build](https://github.com/tedski999/bangnet/actions/workflows/firmware_build.yml/badge.svg?branch=master&event=push)

Using networks of LoRa devices to triangulate the source of loud bangs. Designed and implemented in partial fulfilment of coursework for [CS7NS2 Internet of Things](https://teaching.scss.tcd.ie/module/cs7ns2-internet-of-things/).

See [CONTRIBUTING.md](CONTRIBUTING.md) for tips on how to work on this project.

## Getting Started

### Prerequisites

```sh
git clone --recursive -b release/v5.1 https://github.com/espressif/esp-idf.git
./esp-idf/install.sh esp32
. ./esp-idf/export.sh
```

### Running

```sh
git clone --recursive https://github.com/tedski999/bangnet.git
cd bangnet/firmware/sensor
espsecure.py generate_signing_key --version 1 --scheme ecdsa256 secure_boot_signing_key.pem
idf.py build
```

### Running

```sh
idf.py flash monitor
# Ctrl-] to quit
```

## References

Group project for [CS7NS2 Internet of Things](https://teaching.scss.tcd.ie/module/cs7ns2-internet-of-things/) \
Licensed under [GPLv3](LICENSE)
