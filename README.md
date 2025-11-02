# Zephyr


## Tutorials

- [Zephyr on the ESP32](https://www.youtube.com/watch?v=Z_7y_4O7yTw)
- [Zephyr Using NXP MCUXpresso Extension for VSCode](https://www.youtube.com/watch?v=-RHmk5il8iI)
- [Zephyr Device Trees](https://www.youtube.com/watch?v=Rk8p3OyW5A8)
- [Introduction to Zephyr Part 1: Getting Started - Installation and Blink | DigiKey](https://www.youtube.com/watch?v=mTJ_vKlMS_4&list=PLEBQazB0HUyTmK2zdwhaf8bLwuEaDH-52)
- [Zephyr esp32s3_devkitc](https://docs.zephyrproject.org/latest/boards/espressif/esp32s3_devkitc/doc/index.html)
- [Zephyr esp32s3_devkitc - schematic](https://dl.espressif.com/dl/schematics/SCH_ESP32-S3-DevKitC-1_V1.1_20220413.pdf)


## Zephyr ESP32-S3

### Getting started

Create a virtual environment in your chosen directory

```bash
D:\ESP32Zephyr> python -m venv zephyrproject\.venv
```

```bash
D:\ESP32Zephyr> ls
    Directory: D:\ESP32Zephyr
```

Activate the environment

```bash
PS D:\ESP32Zephyr> Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
PS D:\ESP32Zephyr> zephyrproject\.venv\Scripts\Activate.ps1
(.venv) PS D:\ESP32Zephyr>
```

Install 'west'

```bash
pip install west
```

Initialise the Repo and get the zephyr source code

```bash
west init zephyrproject
cd zephyrproject
west update
```

Now go and make some tea!

We now need to export a Zephyr CMake Package to allow us build the boilerplate code.

```bash
west zephyr-export
```

Install additional Python dependencies

```bash
west packages pip --install
```

Time for more tea!


### Install the Zephyr SDK

```bash
cd zephyr
# we should now be in D:\ESP32Zephyr\zephyrproject\zephyr
west sdk install
```

### Build for the ESP32-S3-DevKitC-1


Obtain all esp32 blobs:

```bash
west blobs fetch hal_espressif
```

#### Building Blinky

To build Blinky for the ESP32-s3 Devkit C you will need to modify the device tree to add an overlay for the device tree that defines the onboard led. The ESP32-S3 actually features an RDG led and not a standard onboard led like other Arduino style devices, so for this example we will drive an led on GPIO pin 8.

<img width="1536" height="1024" alt="image" src="https://github.com/user-attachments/assets/7ae6efd3-b436-4aac-89d5-bb21ec36e87f" />


Create a file: esp32s3_devkitc_procpu.overlay in the boards directory. D:\ESP32Zephyr\zephyrproject\zephyr\boards with the content below. Zephyr will automatically apply the overlay that matches the qualified board name.

```bash
/ {
    aliases {
        i2c-0 = &i2c0;
        watchdog0 = &wdt0;
        led0 = &onboard_led;
    };

    leds {
        compatible = "gpio-leds";

        onboard_led: led_0 {
            gpios = <&gpio0 8 GPIO_ACTIVE_LOW>;
            label = "GPIO LED";
        };
    };
};

```


A few quick rules of thumb when creating overlays:

* Use / { ... } when you add or modify things that live under the root (e.g. aliases, chosen, new top-level nodes like leds).
* Use &some_label { ... } when you want to modify an existing labeled node (e.g. &gpio0, &i2c0).
* We can have both in the same overlay (as we did): / { aliases { ... }; leds { ... }; }; plus references like gpios = <&gpio0 8 ...>;.
* Without the / { ... } wrapper, the overlay wouldn’t know where to place aliases (and it would fail to parse or end up in the wrong scope).

We can then build

```bash
west build -p always -b esp32s3_devkitc/esp32s3/procpu samples\basic\blinky
```

Or for a simpler example you could try:


```bash
 west build -p always -b esp32s3_devkitc/esp32s3/procpu .\samples\hello_world
```

But, continuing with blinky for now, lets flash our new image.

#### Flashing Blinky

Connect the esp32 to your development machine using a USB cable and then confirm the associated Com port in Device Manager.

<img width="379" height="57" alt="image" src="https://github.com/user-attachments/assets/63eba046-9aa2-4fc7-8d49-01a91f3c8d03" />


To flash the newly built image we can use. ```west flash```. It will search for connected devices and automatically copy the image to the board.


```bash
(.venv) PS D:\ESP32Zephyr\zephyrproject\zephyr>
-- west flash: rebuilding
ninja: no work to do.
-- west flash: using runner esp32
-- runners.esp32: reset after flashing requested
-- runners.esp32: Flashing esp32 chip on None (921600bps)
esptool v5.1.0
Connected to ESP32-S3 on COM11:
Chip type:          ESP32-S3 (QFN56) (revision v0.2)
Features:           Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, Embedded PSRAM 8MB (AP_3v3)
Crystal frequency:  40MHz
MAC:                d8:3b:da:a4:c5:7c

Stub flasher running.
Changing baud rate to 921600...
Changed.

Configuring flash size...
Flash will be erased from 0x00000000 to 0x00021fff...
Wrote 147456 bytes at 0x00000000 in 1.9 seconds (616.7 kbit/s).
Hash of data verified.

Hard resetting via RTS pin...
```


#### Building our own Application

The easiest way to get started with the example-application repository within an existing Zephyr workspace is to build a Workspace Application.

```bash
 git clone https://github.com/zephyrproject-rtos/example-application esp32s3_demo
```

I renamed the provided sample structure as follows:

```bash
D:\ESP32Zephyr\zephyrproject\applications\
└── esp32s3_demo/
    ├── src/main.c
    ├── prj.conf
    ├── CMakeLists.txt
    └── boards/esp32s3_devkitc.overlay
```

esp32s3_demo/CMakeLists.txt: The Build recipe. Finds Zephyr, defines project(app) and adds src/main.c. The Board type is normally selected via west build -b, and not hardcoded into the CMakeLists.txt file..

```make
cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app LANGUAGES C)
target_sources(app PRIVATE src/main.c)
```

esp32s3_demo/prj.conf: The Default Kconfig for this app. Enables GPIO, logging, blink feature, and sets main stack size.

```
# esp32s3_demo/prj.conf
CONFIG_BLINK=y
CONFIG_GPIO=y
CONFIG_LOG=y
CONFIG_LOG_DEFAULT_LEVEL=3
CONFIG_MAIN_STACK_SIZE=4096

```

esp32s3_demo/src/main.c: This is the App entry. Minimal LED blinker using DT_ALIAS(led0); toggles the pin via gpio_dt_spec.

```c
/*
 * Copyright (c) 2025 John O'Sullivan
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   2000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void)
{
	int ret;
	bool led_state = true;

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}

		led_state = !led_state;
		printf("LED state: %s\n", led_state ? "ON" : "OFF");
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}


```

esp32s3_demo/boards/esp32s3_devkitc.overlay: The Devicetree overlay for ESP32-S3 DevKitC. Adds a gpio-leds node and aliases { led0 = &onboard_led; }, wiring the LED used by the app.


```
/ {
    aliases {
        led0 = &onboard_led;
    };

    leds {
        compatible = "gpio-leds";

        onboard_led: led_0 {
            gpios = <&gpio0 8 GPIO_ACTIVE_HIGH>;
            label = "GPIO-8 LED";
        };
    };
};

```


esp32s3_demo/Kconfig. This is the App’s Kconfig entry point. Pulls in Kconfig.zephyr, sets up logging template, and defines
    CONFIG_BLINK under “Application options”.

```

# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0
#
# This file is the application Kconfig entry point. All application Kconfig
# options can be defined here or included via other application Kconfig files.
# You can browse these options using the west targets menuconfig (terminal) or
# guiconfig (GUI).

menu "Zephyr"
source "Kconfig.zephyr"
endmenu

module = APP
module-str = APP
source "subsys/logging/Kconfig.template.log_config"


# New: application-specific options
menu "Application options"
config BLINK
    bool "Enable blink logic"
    default y
    help
      Enable the LED blink loop in the application.
endmenu
```
Build the application (Note: we will be running our commands from D:\ESP32Zephyr\zephyrproject\applications>)

```bash
Remove-Item -Recurse -Force build\esp32s3_demo -ErrorAction SilentlyContinue

west build -p always `
  -b esp32s3_devkitc/esp32s3/procpu `
  -d build\esp32s3_demo `
  esp32s3_demo `
  -- `
  "-DDTC_OVERLAY_FILE=boards/esp32s3_devkitc.overlay"
```

Use "monitor" to view the serial output. Use "Ctrl + [" to exit.

```bash
west espressif monitor 
```

#### Also Noteworthy

  - esp32s3_demo/sample.yaml:1 — Twister/CI metadata for building/testing the sample; defines variants and an optional
    debug build overlay.
  - esp32s3_demo/debug.conf:1 — Kconfig overlay to enable debug-friendly options (e.g., CONFIG_DEBUG_OPTIMIZATIONS and
    verbose app logging). Use via -DOVERLAY_CONFIG=debug.conf.
  - esp32s3_demo/VERSION:1 — Application version file (major/minor/patch). Used to set project/app version metadata
    during the build.
  - esp32s3_demo/zephyr/module.yml:1 — Module descriptor for this repo context when used as a Zephyr module. Points
    board_root/dts_root to the repo so custom boards/bindings are discoverable and registers a custom runner script.

#### The relationship between prj.conf and kconfig

When you run west build, CMake runs the Zephyr Kconfig system (same as Linux’s menuconfig). Zephyr reads aAll upstream Kconfig files from the kernel, subsystems, and drivers. The app’s own Kconfig (if present) and the app’s prj.conf and then merges everything into a single generated configuration:
```build/zephyr/.config```. That file is the final resolved configuration used to compile the app. During the build, Zephyr generates: ```build/zephyr/include/generated/autoconf.h```

Note: only Kconfig files can define new symbols. 
