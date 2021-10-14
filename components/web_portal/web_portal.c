/* HTTP File Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "lwip/apps/netbiosns.h"
#include "esp_vfs.h"
#include "esp_http_server.h"
#include "file_manage.h"

static const char *TAG = "web_portal";

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    /* Get handle to embedded file upload script */
    extern const unsigned char login_start[] asm("_binary_login_html_start");
    extern const unsigned char login_end[]   asm("_binary_login_html_end");
    const size_t login_size = (login_end - login_start);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, (const char *)login_start, login_size);
    return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/* Handler to download a file kept on the server */
static esp_err_t index_handler(httpd_req_t *req)
{
    const char *filename = req->uri;

    if (strcmp(filename, "/") == 0) {
        return index_get_handler(req);
    } else if (strcmp(filename, "/favicon.ico") == 0) {
        return favicon_get_handler(req);
    }

    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

void portal_camera_start(httpd_handle_t camera_httpd);
esp_err_t start_file_server(httpd_handle_t server);

/* Function to start the file server */
esp_err_t web_portal_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 18;
    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server = NULL;
    ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
    httpd_start(&server, &config);

    /* URI handler for getting uploaded files */
    httpd_uri_t index = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL    // Pass server data as context
    };
    httpd_register_uri_handler(server, &index);

    start_file_server(server);
    portal_camera_start(server);

    return ESP_OK;
}
