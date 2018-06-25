## Brief
This is intended to be a daemon that manages a wifi interface of
a Linux based IoT device.

## Requirements

### Software
* libmicrohttpd with TLS enabled
* ISC dhclient and dhcpd
* wpasupplicant

On Debian or similar you need these packages

```libmicrohttpd-dev libnl-route-3-dev libnl-genl-3-dev libjson-glib-dev```
 
### Hardware:
wifi interface that supports NL80211 and supports a station VIF 
and an access point VIF simultaneously. This is apparently less
common than you might think. 

### Chipsets/Drivers that should work
#### Marvell 88w8897/mwifiex
Only tested the pci-e version so far but station and ap interfaces
come up and operate.
 
### Chipsets/Drivers that will not work (yet)
#### Realtek rtl8188eu and variants
Could potentially work with the Realtek driver and their weird 
concurrent interface mode but would require lots of work. The
mac80211 driver doesn't seem to work yet.
 
#### Ralink rt3071/rt2800usb
Driver supports nl80211 but can't have a station and an ap up 
at the same time.

#### Qualcomm qca9377/ath10k
ath10k driver and chipset supports all the functionality required
but usb support is experimental and doesn't seem to work. pci-e and SDIO
might work.

## Protocol
 
### Scanning

### Configuring
```curl -H "Content-Type: application/json" '{"ssid":"mynetwork", "psk":"mypassword"}' -v "http://127.0.0.1:1338/config"```

### Status
