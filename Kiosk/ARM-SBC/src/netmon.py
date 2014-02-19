import os
import time

def try_n_secs (nsecs, function, *largs):
	now = time.time()
	ret = None
	while ret is None and time.time() - now < nsecs:
		try:
			ret = function (*largs)
		except:
			print ("fail!")
			time.sleep(nsecs/20)
	return ret

def USB_init():
	try:
		with try_n_secs (1,open,"/sys/bus/platform/drivers/reg-fixed-voltage/unbind", "a") as myfile:
			myfile.write("usb0_vbus.4")
		with try_n_secs (1,open,"/sys/class/gpio/export", "a") as myfile:
			myfile.write("17")
	except:
		pass
	with try_n_secs (1,open,"/sys/class/gpio/gpio17/direction", "a") as myfile:
		myfile.write("out")
	with try_n_secs (1,open,"/sys/class/gpio/gpio17/value", "a") as myfile:
		myfile.write("1")

def USB_power_off():
	with try_n_secs (1,open,"/sys/class/gpio/gpio17/value", "a") as myfile:
		myfile.write("0")

def USB_power_on():
	with try_n_secs (1,open,"/sys/class/gpio/gpio17/value", "a") as myfile:
		myfile.write("1")

def USB_release():
	# This leaves USB power off!!!
	with try_n_secs (1,open,"/sys/class/gpio/unexport", "a") as myfile:
		myfile.write("17")
	with try_n_secs (1,open,"/sys/bus/platform/drivers/reg-fixed-voltage/bind", "a") as myfile:
		myfile.write("usb0_vbus.4")

def connect_iface (iface):
	pass

subprocess.Popen(args, bufsize=0, executable=None, stdin=None, stdout=None, stderr=None, preexec_fn=None, close_fds=False, shell=False, cwd=None, env=None, universal_newlines=False, startupinfo=None, creationflags=0)


def check_iface_dns (dns_ip,hostname,iface):
	host_ip = None
	try:
		subprocess.check_call (["ip","route","add",dns_ip+"/32","dev",iface])
		host_ip = subprocess.check_output(["dig","@"+dns,hostname,"+short"])
	except:
		pass
	finally:
		try:
			subprocess.check_call (["ip","route","delete",dns_ip+"/32","dev",iface])
		except:
			pass
	return host_ip

def check_iface_http (host_ip,iface):
	head = None
	try:
		subprocess.check_call (["ip","route","add",host_ip+"/32","dev",iface])
		head = subprocess.check_output(["curl","-I",host_ip])
	except:
		pass
	finally:
		try:
			subprocess.check_call (["ip","route","delete",host_ip+"/32","dev",iface])
		except:
			pass
	return head


def connect_ppp():
	# wvdial & disown
	wvdial = subprocess.Popen("wvdial",
		cwd='/', close_fds = True,
	ppp0_ip = try_n_secs (30,subprocess.check_output,"ifconfig en0 | grep 'inet ' | awk '{print $2}'", shell=True)
	if ppp0_ip is None:
		wvdial.terminate()
		del wvdial # to avoid the zombie
		wvdial = None
	else:
		check_iface_http(host_ip,"ppp0")


def test_dhcp():
	dhcpcd -T eth0
	new_broadcast_address=10.0.1.255
# 	new_dhcp_lease_time=43200
# 	new_dhcp_message_type=5
# 	new_dhcp_rebinding_time=37800
# 	new_dhcp_renewal_time=21600
# 	new_dhcp_server_identifier=10.0.1.1
# 	new_domain_name=cathilya.org
# 	new_domain_name_servers=10.0.1.1
# 	new_host_name=BBD9000-06
# 	new_ip_address=10.0.1.169
# 	new_network_number=10.0.1.0
# 	new_routers=10.0.1.1
# 	new_subnet_cidr=24
# 	new_subnet_mask=255.255.255.0
	
	new_domain_name_servers # space-delimited
	new_ip_address
	new_subnet_mask
	new_routers
	ifconfig eth0 192.168.1.102 netmask 255.255.255.0 broadcast 192.168.1.255
	ifconfig eth0 new_ip_address netmask new_subnet_mask broadcast new_broadcast_address up
	ip route add check_host_ip via new_routers dev eth0
	
	

