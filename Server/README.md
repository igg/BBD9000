### Coop Management Software: BBD9000-CMS:
##### Hosted software that communicates with kiosks, coop/fleet members and administrators using HTML/HTTP.
* Maintains per-member account, balance, sales and contact information
* Administrative pages to keep track of inventory levels at kiosks, consolidated sales, etc.
* Member pages to update contact info, track purchases, add family members, credit-cards, etc.
* Credit Card Gateway communications for financial transactions (currently Authorize.net Card Present protocol)
* Implemented using Perl/MySQL
* Deployment using  [lighttpd] (http://www.lighttpd.net/)
* Perl source in `BBD9000/Server/web/fcgi-bin/`
  * Package for common tasks in `BBD.pm`
  * Perl script per web-page in `BBD-*.pl`
    * Perl Templates in `BBD9000/Server/tmpl/`
    * Setup scripts and other help in `BBD9000/Server/web/setup/`
      * These are meant to be read and commands executed manually for now depending on OS setup!
      * Please assume that these will completely and irreversibly buggeer your OS if blindly executed!!!

##### A missing directory is required in deployment `Server/priv`, containing the following files:
* `DB_CONF.cnf`: Database access credentials for server scripts. A file in the following format:

        # DB_CONF.cnf
        
        [client]
        host     = localhost
        database = bbd_members
        user     = bbd-server
        password = xxxxxxxx

* `BBD.pem`: Server's **private** key for kiosk communications
  * generate new 1024-bit RSA keys using the openssl CLI tools for the server and each kiosk.
  * Use openssl CLI tools to generate public versions of the private keys.
  * Each kiosk gets the server's **public** key, and its own **private** key.
    * The server gets its own **private** key and each kiosk's **public** key.
* `BBD-pub.pem`: Server's **public** counterpart to `BBD.pem` private key.  Needs to also be on the Kiosk ARM-SBC.
* `BBD9000-0001-pub.pem`: Kiosk **public** 1024-bit RSA keys by Kiosk database ID (this is for kiosk ID 1).
  * Each kiosk also has the **private** counterpart of this key on its ARM-SBC.
* `BBDC_GW.conf`: The credit-card gateway credentials, containing one uncommented line in the following format:
 
        CC_URL x_cpversion x_login x_market_type x_device_type x_tran_key
   e.g.:

        # These are the authorize.net transaction credentials
        # The order of the space-separated fields is:
        # CC_URL x_cpversion x_login x_market_type x_device_type x_tran_key
        https://cardpresent.authorize.net/gateway/transact.dll 1.0 xxxxxxxxxx 2 3 xxxxxxxxxx

