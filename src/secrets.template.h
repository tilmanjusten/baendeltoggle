#define WIFI_SSID "..."
#define WIFI_PASS "..."
#define HTTP_PORT 80
#define URL_PARAM_ON "on"
#define URL_PARAM_OFF "off"
#define URL_HOST "lichter.fritz.box"
#define URL_PATH "/switch/"
#define OTA_PORT 8266
#define OTA_HOSTNAME "tuer"
#define OTA_PASSWORD ""

/* Devices
 * 0: Alle Lichter (alle-lichter)
 * 1: Wohnzimmerlicht (wohnzimmerlicht)
 * 2: Sofa & Regal (sofalicht)
 * 3: Lichterkette (lichterkette)
 * 4: Türlicht (tuerlicht)
 * 5: Galerie (galerielicht)
 */
//const char *devices[] = {"tuerlicht", "galerielicht", "lichterkette", "sofalicht"};
const char *devices[] = {"alle-lichter"};
//const char* devices[] = {"sofalicht", "lichterkette", "galerielicht"};
//const char* devices[] = {"tuerlicht", "galerielicht"};
