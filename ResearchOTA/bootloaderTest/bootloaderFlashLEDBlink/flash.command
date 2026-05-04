serial_port=$(python3 wchSerialSearch.py)
upload_tool_arduino=/Users/deqinguser/Library/Arduino15/packages/realtek/tools/ameba_d_tools/1.1.3/upload_image_tool_macos
"$upload_tool_arduino" "$(pwd)" "$serial_port" "{board}" "Enable" "Disable" 1500000 
