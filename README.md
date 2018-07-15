## Brief
This is intended to be a daemon that manages a wifi interface of
a Linux based IoT device.

## Building

meson builddir
cd builddir
ninja

## Requirements

### Software
* meson (during build only)
* ninja (during build only)
* glib
* glib-json
* libmicrohttpd with TLS enabled
* libnl-route
* libnl-genl
* wpasupplicant

For buildroot an external package can be found [here](https://github.com/thingyjp/thingymcconfig-buildroot)

On Debian or similar you need these packages

```libmicrohttpd-dev libnl-route-3-dev libnl-genl-3-dev libjson-glib-dev```
 
### Hardware:
wifi interface that supports NL80211 and supports a station VIF(virtual interface) 
and an access point VIF simultaneously. This is apparently less
common than you might think. 

To check run:

```
iw list
```

Then look for a section that resembles the example below.
You need a line that shows you can have one or more managed VIFs
and one or more AP VIFs at the same time.
In the example you can see the second combination allows for
1 managed VIF and 1 AP VIF.


```
        valid interface combinations:
                 * #{ managed } <= 1, #{ P2P-device } <= 1, #{ P2P-client, P2P-GO } <= 1,
                   total <= 3, #channels <= 1
                 * #{ managed } <= 1, #{ AP } <= 1, #{ P2P-client } <= 1, #{ P2P-device } <= 1,
                   total <= 4, #channels <= 1
```

### Chipsets/Drivers that should work
#### Broadcom BCM43143/brcmfmac
Only tested the usb version so far (RaspberryPi official usb dongle).
#### Marvell 88w8801,88w8897/mwifiex
Only tested the usb version of the 88w8801 and the pci-e version of 88w8897
so far. Station and AP come up and operate correctly.
However setting a custom IE in the access point broadcasts doesn't seem to work.
 
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

## Application interface
Applications that want to be notified of connectivity changes
can do so by listening to a unix domain socket. Applications can
also use this socket to inform thingymcconfig of their end-to-end
connectivity state for use by clients in during provisioning.
This is useful in situations where a thing might have a working wifi
connection but the app(s) that implement the functionality can't
actually connect to required services. Application level errors etc
can be presented to the provisioning client so it can decide how to
proceed.

## Provisioning Protocol
The intention is to make this as simple as possible and for now
only one network can be configured at once.

Requests and responses are done over HTTPS(not implemented yet) and serialised as JSON.
These aren't really lightweight but good HTTP and JSON libraries 
exist for almost every platform.

The thing will present it's unique certificate so that you can
validate it's identity while the HTTPS connection is being made.
Your HTTP client should only trust the CA used to sign said certificate. 

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
curl -v "http://127.0.0.1:1338/scan"
```

### Configuring
#### Request
```json
{
  "ssid": "yourssid",
  "psk": "yourpassword"
}
```
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
    "config_state": "configured",
    "supplicant": {
      "connected": true
    },
    "dhcp4": {
      "state": "configured",
      "lease": {
        "ip": "192.168.2.146",
        "subnetmask": "255.255.255.0",
        "defaultgw": "192.168.2.1",
        "nameservers": [
          "192.168.2.1"
        ]
      }
    }
  }
}
```
#### curl
```
curl -v "http://127.0.0.1:1338/status"
```
