set(_EMBED_FILES
        "www/index_ov2640.html.gz"
        "www/index_ov3660.html.gz"
        "www/index_ov5640.html.gz"
        "www/monitor.html.gz")

idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       EMBED_FILES "favicon.ico" "upload_script.html" ${_EMBED_FILES}
                       PRIV_REQUIRES fatfs spiffs esp_http_server mdns app_update esp32-camera nvs_flash)