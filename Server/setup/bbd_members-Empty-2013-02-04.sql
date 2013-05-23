# ************************************************************
# Sequel Pro SQL dump
# Version 4004
#
# http://www.sequelpro.com/
# http://code.google.com/p/sequel-pro/
#
# Host: 127.0.0.1 (MySQL 5.5.29-0ubuntu0.12.04.1)
# Database: bbd_members
# Generation Time: 2013-02-04 14:52:07 +0000
# ************************************************************


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;


# Dump of table alert_roles
# ------------------------------------------------------------

DROP TABLE IF EXISTS `alert_roles`;

CREATE TABLE `alert_roles` (
  `alert_role_id` bigint(20) NOT NULL AUTO_INCREMENT,
  `alert_id` varchar(64) NOT NULL,
  `role_id` varchar(64) NOT NULL,
  PRIMARY KEY (`alert_role_id`),
  KEY `alert_idx` (`alert_id`),
  KEY `role_idx` (`role_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

LOCK TABLES `alert_roles` WRITE;
/*!40000 ALTER TABLE `alert_roles` DISABLE KEYS */;

INSERT INTO `alert_roles` (`alert_role_id`, `alert_id`, `role_id`)
VALUES
	(2,'Gateway','Gateway Czar'),
	(3,'Fuel','Fuel Alerts'),
	(4,'Power','Kiosk Czar'),
	(5,'Security','Kiosk Czar'),
	(6,'Software','Software Czar'),
	(7,'Network','Kiosk Czar');

/*!40000 ALTER TABLE `alert_roles` ENABLE KEYS */;
UNLOCK TABLES;


# Dump of table alerts
# ------------------------------------------------------------

DROP TABLE IF EXISTS `alerts`;

CREATE TABLE `alerts` (
  `alert_id` varchar(64) NOT NULL,
  PRIMARY KEY (`alert_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

LOCK TABLES `alerts` WRITE;
/*!40000 ALTER TABLE `alerts` DISABLE KEYS */;

INSERT INTO `alerts` (`alert_id`)
VALUES
	('Fuel'),
	('Gateway'),
	('Network'),
	('Power'),
	('Security'),
	('Software');

/*!40000 ALTER TABLE `alerts` ENABLE KEYS */;
UNLOCK TABLES;


# Dump of table cc_transactions
# ------------------------------------------------------------

DROP TABLE IF EXISTS `cc_transactions`;

CREATE TABLE `cc_transactions` (
  `cc_trans_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `amount` float NOT NULL,
  `pre_auth` float DEFAULT '0',
  `status` enum('PREAUTH','CAPTURED','VOIDED','ERROR') NOT NULL DEFAULT 'ERROR',
  `message` varchar(128) NOT NULL,
  `member_id` int(10) unsigned NOT NULL,
  `kiosk_id` int(10) unsigned NOT NULL,
  `timestamp` datetime NOT NULL,
  `response_code` int(11) NOT NULL,
  `reason_code` int(11) NOT NULL,
  `auth_code` varchar(64) NOT NULL,
  `gwy_trans_id` varchar(64) NOT NULL,
  `cc_id` bigint(20) DEFAULT NULL,
  PRIMARY KEY (`cc_trans_id`),
  KEY `member_id` (`member_id`),
  KEY `kiosk_id` (`kiosk_id`),
  KEY `response_code` (`response_code`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;



# Dump of table deliveries
# ------------------------------------------------------------

DROP TABLE IF EXISTS `deliveries`;

CREATE TABLE `deliveries` (
  `delivery_id` bigint(11) unsigned NOT NULL AUTO_INCREMENT,
  `member_id` bigint(20) unsigned DEFAULT NULL,
  `from_kiosk_id` bigint(20) unsigned DEFAULT NULL,
  `timestamp` datetime NOT NULL,
  `kiosk_id` bigint(20) unsigned NOT NULL,
  `item` varchar(64) NOT NULL,
  `quantity` float NOT NULL,
  `price_per_item` float NOT NULL,
  `total_price` float NOT NULL,
  PRIMARY KEY (`delivery_id`),
  KEY `timestamp_idx` (`timestamp`),
  KEY `item_idx` (`item`),
  KEY `kiosk_idx` (`kiosk_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;



# Dump of table fuel_prices
# ------------------------------------------------------------

DROP TABLE IF EXISTS `fuel_prices`;

CREATE TABLE `fuel_prices` (
  `type` enum('FULL','ONE-DAY','NON-FUELING') NOT NULL,
  `price_per_gallon` float NOT NULL,
  PRIMARY KEY (`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

LOCK TABLES `fuel_prices` WRITE;
/*!40000 ALTER TABLE `fuel_prices` DISABLE KEYS */;

INSERT INTO `fuel_prices` (`type`, `price_per_gallon`)
VALUES
	('FULL',2.95),
	('ONE-DAY',3.7),
	('NON-FUELING',0);

/*!40000 ALTER TABLE `fuel_prices` ENABLE KEYS */;
UNLOCK TABLES;


# Dump of table item_prices
# ------------------------------------------------------------

DROP TABLE IF EXISTS `item_prices`;

CREATE TABLE `item_prices` (
  `item` varchar(64) NOT NULL,
  `price` float NOT NULL,
  `description` text,
  PRIMARY KEY (`item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

LOCK TABLES `item_prices` WRITE;
/*!40000 ALTER TABLE `item_prices` DISABLE KEYS */;

INSERT INTO `item_prices` (`item`, `price`, `description`)
VALUES
	('fuel',3.6,'Fuel price for a FULL membership.  Other memberships, and expired FULL memberships pay the surcharge'),
	('membership',100,'Full membership fee'),
	('renewal',30,'Renew an EXPIRED membership.  ONE-DAY memberships never expire!'),
	('trial_surcharge',0.75,'Per-gallon surcharge for EXPIRED and ONE-DAY memberships'),
	('upgrade',70,'Upgrade fee for a non-ONE-DAY membership to FULL.  ONE-DAY memberhip upgrade to FULL is the full membership price');

/*!40000 ALTER TABLE `item_prices` ENABLE KEYS */;
UNLOCK TABLES;


# Dump of table kiosk_alerts
# ------------------------------------------------------------

DROP TABLE IF EXISTS `kiosk_alerts`;

CREATE TABLE `kiosk_alerts` (
  `kiosk_alert_id` bigint(11) NOT NULL AUTO_INCREMENT,
  `timestamp` datetime NOT NULL,
  `received` datetime NOT NULL,
  `alert_id` varchar(64) NOT NULL,
  `kiosk_id` bigint(20) NOT NULL,
  `message` varchar(256) DEFAULT NULL,
  `status` enum('ACTIVE','HANDLED') NOT NULL DEFAULT 'ACTIVE',
  `notes` text,
  PRIMARY KEY (`kiosk_alert_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;



# Dump of table kiosk_request_log
# ------------------------------------------------------------

DROP TABLE IF EXISTS `kiosk_request_log`;

CREATE TABLE `kiosk_request_log` (
  `request_id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `timestamp` datetime NOT NULL,
  `kiosk_id` int(10) unsigned NOT NULL,
  `message_sha1` varchar(64) NOT NULL,
  `count` int(10) unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`request_id`),
  KEY `kiosk_id` (`kiosk_id`),
  KEY `message_sha1` (`message_sha1`),
  KEY `timestamp` (`timestamp`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;



# Dump of table kiosks
# ------------------------------------------------------------

DROP TABLE IF EXISTS `kiosks`;

CREATE TABLE `kiosks` (
  `kiosk_id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(256) NOT NULL,
  `rsa_key` varchar(1024) NOT NULL,
  `is_online` tinyint(1) DEFAULT NULL,
  `last_checkin` datetime NOT NULL,
  `last_status` datetime DEFAULT NULL,
  `checkin_delta_h` int(11) DEFAULT NULL,
  `last_ip` varchar(64) DEFAULT NULL,
  `status` varchar(256) NOT NULL,
  `voltage` float NOT NULL DEFAULT '12',
  `fuel_type` varchar(8) NOT NULL DEFAULT 'B99',
  `fuel_avail` float NOT NULL,
  `fuel_capacity` float NOT NULL,
  `fuel_value` float NOT NULL,
  `fuel_price` float NOT NULL DEFAULT '3.99',
  `address1` varchar(256) NOT NULL,
  `address2` varchar(256) NOT NULL,
  `city` varchar(256) NOT NULL,
  `state` varchar(256) NOT NULL,
  `zip` varchar(256) NOT NULL,
  `latitude` float NOT NULL,
  `longitude` float NOT NULL,
  `timezone` varchar(256) NOT NULL DEFAULT 'US/Eastern',
  PRIMARY KEY (`kiosk_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;



# Dump of table member_ccs
# ------------------------------------------------------------

DROP TABLE IF EXISTS `member_ccs`;

CREATE TABLE `member_ccs` (
  `cc_id` bigint(11) unsigned NOT NULL AUTO_INCREMENT,
  `cc_name` varchar(256) NOT NULL DEFAULT '',
  `member_id` bigint(20) DEFAULT NULL,
  `mag_name` varchar(256) NOT NULL DEFAULT '',
  `last4` varchar(256) DEFAULT NULL,
  PRIMARY KEY (`cc_id`),
  KEY `cc_name` (`cc_name`(255)),
  KEY `member_id` (`member_id`),
  KEY `mag_name` (`mag_name`(255))
) ENGINE=InnoDB DEFAULT CHARSET=utf8;



# Dump of table member_roles
# ------------------------------------------------------------

DROP TABLE IF EXISTS `member_roles`;

CREATE TABLE `member_roles` (
  `member_role` bigint(20) NOT NULL AUTO_INCREMENT,
  `member_id` bigint(20) NOT NULL,
  `role_id` varchar(64) NOT NULL,
  PRIMARY KEY (`member_role`),
  KEY `member_roles` (`member_id`,`role_id`),
  KEY `members` (`member_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

LOCK TABLES `member_roles` WRITE;
/*!40000 ALTER TABLE `member_roles` DISABLE KEYS */;

INSERT INTO `member_roles` (`member_role`, `member_id`, `role_id`)
VALUES
	(11,1,'All Purchases'),
	(37,1,'Broadcast Email'),
	(35,1,'Fuel Alerts'),
	(6,1,'Fuel Czar'),
	(1,1,'Fuel Deliveries'),
	(26,1,'Gateway Czar'),
	(25,1,'Kiosk Czar'),
	(2,1,'Memberships'),
	(27,1,'Software Czar'),
	(12,3,'All Purchases'),
	(40,3,'Broadcast Email'),
	(33,3,'Fuel Alerts'),
	(9,3,'Fuel Czar'),
	(8,3,'Fuel Deliveries'),
	(49,3,'Kiosk Czar'),
	(7,3,'Memberships'),
	(23,4,'All Purchases'),
	(41,4,'Broadcast Email'),
	(32,4,'Fuel Alerts'),
	(46,4,'Fuel Czar'),
	(15,4,'Fuel Deliveries'),
	(24,4,'Memberships'),
	(13,6,'All Purchases'),
	(19,16,'All Purchases'),
	(38,16,'Broadcast Email'),
	(34,16,'Fuel Alerts'),
	(20,16,'Fuel Czar'),
	(21,16,'Fuel Deliveries'),
	(22,16,'Memberships'),
	(42,174,'All Purchases'),
	(43,174,'Broadcast Email'),
	(44,174,'Fuel Alerts'),
	(14,174,'Fuel Czar'),
	(45,174,'Fuel Deliveries'),
	(48,174,'Kiosk Czar'),
	(47,174,'Memberships'),
	(36,203,'All Purchases'),
	(39,203,'Broadcast Email'),
	(30,218,'Fuel Alerts'),
	(31,218,'Supplier'),
	(28,219,'Fuel Alerts'),
	(29,219,'Supplier');

/*!40000 ALTER TABLE `member_roles` ENABLE KEYS */;
UNLOCK TABLES;


# Dump of table members
# ------------------------------------------------------------

DROP TABLE IF EXISTS `members`;

CREATE TABLE `members` (
  `member_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `membership_id` int(10) unsigned NOT NULL,
  `is_primary` tinyint(1) NOT NULL DEFAULT '1',
  `name` varchar(256) NOT NULL,
  `first_name` varchar(256) DEFAULT '',
  `last_name` varchar(256) DEFAULT '',
  `pin` varchar(256) NOT NULL,
  `email` varchar(256) NOT NULL,
  `address1` varchar(256) NOT NULL,
  `address2` varchar(256) DEFAULT NULL,
  `city` varchar(256) NOT NULL DEFAULT 'Baltimore',
  `state` varchar(256) NOT NULL DEFAULT 'MD',
  `zip` varchar(256) NOT NULL,
  `home_phone` varchar(256) DEFAULT NULL,
  `work_phone` varchar(256) DEFAULT NULL,
  `mobile_phone` varchar(256) DEFAULT NULL,
  `credit` float NOT NULL DEFAULT '0',
  `fuel_preauth` float NOT NULL DEFAULT '15',
  `login` varchar(256) NOT NULL,
  `password` varchar(256) NOT NULL,
  PRIMARY KEY (`member_id`),
  KEY `membership_id` (`membership_id`),
  KEY `first_name` (`first_name`(255)),
  KEY `last_name` (`last_name`(255)),
  KEY `pin` (`pin`(255)),
  KEY `login` (`login`(255))
) ENGINE=InnoDB DEFAULT CHARSET=utf8;



# Dump of table memberships
# ------------------------------------------------------------

DROP TABLE IF EXISTS `memberships`;

CREATE TABLE `memberships` (
  `membership_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `membership_number` int(11) NOT NULL,
  `start_date` datetime NOT NULL,
  `expires` datetime DEFAULT NULL,
  `type` enum('FULL','ONE-DAY','NON-FUELING','SUPPLIER') NOT NULL,
  `status` enum('ACTIVE','PENDING','ASK_RENEWAL','EXPIRED') NOT NULL,
  `last_renewal` datetime NOT NULL,
  PRIMARY KEY (`membership_id`),
  UNIQUE KEY `membership_number` (`membership_number`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;



# Dump of table messages
# ------------------------------------------------------------

DROP TABLE IF EXISTS `messages`;

CREATE TABLE `messages` (
  `message_id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `kiosk_id` int(11) unsigned NOT NULL,
  `timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `parameter` enum('avail_gallons','add_gallons','operator_code','patch','ppg','fuel','maint') NOT NULL,
  `value` varchar(256) NOT NULL,
  PRIMARY KEY (`message_id`),
  KEY `kiosk_id` (`kiosk_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;



# Dump of table notices
# ------------------------------------------------------------

DROP TABLE IF EXISTS `notices`;

CREATE TABLE `notices` (
  `notice_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `kiosk_id` int(11) NOT NULL,
  `timestamp` datetime NOT NULL,
  `message` varchar(256) NOT NULL,
  `received` datetime NOT NULL,
  PRIMARY KEY (`notice_id`),
  KEY `kiosk_id` (`kiosk_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;



# Dump of table promotions
# ------------------------------------------------------------

DROP TABLE IF EXISTS `promotions`;

CREATE TABLE `promotions` (
  `promotion_id` int(11) NOT NULL AUTO_INCREMENT,
  `item` varchar(64) NOT NULL,
  `price` float NOT NULL,
  `kiosk_id` int(11) DEFAULT NULL,
  `number_left` int(11) DEFAULT NULL,
  `date_start` datetime DEFAULT NULL,
  `date_end` datetime DEFAULT NULL,
  PRIMARY KEY (`promotion_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

LOCK TABLES `promotions` WRITE;
/*!40000 ALTER TABLE `promotions` DISABLE KEYS */;

INSERT INTO `promotions` (`promotion_id`, `item`, `price`, `kiosk_id`, `number_left`, `date_start`, `date_end`)
VALUES
	(1,'set_email_fuel_credit',10,NULL,NULL,NULL,NULL);

/*!40000 ALTER TABLE `promotions` ENABLE KEYS */;
UNLOCK TABLES;


# Dump of table roles
# ------------------------------------------------------------

DROP TABLE IF EXISTS `roles`;

CREATE TABLE `roles` (
  `role_id` varchar(64) NOT NULL,
  `label` varchar(256) DEFAULT NULL,
  `order` int(11) DEFAULT '0',
  `script` varchar(256) DEFAULT NULL,
  PRIMARY KEY (`role_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

LOCK TABLES `roles` WRITE;
/*!40000 ALTER TABLE `roles` DISABLE KEYS */;

INSERT INTO `roles` (`role_id`, `label`, `order`, `script`)
VALUES
	('All Purchases','',NULL,NULL),
	('Broadcast Email','Broadcast',3,'BBD-broadcast-email.pl'),
	('Fuel Alerts',NULL,NULL,NULL),
	('Fuel Czar','',NULL,NULL),
	('Fuel Deliveries','',1,'BBD-fuel_deliveries.pl'),
	('Gateway Czar','',NULL,NULL),
	('Kiosk Czar','',NULL,NULL),
	('Memberships','',2,'BBD-memberships.pl'),
	('Software Czar','',NULL,NULL),
	('Supplier','',NULL,NULL);

/*!40000 ALTER TABLE `roles` ENABLE KEYS */;
UNLOCK TABLES;


# Dump of table sales
# ------------------------------------------------------------

DROP TABLE IF EXISTS `sales`;

CREATE TABLE `sales` (
  `sale_id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `kiosk_id` int(10) unsigned NOT NULL,
  `member_id` int(10) unsigned NOT NULL,
  `timestamp` datetime NOT NULL,
  `item` varchar(64) NOT NULL,
  `quantity` float NOT NULL,
  `per_item` float NOT NULL DEFAULT '0',
  `amount` float NOT NULL,
  `max_flow` float DEFAULT NULL,
  `credit` float NOT NULL DEFAULT '0',
  `cc_trans_id` int(10) unsigned DEFAULT NULL,
  PRIMARY KEY (`sale_id`),
  KEY `kiosk_id` (`kiosk_id`),
  KEY `member_id` (`member_id`),
  KEY `timestamp` (`timestamp`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;



# Dump of table sessions
# ------------------------------------------------------------

DROP TABLE IF EXISTS `sessions`;

CREATE TABLE `sessions` (
  `id` char(32) NOT NULL,
  `a_session` text NOT NULL,
  UNIQUE KEY `id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;




/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;
/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
