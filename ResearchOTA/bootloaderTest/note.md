try to download https://github.com/ambiot/ambd_sdk/tree/dev

git clone --branch dev https://github.com/ambiot/ambd_sdk.git

but it seems does not compile on mac, let's try linux.

A lot of sh files needs chmod +x for permission.

Uncomment 

```
bootloader: CORE_TARGETS
	make -C asdk bootloader
```

and we can make bootloader only, such as
running in

```ambd_sdk/project/realtek_amebaD_va0_example/GCC-RELEASE/project_lp``` ```make bootloader```

then ambd_sdk/project/realtek_amebaD_va0_example/GCC-RELEASE/project_lp/asdk/image/km0_boot_all.bin appears.

we can also run ```make bootloader``` in ```ambd_sdk/project/realtek_amebaD_va0_example/GCC-RELEASE/project_lp/asdk```

Then we copy the km0_boot_all.bin to local and try to flash it, got error ```#[MODULE_BOOT-LEVEL_ERROR]:IMG2 SIGN Invalid```

try again with https://github.com/ambiot/ambd_sdk/archive/refs/tags/v1.0.1.zip

give +x to all script ```find . -type f -name "*.sh" -exec chmod +x {} \;```

Even the release V1.0.1 give the same error ```#[MODULE_BOOT-LEVEL_ERROR]:IMG2 SIGN Invalid```

Let's try the other way. Just do binary modification directly.

Compile bootloader in SDK, save image folder, change the OTA_Region, and we can see in km0_boot_all.bin, the 0xefe changed from 0x10 to 0x20. And the 0xef8 used to be ```00 60 00 08 00 60 10 08```. In the Arduino version of km0_boot_all.bin, the sequence is located in 0x111C.

LS_IMG2_OTA2_ADDR seems not changing anything

OK. clone, ```https://github.com/Ameba-AIoT/ameba-rtos-d``` and use the missing ```rtl8721d_usi*.h``` from ```https://github.com/ambiot/ambd_sdk```. The bootloader compiles and runs. Just not jump to img2.

OK. ota_get_cur_index may need to be updated!
















