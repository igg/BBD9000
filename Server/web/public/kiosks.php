<?php
	$kiosk_directories = glob(dirname(__FILE__) . '/kiosks/*' , GLOB_ONLYDIR);
	foreach ($kiosk_directories as $dirname) {
		echo '<div class="box2">';
		include('kiosks/'.basename($dirname).'/'.'kiosk.php');
		echo '</div>';
	}
?>
  
<div class="box2">
	<h3>Operation</h3>
	<h4>24/7 access at all our locations!</h4>
	<p>*** Please Note: "trial" members pay $0.75 additional per gallon</p>
</div>
