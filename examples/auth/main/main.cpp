#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <map>
#include <set>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/ledc.h>
#include "sdkconfig.h"
#include <esp_event.h>
#include <esp_event_loop.h>
#include <string>
#include "express.h"
#include "mdns.h"
#include "protocol_examples_common.h"
#include "www_fs.h"
#include "ramlog.h"

static char tag[] = "EXM";
#if 1
#define msg_info(fmt, args...) ESP_LOGI(tag, fmt, ## args);
#define msg_init(fmt, args...)  ESP_LOGI(tag, fmt, ## args);
#define msg_debug(fmt, args...)  ESP_LOGI(tag, fmt, ## args);
#define msg_error(fmt, args...)  ESP_LOGE(tag, fmt, ## args);
#else
#define msg_info(fmt, args...)
#define msg_init(fmt, args...)
#define msg_debug(fmt, args...)
#define msg_error(fmt, args...)  ESP_LOGE(TAG, fmt, ## args);
#endif


Express e;



void SETUP_task(void* parameter)
{
	/* Use session */
	e.use("", e.getSessionMW());
	/* Use json MW */
	e.use("", e.getJsonMW());

	auto withAuth = e.getWithAuthMW();

	e.get("api/info", withAuth,  [](ExRequest* req) {
		req->json("[{\"k\": \"Serial number\", \"v\": 1},{\"k\": \"Firmware\", \"v\": \"ESP32 test v1.0.0\"} ]");
	});
	e.get("api/network", withAuth, [](ExRequest* req) {
		req->json("{\"ip\": \"192.168.124.227\", \"netmask\": \"255.255.255.0\", \"gateway\": \"\", \"dhcp\": \"STATIC\", \"ntp\": \"NONTP\", \"ntps\": \"\" }");
	});
	e.get("api/log", [](ExRequest* req) {
		req->txt(RAMLog::instance()->read().c_str());
	});
	e.post("api/login", e.getStdLoginFunction());

	e.get("api/logout", [](ExRequest* req) {
		e.doLogOut(req);
		req->json("{ \"ok\": true }");
	});
	e.addUser("admin", "admin");


	e.addStatic(www_filesystem);

	e.on("test", [](WSRequest* req, char* arg, int arg_len) {
		std::string s(arg, arg_len);
		msg_debug("Got test value <%s>", s.c_str());
		req->send(s.c_str());
	});
	vTaskDelay(100);
	e.start(80, 0, 1);
	// e.start(80, 7, 1); // More speed
	vTaskDelete(NULL); /* Delete SETUP task */
}

/*!
 * \brief MAIN.
 */
extern "C" void app_main(void)
{
	esp_chip_info_t chip_info;
	std::string m_hostname = "express";
	// esp_pm_config_esp32_t m_pcfg;

	/* Activate RAM log */
	RAMLog::instance()->install();


	/* Print chip information */
	esp_chip_info(&chip_info);
	ESP_LOGI(tag, "This is ESP32 chip with %d CPU cores, WiFi%s%s, ", chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "", (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
	ESP_LOGI(tag, "silicon revision %d, ", chip_info.revision);
	// ESP_LOGI(tag,"CPU frequency: %d, Maximum priority level: %d, IDLE priority level: %d",getCpuFrequencyMhz(), configMAX_PRIORITIES - 1, tskIDLE_PRIORITY);
	ESP_LOGI(tag, "main_task: active on core %d", xPortGetCoreID());


	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	// m_pcfg.light_sleep_enable = false;
	// m_pcfg.max_freq_mhz = 80;
	// m_pcfg.min_freq_mhz = 80;
	// ESP_ERROR_CHECK(esp_pm_configure(&m_pcfg));


	/* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
	 * Read "Establishing Wi-Fi or Ethernet Connection" section in
	 * examples/protocols/README.md for more information about this function.
	 */
	ESP_ERROR_CHECK(example_connect());

	//initialize mDNS service
	ESP_ERROR_CHECK(mdns_init());
	mdns_hostname_set(m_hostname.c_str());
	mdns_instance_name_set(m_hostname.c_str());
	mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
	msg_info("Host name: %s.local\n", m_hostname.c_str());

	xTaskCreatePinnedToCore(SETUP_task, "SETUP", 10000, NULL, /* prio */ 0, /* task handle */ NULL, 1);
	vTaskDelete(NULL); /* Delete SETUP task */
}
