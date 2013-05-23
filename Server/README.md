### Coop Management Software: BBD9000-CMS:
* Hosted software that communicates with kiosks, coop/fleet members and administrators using HTML/HTTP.
    * Maintains per-member account, balance, sales and contact information
    * Administrative pages to keep track of inventory levels at kiosks, consolidated sales, etc.
    * Member pages to update contact info, track purchases, add family members, credit-cards, etc.
    * Credit Card Gateway communications for financial transactions (currently Authorize.net Card Present protocol)
    * Implemented using Perl/MySQL
    * Source in `BBD9000/Server/web/fcgi-bin/`
    * Setup scripts and other help in `BBD9000/Server/web/setup/`
        * These are meant to be read and commands executed manually for now depending on OS setup!
        * Please assume that these will completely and irreversibly buggeer your OS if blindly executed!!!

