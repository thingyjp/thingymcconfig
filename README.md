## Brief
This is intended to be a daemon that manages a wifi interface of
a Linux based IoT device.

## Requirements

### Software
* libmicrohttpd with TLS enabled
* ISC dhclient and dhcpd
* wpasupplicant

For buildroot an external package can be found [here](https://github.com/thingyjp/thingymcconfig-buildroot)

On Debian or similar you need these packages

```libmicrohttpd-dev libnl-route-3-dev libnl-genl-3-dev libjson-glib-dev```
 
### Hardware:
wifi interface that supports NL80211 and supports a station VIF 
and an access point VIF simultaneously. This is apparently less
common than you might think. 

### Chipsets/Drivers that should work
#### Broadcom BCM43143/brcmfmac
Only tested the usb version so far (RaspberryPi official usb dongle).
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
The intention is to make this as simple as possible and for now
only one network can be configured at once.

Any client app should use the "scan" endpoint to get a list of
access points the thing itself can see instead of using any local
data.

Once the user has choosen one of the access points the client app
should send over the network details via the "config" endpoint and
then periodically poll the status end point to check if the process
has completed or failed.

### Scanning
#### Request
#### Response
```json
{
  "scanresults": [
    {
      "bssid": "??:??:??:??:??:??",
      "frequency": 2412,
      "rssi": -36,
      "ssid": "funkytown"
    }
  ]
}
```
#### curl
```
curl -H -v "http://127.0.0.1:1338/scan"
```

### Configuring
#### Request
#### Response
#### curl
```
curl -H "Content-Type: application/json" -d '{"ssid":"mynetwork", "psk":"mypassword"}' -v "http://127.0.0.1:1338/config"
```

### Status Polling
#### Request
#### Response
```json
{
  "network": {
    "supplicant": {
      "connected": true
    }
  }
}
```
#### curl
```
curl -H -v "http://127.0.0.1:1338/status"
```
