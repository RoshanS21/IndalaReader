# If you're using a server hosted locally, use a tool like ngrok to get a public IP.
    https://ngrok.com/

# Use the following command to get the certificates for your server and save it in a file.
# Replace <your_server_url> and <port>.
    openssl s_client -showcerts -connect <your_server_url:port> </dev/null | sed -n -e '/-BEGIN/,/-END/ p' > server_cert.pem
    Example:
    openssl s_client -showcerts -connect c911-2600-1700-e01-1550-3c22-24a1-23d-285f.ngrok-free.app:443 </dev/null | sed -n -e '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' > server_cert.pem
# Place server_cert.pem in your main directory.

# When Issues Occur
    * Check server URL
    * Check server certificate
    * Check Wi-Fi credentials
    * Make sure server returns proper Content-Type Headers. In this case, just json.
    * Check you're reading the proper # of bits for the reader you have.

# Do You Need to Change the Certificate Every Time?
    Yes, if you're using the free version of ngrok, the URLs and therefore the associated certificates can change every time you start a new session.
