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

I (2265) WIEGAND_READER: Wiegand Reader Initialized on Pins 4 (D0) and 5 (D1) I (2275) main_task: Returned from app_main() I (5865) WIEGAND_READER: Processing 32 bits I (5865) WIEGAND_READER: Facility Code: 58 I (5865) WIEGAND_READER: Card Number: 2000584704 I (5865) WIEGAND_READER: Card Number(Hex): 773e8000 I (5865) WIEGAND_READER: E (5915) WIEGAND_READER: Wiegand timeout, received 32 bits (incomplete data) I (5915) WIEGAND_READER: Wiegand data: 10011101001110111001111101000000 I (6805) WIEGAND_READER: HTTP_EVENT_ON_CONNECTED I (6805) WIEGAND_READER: HTTP_EVENT_HEADER_SENT I (7175) WIEGAND_READER: HTTP_EVENT_ON_HEADER, key=Content-Length, value=48 I (7175) WIEGAND_READER: HTTP_EVENT_ON_HEADER, key=Content-Type, value=application/json; charset=utf-8 I (7185) WIEGAND_READER: HTTP_EVENT_ON_HEADER, key=Date, value=Mon, 25 Nov 2024 23:55:09 GMT I (7195) WIEGAND_READER: HTTP_EVENT_ON_HEADER, key=Etag, value=W/"30-3AEUhKFRTum+wsLiyMZHKN/H0+Q" I (7205) WIEGAND_READER: HTTP_EVENT_ON_HEADER, key=Ngrok-Agent-Ips, value=2603:8081:1410:5a02:3104:d336:b66a:5247 I (7215) WIEGAND_READER: HTTP_EVENT_ON_HEADER, key=X-Powered-By, value=Express I (7225) WIEGAND_READER: HTTP_EVENT_ON_DATA, len=48 I (7225) WIEGAND_READER: HTTP_EVENT_ON_FINISH I (7235) WIEGAND_READER: Server Response: {"message":"Card authorized","status":"success"} I (7245) WIEGAND_READER: Message: Card authorized I (7245) WIEGAND_READER: Status: success I (7255) WIEGAND_READER: HTTP POST Status: 200 I (7255) WIEGAND_READER: HTTP_EVENT_DISCONNECTED I (34575) WIEGAND_READER: Processing 32 bits I (34575) WIEGAND_READER: Facility Code: 58 I (34575) WIEGAND_READER: Card Number: 1927336448 I (34575) WIEGAND_READER: Card Number(Hex): 72e0d200 I (34585) WIEGAND_READER: E (34625) WIEGAND_READER: Wiegand timeout, received 32 bits (incomplete data) I (34625) WIEGAND_READER: Wiegand data: 00011101001110010111000001101001 I (35475) WIEGAND_READER: HTTP_EVENT_ON_CONNECTED I (35475) WIEGAND_READER: HTTP_EVENT_HEADER_SENT I (35725) WIEGAND_READER: HTTP_EVENT_ON_HEADER, key=Content-Length, value=48 I (35725) WIEGAND_READER: HTTP_EVENT_ON_HEADER, key=Content-Type, value=application/json; charset=utf-8 I (35735) WIEGAND_READER: HTTP_EVENT_ON_HEADER, key=Date, value=Mon, 25 Nov 2024 23:55:38 GMT I (35735) WIEGAND_READER: HTTP_EVENT_ON_HEADER, key=Etag, value=W/"30-3AEUhKFRTum+wsLiyMZHKN/H0+Q" I (35745) WIEGAND_READER: HTTP_EVENT_ON_HEADER, key=Ngrok-Agent-Ips, value=2603:8081:1410:5a02:3104:d336:b66a:5247 I (35765) WIEGAND_READER: HTTP_EVENT_ON_HEADER, key=X-Powered-By, value=Express I (35765) WIEGAND_READER: HTTP_EVENT_ON_DATA, len=48 I (35775) WIEGAND_READER: HTTP_EVENT_ON_FINISH I (35775) WIEGAND_READER: Server Response: {"message":"Card authorized","status":"success"} I (35785) WIEGAND_READER: Message: Card authorized I (35795) WIEGAND_READER: Status: success I (35795) WIEGAND_READER: HTTP POST Status: 200 I (35805) WIEGAND_READER: HTTP_EVENT_DISCONNECTED It works now. What did we really change? Do we have to change the certificate every time I run ngrok?
Great to see that it's working now! The changes we made involved verifying and updating the server certificate to ensure it matched the current ngrok URL. This was crucial because ngrok URLs can change every time you start a new ngrok session unless you're using a reserved subdomain with a paid plan.

Key Changes:
Updated the Server Certificate: We fetched and updated the server certificate to ensure it matched the ngrok URL currently in use.

Verified Network Connectivity: Confirmed that the network settings and connectivity were correct.

# Do You Need to Change the Certificate Every Time?
    Yes, if you're using the free version of ngrok, the URLs and therefore the associated certificates can change every time you start a new session.
