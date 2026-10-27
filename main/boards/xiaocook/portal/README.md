# XiaoCook Wi-Fi portal

Customized from 78/esp-wifi-connect 3.3.1 (MIT; see LICENSE).
The HTML is self-contained and runs without external fonts, scripts or images.
Vietnamese copy and Apple-inspired light styling are owned here; the Wi-Fi HTTP API is unchanged.

Only CONFIG_BOARD_TYPE_XIAOCOOK embeds these pages. main/CMakeLists.txt uses GNU
linker --wrap on the upstream HTML start symbols and --defsym to point them to
these project-owned, null-terminated assets. No managed component is modified.
After updating esp-wifi-connect, verify its embedded symbol names and HTTP API.

The hotspot and DHCP hostname use XiaoCook- followed by the final two MAC bytes.
Build with the existing XiaoCook preset; flash the firmware to see it on hardware.
Preview the HTML with a local HTTP server and mocked /scan, /saved/list and
/advanced/config responses. A browser preview does not validate ESP32 Wi-Fi.
