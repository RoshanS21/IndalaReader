idf_component_register(
    SRCS
        "http_client.c"
        "led_control.c"
        "main.c"
        "wiegand_reader.c"
        "wifi.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "server_cert.pem")
