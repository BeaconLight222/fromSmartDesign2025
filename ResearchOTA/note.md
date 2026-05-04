## notes

Imagetool.exe can use serial to erase chip

We can connect serial and erase 2MB of the chip (0x08000000)

And we need at least 3 files:

km0_boot_all.bin     @ 0x08000000
km4_boot_all.bin     @ 0x08004000
km0_km4_image2.bin   @ 0x08006000

km0_km4_image2.bin seems converted from axf file and merge with other code. 
The bins seems matching the flash content directly, at least at the beginning.

By default OTA1 start at 0x08006000 OTA2 start at 0x08106000 (2M flash)




## BW16_OTA_switchTest

I created a sketch BW16_OTA_switchTest, with help from https://github.com/MXV3A/BW16-OTA-Updates/blob/main/AnchorOTA.cpp

ota_get_cur_index() can tell we are from OTA1 (0) or OTA2 (1)

The logic seems to be the bootloader will check if 0x08106000 has 0x35393138 (OTA_VALID), if not check 0x08006000

I used flash_erase_sector flash_stream_read flash_stream_write to copy data from OTA_ADDRESS_1 to OTA_ADDRESS_2, after reboot it just work in OTA2.

It seems the OTA firmware does not need any code special. The bootloader already support OTA1 OTA2 boot.


## reading from application note

4M mapping 9.1.4
13 OTA
