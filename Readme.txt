# If you're using a server hosted locally, use a tool like ngrok to get a public IP.
    https://ngrok.com/

# Use the following command to get the certificates for your server and save it in a file.
# Replace <your_server_url> with your server's URL.
    openssl s_client -showcerts -connect <your_server_url> </dev/null | sed -n -e '/-BEGIN/,/-END/ p' > server_cert.pem
# Place server_cert.pem in your main directory.

# When Issues Occur
    * Check server URL
    * Check server certificate
    * Check Wi-Fi credentials
    * Make sure server returns proper Content-Type Headers. In this case, just json.
    * Check you're reading the proper # of bits for the reader you have.
