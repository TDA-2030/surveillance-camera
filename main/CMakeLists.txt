
idf_component_register(SRC_DIRS "." "avi_player"
        INCLUDE_DIRS "include" "." "avi_player"
        # PRIV_REQUIRES esp32-camera esp-face nvs_flash esp_http_server fb_gfx
        EMBED_FILES "www/index_ov2640.html.gz" "www/index_ov3660.html.gz"
        )
