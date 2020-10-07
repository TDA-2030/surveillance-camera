
idf_component_register(SRC_DIRS "."
        INCLUDE_DIRS "include" "."
        # PRIV_REQUIRES esp32-camera esp-face nvs_flash esp_http_server fb_gfx
        EMBED_FILES "www/index_ov2640.html.gz" "www/index_ov3660.html.gz"
        )
