<html>

<head>
<meta http-equiv="Biodiesel" content="text/html; charset=windows-1252">
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
<link href="CSS/bbd.css" rel="stylesheet" type="text/css">
<style type="text/css">
<!--

.notice {

background: #FFFFFF; 

} 

.style6 {font-size: 80%}
.style7 {color: #FF0000}
.style8 {color: #006600}
.style9 {color: #009933}
-->
</style>
</head>

<body id="home">
<div id="wrapper">

<div class="box">
</div>


 <?php include('menu_nav.php'); ?>



<div id="column1">

<center>

</center>

<!-- div class="notice">
<h2> The Power at <span class="style7">Mill Valley</span> is <span class="style8">ON</span> and the Pump is <span class="style9">UP</span></h2>
<p> Thanks Doug. </p>

</div -->

<div>
<h2>Welcome Test Coop Members</h2> 
<h4>BBD9000 self-service kiosks and coop management software</h4>
<p> The BBD9000 is a Free, Open Hardware/Open Source project now hosted at
<a href="https://github.com/igg/BBD9000">GitHub</a>.
BBD9000 supports automated self-service fuel kiosks, coop membership management, sales, etc.
</p>
 
<h3>How To Use the Pump</h3>	
   <p>In order to use the automated kiosk, every member needs to select a Secret Pump Number (SPN) that will be entered on the kiosk's keypad after swiping their credit card.  The SPN is any number you chose between 4 and 20 digits.
   Additionally, we need to make sure that we have your first and last name as it appears on your credit card(s).  The name read off of your credit card's magnetic strip and the SPN you type in at the keypad (followed by '#') is used to identify you as a member and lets you use our automated fuel dispensing kiosk.</p>
   
<h3>Your Name Matters </h3>
   <p>Since many people use a mix of names on their credit cards with or without initials, prefixes, etc, these are all ignored.  We only need the first and last name, which should not have any spaces, punctuation, prefixes or suffixes.  We do not need any other credit card information, and we never store any credit card numbers! This lets you use any credit card as long as the first and last names on the card match what we have in your account information.
   The last piece of information we need is a valid email address. The kiosk does not print receipts for purchases, and receipts can only be sent to an email address. If your email address changes, please update your contact information to continue to receive receipts for purchases.</p>

   <p>Using the kiosk entails swiping a credit card, entering your SPN, and waiting to be authorized by the server.  The door unlatches automatically if you are authorized.  It will probably need a little push to disengage the lock before pulling it open. There is no buzzing sound - just open the door if you are successfully authorized. Once the door is opened, pull the dispensing nozzle out, and flip the pump switch forward with the nozzle or by hand.  You will hear the pump come on, and you are ready to fill up. After filling up, note your dollar and fuel total on the display. When you replace the nozzle and shut off the pump, your credit card will be charged.  Put away the hose, and close the door (make sure its locked!).  A detailed receipt will arrive by email.
   To establish and maintain your contact information, we have set up web pages for individual coop members.  Members log into their page using a login and password just like any other "members only" web site. These web pages have other functions, like keeping track of your fuel usage, membership status, adding/modifying/deleting family members, etc.</p>
   
  
	
	</div>

</div>
<div id="column2">
   <div class="gutter">
  <div class="box2">
	  <h3>Carbon Tracker</h3>
	<div style="position:relative;left:10px" id="odometerDiv"></div>
	  <p>
	&nbsp;&nbsp;&nbsp;&nbsp;tons of CO<sub>2</sub> displaced by our members since 2013-06-11
	</p>
	<script src="/JavaScript/odometer.js" type="text/javascript"></script>
	<script type="text/javascript">
	<!--
		div = document.getElementById("odometerDiv");
		myOdometer = new Odometer(div, {digits: 6, hundredths: true, digitHeight: 20, digitWidth: 18, bustedness: 0});
		myOdometer.set(<?php include('carbon-tons.php'); ?>);
	//-->
	</script>

  </div>

   <?php include('kiosks.php'); ?>
  


</div>
</div>
 <div id="footer">Copyright 2005-2013 BBDC All rights reserved</div>
</div>

</body>
</html>
