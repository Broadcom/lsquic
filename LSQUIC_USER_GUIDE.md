# LSQUIC with QUIC Hardware Offload - User Guide

**Version**: 1.0
**Last Updated**: March 9, 2026

---

## Table of Contents

1. [Overview](#overview)
2. [Prerequisites](#prerequisites)
3. [Obtaining Source Code](#obtaining-source-code)
4. [Building BoringSSL](#building-boringssl)
5. [Building LSQUIC](#building-lsquic)
6. [Building BNXT Driver](#building-bnxt-driver)
7. [System Configuration](#system-configuration)
8. [Running HTTP Server](#running-http-server)
9. [Client Machine Setup](#client-machine-setup)
10. [Running HTTP Client](#running-http-client)
11. [Troubleshooting](#troubleshooting)
12. [Known Limitations](#known-limitations)
13. [Appendix](#appendix)

---

## Overview

### What is LSQUIC?

**LiteSpeed QUIC (LSQUIC)** is an open-source implementation of QUIC and HTTP/3 for servers and clients. It is used in production by LiteSpeed Web Server, LiteSpeed ADC, and OpenLiteSpeed.

**Official Documentation**: For complete LSQUIC library documentation, visit [https://lsquic.readthedocs.io/en/latest/](https://lsquic.readthedocs.io/en/latest/)

**Supported QUIC Versions**:
- ✅ QUIC v1 and v2 (RFC 9000, 9001, 9002) - **IETF QUIC**
- ✅ HTTP/3 (RFC 9114)
- ✅ Internet-Draft versions 27, 29
- ⚠️ Legacy Google QUIC: Q043, Q046, Q050 (software-only, no hardware offload)

**Source Code**: See [README.md](README.md) for the official LSQUIC GitHub repository and general information.

### QUIC Hardware Offload Integration

This guide shows how to build and run lsquic with **Broadcom BNXT_EN driver with QUIC hardware offload**.

**Hardware offload support**:
- ✅ **TX offload**: Hardware-accelerated packet encryption for IETF QUIC
- ✅ **IETF QUIC only**: v1, v2, and Internet-Drafts
- ❌ **Google QUIC**: Not supported (Q043, Q046, Q050 run in software only)

**📖 For detailed QUIC offload API documentation, see:**
- **[QUIC_OFFLOAD_USER_GUIDE.md](../nxt-linux-drivers/v3/quic/docs/QUIC_OFFLOAD_USER_GUIDE.md)** - Complete API reference, data structures, examples
- **[QUIC_API.md](../nxt-linux-drivers/v3/quic/docs/QUIC_API.md)** - Focused API documentation

### Operating Modes

This guide covers two modes:

1. **Software-only mode**: All crypto in software (baseline)
2. **TX offload mode**: Hardware-accelerated packet encryption (IETF QUIC only)

---

## Prerequisites

### Hardware Requirements

- **Network Adapter**:
  - BCM576xx (Thor2/P7) - **QUIC offload supported**

### Software Requirements

#### Operating System
- **Linux**: Kernel 5.15+
- **Distribution**: Ubuntu 22.04+, RHEL 8+, or equivalent

#### Build Tools
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    golang \
    zlib1g-dev \
    libevent-dev \
    linux-headers-$(uname -r)

# RHEL/CentOS
sudo yum groupinstall -y "Development Tools"
sudo yum install -y \
    cmake \
    git \
    golang \
    zlib-devel \
    libevent-devel \
    kernel-devel-$(uname -r)
```

### Network Configuration

- **Two machines recommended ** (server and client) connected via the BNXT NIC
- Direct connection or low-latency network between the two machines
- Static IP addresses configured using RFC 1918 private ranges (e.g., 192.168.1.0/24)
- Firewall rules allowing UDP ports (default: 4433+)

**IMPORTANT**: QUIC hardware offload requires packets to traverse the network interface. The server and client should be on separate machines (or at minimum, traffic must be routed through the BNXT NIC). If the kernel routes packets via loopback (e.g., both endpoints on the same machine using `localhost` or the same IP), packets bypass the NIC entirely and hardware offload will not operate.

---

## Obtaining Source Code

### Directory Structure

We'll organize everything under a single workspace:

```bash
# Create workspace
mkdir -p ~/projects
cd ~/projects
```

Final structure:
```
~/projects/
├── boringssl/          # SSL/TLS library
├── lsquic/             # QUIC implementation
└── nxt-linux-drivers/  # BNXT driver with offload
    └── v3/             # Driver version 3
```

### 1. Obtain BNXT_EN Driver

**Obtain the BNXT_EN driver package with QUIC offload support from Broadcom.**

The driver package provides hardware offload capabilities for QUIC 1-RTT packet encryption/decryption on Broadcom NetXtreme Ethernet adapters.

#### How to Obtain

Contact your Broadcom representative or access the driver through the appropriate Broadcom channels to obtain:
1. The BNXT_EN driver source code package
2. Compatible firmware updates (if needed)
3. Access to technical support and documentation

#### What's Included in the Driver Package

The BNXT_EN driver package includes:
- **Driver source code** with QUIC offload support (`bnxt_quic.c`, `bnxt_en.ko`)
- **User-space include files** (`usrinclude/linux/bnxt_quic_usr_include.h`)
- **Documentation**:
  - BNXT_EN driver release notes
  - [Broadcom NetXtreme QUIC Hardware Offload User Guide](../nxt-linux-drivers/v3/quic/docs/QUIC_OFFLOAD_USER_GUIDE.md)
  - Installation procedures
  - System requirements and dependencies
  - Firmware version requirements

#### Supported Hardware

The QUIC offload feature is supported on:
- **Broadcom NetXtreme-E Thor2 (P7 series)** - BCM576xx
- **Firmware Version**:
  - For QUIC offload: Firmware 2.38+ with QUIC offload support is recommended
  - Check firmware version: `ethtool -i <interface>`

#### Documentation References

For comprehensive information about the BNXT_EN driver and hardware capabilities:

**Official Broadcom Documentation** (Primary Reference):
- **[Broadcom Ethernet Network Adapter User Guide](https://techdocs.broadcom.com/us/en/storage-and-ethernet-connectivity/ethernet-nic-controllers/bcm957xxx/adapters.html)** - **OFFICIAL ONLINE DOCUMENTATION**
  - Complete driver installation and configuration guide
  - Hardware installation procedures
  - Driver software installation for Linux, VMware, and Windows
  - Configuration and tuning for optimum performance
  - Statistics gathering and monitoring
  - Utilities and tools reference
  - RoCE (RDMA over Converged Ethernet) setup
  - Troubleshooting and FAQ
  - Hardware and regulatory information
  
- **[Local Driver README](../nxt-linux-drivers/v3/README.TXT)** - Local copy of bnxt_en driver documentation
  - Driver compilation and installation instructions  
  - Driver settings and configuration options
  - Operating system compatibility and limitations
  - Firmware update procedures
  
**QUIC-Specific Offload Documentation**:
- **[Broadcom NetXtreme QUIC Hardware Offload User Guide](../nxt-linux-drivers/v3/quic/docs/QUIC_OFFLOAD_USER_GUIDE.md)** - QUIC offload API documentation
  - QUIC offload API reference and data structures
  - Dual key support and key updates
  - Complete C code examples
  - Performance optimization techniques
  - Security considerations

**Note**: The online Broadcom TechDocs provides the most up-to-date information. Always consult the [official web documentation](https://techdocs.broadcom.com/us/en/storage-and-ethernet-connectivity/ethernet-nic-controllers/bcm957xxx/adapters.html) for the latest driver features, compatibility, and best practices.

#### Next Steps

Once you have obtained the driver package:
1. Extract the driver source to `~/projects/nxt-linux-drivers/v3/`
2. Refer to the **BNXT_EN driver release notes** for specific installation instructions
3. Continue with the remaining source code steps and building sections below

### 2. Clone BoringSSL

```bash
cd ~/projects
git clone https://boringssl.googlesource.com/boringssl
cd boringssl
```

**Use specific tested version** (recommended):
```bash
git checkout 0.20250807.0
```

Or use latest main (may require adjustments):
```bash
git checkout main
```

### 3. Clone LSQUIC

```bash
cd ~/projects
git clone https://github.com/Broadcom/lsquic.git
cd lsquic
git submodule update --init
```

**Use latest Broadcom modifications to master**:
```bash
git checkout bcm/master
```

Or use tagged release version instead:
```bash
git checkout bcm/v1.0
```

---

## Building BoringSSL

### Step 1: Configure Build

```bash
cd ~/projects/boringssl
```

### Step 2: Build Library

**Production build** (optimized):
```bash
cmake -DCMAKE_BUILD_TYPE=Release .
make -j$(nproc)
```

### Step 3: Verify Build

```bash
ls -lh
# Should see: libssl.a, libcrypto.a (or .so if shared)
```

### Step 4: Note BoringSSL Path

```bash
export BORINGSSL=$PWD
echo "BORINGSSL path: $BORINGSSL"
# Add to ~/.bashrc for persistence:
echo "export BORINGSSL=$BORINGSSL" >> ~/.bashrc
```

**Expected output**:
```
BORINGSSL path: /home/username/projects/boringssl
```

---

## Building LSQUIC

### Step 1: Configure CMake

```bash
cd ~/projects/lsquic
```

**Build with QUIC offload support**:
```bash
cmake -DLIBSSL_DIR=$BORINGSSL .
```

### Step 2: Build Library and Examples

```bash
make -j$(nproc)
```

**Expected output**:
```
[ 10%] Building C object src/liblsquic/CMakeFiles/lsquic.dir/...
[ 50%] Building C object bin/CMakeFiles/http_client.dir/...
[ 75%] Building C object bin/CMakeFiles/http_server.dir/...
[100%] Built target http_server
```

### Step 3: Verify Build

```bash
# Check library
ls -lh src/liblsquic/liblsquic.a

# Check example binaries
ls -lh bin/http_server bin/http_client

# Check offload integration
ls -lh bin/http_offload.c bin/http_offload.h
```

### Step 4: Generate Test Certificates

```bash
cd bin

# Generate self-signed certificate
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout key.pem \
    -out cert.pem \
    -days 365 \
    -subj "/CN=localhost"

# Verify certificates
ls -lh cert.pem key.pem
```

### Understanding LSQUIC Offload Integration

The LSQUIC offload integration is implemented at the **application level** in the example programs (`bin/http_offload.c` and `bin/http_offload.h`), not within the core LSQUIC library. This provides a reference implementation for integrating QUIC hardware offload.

**Architecture**:

```
┌─────────────────────────────────────────────────────────────┐
│  LSQUIC Application (http_server, http_client)              │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  http_offload.c / http_offload.h                      │  │
│  │  - Session management (ctrl_fd)                       │  │
│  │  - Flow lifecycle (add/delete)                        │  │
│  │  - Key extraction from lsquic_conn_t                  │  │
│  │  - ioctl() calls to driver                            │  │
│  └───────────────┬───────────────────────────────────────┘  │
│                  │ Uses lsquic APIs                          │
│  ┌───────────────▼───────────────────────────────────────┐  │
│  │  lsquic library (src/liblsquic/)                      │  │
│  │  - QUIC protocol implementation                       │  │
│  │  - TLS handshake (via BoringSSL)                      │  │
│  │  - Packet encryption/decryption (software fallback)   │  │
│  │  - Provides key access APIs                           │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────┼───────────────────────────────────────────┘
                  │ ioctl(SIOCDEVQUIC*)
┌─────────────────▼───────────────────────────────────────────┐
│  BNXT_EN Driver (Kernel Module)                             │
│  - bnxt_quic.c: QUIC offload manager                        │
│  - Session and flow management                              │
│  - Hardware programming (MPC crypto engine)                 │
└─────────────────┬───────────────────────────────────────────┘
                  │ Hardware commands
┌─────────────────▼───────────────────────────────────────────┐
│  NetXtreme Hardware (BCM576xx)                              │
│  - MPC crypto engine (AES-GCM)                              │
│  - Header protection                                         │
└─────────────────────────────────────────────────────────────┘
```

**Key Features**:
- ✅ Manages offload flow lifecycle (add/delete/flush)
- ✅ Extracts cryptographic keys from QUIC handshake
- ✅ Configures hardware flows for each connection
- ✅ Handles CID validation (hardware requires 0 or 8-byte CIDs)
- ✅ Automatic fallback to software if offload unavailable

**How It Works**:
1. Application starts with `-O <direction>` and `-s IP:PORT`
2. Integration layer calls flow add ioctl to create offload flow
3. When QUIC connection reaches 1-RTT state:
   - Extracts keys from TLS session
   - Calls `SIOCDEVQUICFLOWADD ioctl to offload flow to hardware
4. Hardware encrypts packets automatically
5. On connection close, integration layer updates hardware

**Cipher Support**:
- AES-128-GCM (most common)
- AES-256-GCM
- Hardware validation ensures compatibility

**Code Location**:
```
lsquic/bin/
├── http_offload.c    # Offload integration implementation
├── http_offload.h    # Offload interface definitions
├── http_server.c     # Uses offload via -O and -s IP:PORT
└── http_client.c     # Client application
```

For detailed API information and advanced offload usage, refer to [QUIC_OFFLOAD_USER_GUIDE.md](../nxt-linux-drivers/v3/quic/docs/QUIC_OFFLOAD_USER_GUIDE.md).

---

## Building BNXT_EN Driver

This section covers building the BNXT_EN driver with QUIC offload support from source.

**For complete and authoritative build instructions, refer to:**
- **[Broadcom Ethernet Network Adapter User Guide - Installing Software](https://techdocs.broadcom.com/us/en/storage-and-ethernet-connectivity/ethernet-nic-controllers/bcm957xxx/adapters.html)** - Official online documentation
- **[Local Driver README](../nxt-linux-drivers/v3/README.TXT)** - Section "BNXT_EN Driver Compilation"

### Prerequisites

Before building the driver, ensure you have:

**Required Packages:**
- **Kernel headers and build infrastructure**: The driver build depends on installed kernel headers
  ```bash
  # Ubuntu/Debian
  sudo apt-get install linux-headers-$(uname -r)
  
  # RHEL/CentOS  
  sudo yum install kernel-devel-$(uname -r) kernel-headers-$(uname -r)
  ```

- **Build tools**: GCC 4.9 or later (required by modern kernels), make, awk, sed, grep
  ```bash
  # Ubuntu/Debian
  sudo apt-get install build-essential
  
  # RHEL/CentOS
  sudo yum groupinstall "Development Tools"
  ```

- **Root/sudo access** for driver loading

**Important Notes:**
- As noted in the official driver README, the driver should compile on any contemporary Linux distributions
- For a complete list of supported Operating Systems, consult the **Broadcom Ethernet Network Adapter User Guide**
- Kernel 5.15 or later is required for QUIC offload support

### Step 1: Navigate to Driver Source

```bash
cd ~/projects/nxt-linux-drivers/v3/
```

### Step 2: Build the Driver

The driver uses Linux's standard Kbuild infrastructure:

```bash
# Build the driver module
make
```

**For multi-core systems (faster build)**:
```bash
make -j$(nproc)
```

**Expected output**:
```
CC [M]  /path/to/nxt-linux-drivers/v3/bnxt.o
CC [M]  /path/to/nxt-linux-drivers/v3/bnxt_quic.o
...
LD [M]  /path/to/nxt-linux-drivers/v3/bnxt_en.ko
```

### Step 3: Verify Build Artifacts

```bash
# Check that the driver module was built
ls -lh bnxt_en.ko

# Check module information
modinfo ./bnxt_en.ko | head -15
```

**Expected output**:
```
-rw-r--r-- 1 user user 2.5M Jan 14 10:30 bnxt_en.ko
filename:       /path/to/nxt-linux-drivers/v3/bnxt_en.ko
version:        1.10.3
license:        GPL
description:    Broadcom BCM573xx network driver
...
```

### Step 4: Install the Driver (Optional)

To install the driver module into the system:

```bash
sudo make install
```

**Note**: On some distributions, the `make install` step automatically triggers initrd rebuild via the INSTALLKERNEL Kbuild hook. If not, you may need to rebuild initrd manually:

```bash
# RHEL/CentOS
sudo dracut --force

# Ubuntu/Debian  
sudo update-initramfs -u
```

### Advanced Build Options

**Build for specific kernel version**:
```bash
KVER=<version> make
```

**Build with custom kernel headers location**:
```bash
KDIR=<path> make
```

### Troubleshooting Build Issues

**Issue: Missing kernel headers**
```bash
# Verify kernel headers are installed
ls -l /lib/modules/$(uname -r)/build
# Should show a valid symlink to kernel build directory

# Ubuntu/Debian
sudo apt-get install linux-headers-$(uname -r)

# RHEL/CentOS
sudo yum install kernel-devel-$(uname -r)
```

**Issue: Build errors due to toolchain version**
- Modern kernels require GCC 4.9 or later
- Ensure you're using the latest stable version of the toolchain
- Refer to: https://www.kernel.org/doc/html/latest/process/changes.html

**Issue: Build errors**
```bash
# Clean and rebuild
make clean
make

# Check kernel version compatibility
uname -r
# Driver requires kernel 5.15 or later for QUIC offload
```

**Issue: Module version mismatch**
```bash
# Ensure kernel headers match running kernel
uname -r
ls /lib/modules/$(uname -r)/build
# Both should match

# If they don't match, install correct headers or reboot into matching kernel
```

### Verifying Build Integrity (Optional)

The driver package includes a MANIFEST file for verifying file integrity:

```bash
cd ~/projects/nxt-linux-drivers/v3/
sha512sum -c MANIFEST
```

**Note**: This verifies files haven't been modified since packaging, useful for debugging.

### Additional Information

For complete and authoritative build instructions, refer to:
- **[Broadcom Ethernet Network Adapter User Guide - Installing Software for Linux](https://techdocs.broadcom.com/us/en/storage-and-ethernet-connectivity/ethernet-nic-controllers/bcm957xxx/adapters.html)** - Official online documentation with latest information
- **[Local Driver README](../nxt-linux-drivers/v3/README.TXT)** - Section "BNXT_EN Driver Compilation"
- Official Kbuild documentation: https://www.kernel.org/doc/html/latest/kbuild/modules.html

---

## System Configuration

### Step 1: Load BNXT_EN Driver

After building the driver, you need to load it into the kernel.

#### Load Required Dependencies

**IMPORTANT**: Several kernel modules must be loaded before `bnxt_en.ko`. These dependencies are documented in the [Broadcom Ethernet Network Adapter User Guide](https://techdocs.broadcom.com/us/en/storage-and-ethernet-connectivity/ethernet-nic-controllers/bcm957xxx/adapters.html) and [local driver README](../nxt-linux-drivers/v3/README.TXT) - Section "BNXT_EN Driver Dependencies".

**Required dependencies (kernel 5.15 or higher)**:
```bash
# 1. Load TLS module (required for bnxt_en driver to load)
# CONFIG_TLS and CONFIG_TLS_DEVICE must be enabled as module options
sudo modprobe tls

# Verify TLS module loaded
lsmod | grep tls
```

**Note on TLS module**: LSQUIC does not use the kernel TLS module — it performs all TLS 1.3 handshake and QUIC packet encryption/decryption in userspace via BoringSSL. However, the `bnxt_en` driver also supports kTLS offload, which depends on the kernel `tls` module (`net/tls.h`). The `tls` module must be loaded before `bnxt_en.ko` or the driver will fail to load.

**Additional dependencies (if applicable)**:
```bash
# 2. Load hwmon module (if CONFIG_HWMON_MODULE is set as module option)
sudo modprobe hwmon

# 3. Load devlink module (kernel 4.6+ or certain backported kernels like RHEL 3.10)
# If CONFIG_NET_DEVLINK is set as module option
sudo modprobe devlink

# 4. Load vxlan module (kernels older than 4.7, if CONFIG_VLAN_MODULE is set)
sudo modprobe vxlan
```

**Verify dependencies loaded**:
```bash
lsmod | grep -E "tls|hwmon|devlink"
```

**Note**: The `tls` kernel module **must** be loaded before `bnxt_en.ko` because the driver depends on it for its TLS offload support. See the [Broadcom Ethernet Network Adapter User Guide](https://techdocs.broadcom.com/us/en/storage-and-ethernet-connectivity/ethernet-nic-controllers/bcm957xxx/adapters.html) for complete dependency information.

#### Unload Existing Driver (if any)

```bash
# Check if old driver is loaded
lsmod | grep bnxt_en

# Unload existing driver (if present)
sudo rmmod bnxt_en 2>/dev/null || true

# Verify removal
lsmod | grep bnxt_en  # Should return nothing
```

#### Load New Driver with QUIC Offload Support

```bash
cd ~/projects/nxt-linux-drivers/v3

# Load the newly built driver
sudo insmod ./bnxt_en.ko

# Verify driver loaded successfully
lsmod | grep bnxt_en
```

**Expected output**:
```
bnxt_en               245760  0
```

#### Verify Driver Installation

```bash
# Check kernel messages for successful load
dmesg | tail -20 | grep -i bnxt

# Check driver version
modinfo bnxt_en | grep version
```

**Expected dmesg output**:
```
[12345.678] bnxt_en 0000:xx:00.0 eth0: Broadcom BCM576xx...
```

#### Troubleshooting Driver Load Issues

**Issue: Module not found**
```bash
# Verify the .ko file exists
ls -lh ~/projects/nxt-linux-drivers/v3/bnxt_en.ko

# Try with full path
sudo insmod ~/projects/nxt-linux-drivers/v3/bnxt_en.ko
```

**Issue: TLS module dependency error**
```bash
# Ensure TLS module is loaded first
sudo modprobe tls
lsmod | grep tls

# Then load bnxt_en
sudo insmod ./bnxt_en.ko
```

**Issue: Version magic mismatch**
```bash
# Rebuild driver against current kernel
make clean
make -j$(nproc)
sudo insmod ./bnxt_en.ko
```

**Issue: Device busy / already in use**
```bash
# Force remove old driver
sudo rmmod -f bnxt_en

# If still fails, check what's using it
lsof | grep bnxt
ps aux | grep bnxt

# Reboot if necessary, then load new driver
```

### Step 2: Configure Network Interface

After loading the driver, configure the network interface for use with LSQUIC.

**IMPORTANT**: The IP address must be assigned to the BNXT network interface, and the server and client should be on separate machines so that packets travel through the NIC. QUIC hardware offload only operates on packets that are transmitted out the network interface — packets routed via the kernel loopback path bypass the NIC and will not be offloaded.

#### Set Environment Variables

```bash
# Server machine configuration
export SERVER_IP="192.168.1.10"      # Your server IP
export IFACE="eno16795np0"             # Your interface name

# Save to ~/.bashrc for persistence
echo "export SERVER_IP=\"192.168.1.10\"" >> ~/.bashrc
echo "export IFACE=\"eno16795np0\"" >> ~/.bashrc
```

#### Configure IP Address

```bash
# Bring interface down (if up)
sudo ip link set $IFACE down

# Set IP address
sudo ifconfig $IFACE $SERVER_IP up

# Alternative using ip command
sudo ip addr add $SERVER_IP/24 dev $IFACE
sudo ip link set $IFACE up
```

#### Verify Network Configuration

```bash
# Check interface status
ip addr show $IFACE

# Verify IP address assigned
ifconfig $IFACE | grep inet

# Test interface is up
ip link show $IFACE | grep "state UP"

# Test loopback connectivity
ping -c 4 $SERVER_IP
```

**Expected output**:
```
inet 192.168.1.10  netmask 255.255.255.0  broadcast 192.168.1.255
        inet 192.168.1.10/24 scope global eno16795np0
64 bytes from 192.168.1.10: icmp_seq=1 ttl=64 time=0.043 ms
```

#### Advanced Configuration (Optional)

**Set MTU for jumbo frames**:
```bash
sudo ip link set $IFACE mtu 9000
ip link show $IFACE | grep mtu
```

**Configure static route** (if needed):
```bash
sudo ip route add 192.168.1.0/24 dev $IFACE
```

#### Check Hardware Capabilities

```bash
# Check driver information
ethtool -i $IFACE

# Check statistics (for QUIC offload monitoring)
ethtool -S $IFACE | grep quic
```

**Expected output**:
```
driver: bnxt_en
version: 1.10.3-238.0.1.2
firmware-version: 238.0.1.2/pkg 238.0.1.2
```

#### Troubleshooting Network Configuration

**Issue: Interface not found**
```bash
# List all interfaces
ip link show

# Check if driver recognized hardware
lspci | grep -i broadcom
dmesg | grep -i bnxt | grep -i "eth\|eno\|enp"
```

**Issue: Cannot set IP address**
```bash
# Check if interface is managed by NetworkManager
nmcli device status

# Disable NetworkManager for this interface
sudo nmcli device set $IFACE managed no

# Or configure via NetworkManager
sudo nmcli connection modify $IFACE ipv4.addresses $SERVER_IP/24
sudo nmcli connection up $IFACE
```

**Issue: Interface keeps going down**
```bash
# Check link status
ethtool $IFACE | grep "Link detected"

# Check cable connection
# Check switch port configuration
# Check for firmware issues in dmesg
dmesg | grep -i $IFACE | tail -20
```

**Issue: Cannot ping self**
```bash
# Check firewall rules
sudo iptables -L | grep DROP

# Check routing table
ip route show

# Verify ARP table
arp -a
```

---

## Running HTTP Server

### Software-Only Mode (Baseline)

```bash
cd ~/projects/lsquic/bin

./http_server \
    -s ${SERVER_IP}:4433 \
    -c localhost,./cert.pem,./key.pem \
    -y 0 \
    -o ql_bits=0 \
    -g \
    -S sndbuf=2000000000 \
    -S rcvbuf=1000000000 \
    -m 250 \
    -L notice
```

**Parameter explanation**:
- `-s <ip:port>`: Server bind address and port
- `-c <host,cert,key>`: TLS certificate configuration
- `-y 0`: Use latest QUIC version
- `-o ql_bits=0`: Disable greasing (cleaner logs)
- `-g`: Enable logging
- `-S sndbuf/rcvbuf`: Socket buffer sizes
- `-m 250`: Max concurrent connections per process
- `-L notice`: Log level (notice, info, debug)

**Expected output**:
```
[NOTICE] Document root is not set: start in Interop Mode
```

### TX Offload Mode (Hardware Acceleration)

```bash
cd ~/projects/lsquic/bin

./http_server \
    -s ${SERVER_IP}:4433 \
    -c localhost,./cert.pem,./key.pem \
    -y 0 \
    -o ql_bits=0 \
    -g \
    -S sndbuf=2000000000 \
    -S rcvbuf=1000000000 \
    -m 250 \
    -L notice \
    -O tx
```

**Additional parameters for TX offload**:
- `-O tx`: Enable TX offload
- `-F`: (Optional) Flush all stale QUIC offload flows on startup — see below

**Expected output**:
```
[NOTICE] Document root is not set: start in Interop Mode
```

### Flushing Stale QUIC Offload Flows

The BNXT_EN driver can leak hardware resources if an application exits without properly deleting all offloaded QUIC flows (e.g., crash, `kill -9`, or application bug). These stale flows remain programmed in hardware and consume resources until the driver is unloaded or the device is closed.

To address this, the driver provides a flush ioctl (`SIOCDEVQUICFLOWFLUSH`) that removes **all** QUIC offload flows from the hardware. The `-F` flag on `http_server` invokes this flush at startup, ensuring a clean slate before the server begins accepting connections.

**Usage**:
```bash
./http_server \
    -s ${SERVER_IP}:4433 \
    -c localhost,./cert.pem,./key.pem \
    -y 0 -o ql_bits=0 -g \
    -L notice \
    -O tx \
    -F
```

**When to use `-F`**:
- After a previous server instance crashed or was killed without clean shutdown
- When `ethtool -S $IFACE | grep quic_active_flows` shows non-zero but no server is running
- During development and testing where flows may not always be properly cleaned up
- As a general safety measure in scripts that restart the server

**When NOT to use `-F`**:
- When other applications on the same machine are actively using QUIC offload on the same interface — the flush removes **all** QUIC flows on the device, not just those belonging to the application issuing the flush

**What happens**:
1. Server opens an ioctl socket
2. Before starting the event loop, issues `SIOCDEVQUICFLOWFLUSH` on the bound interface
3. Driver removes all QUIC flows (both TX and RX, all key phases) from hardware
4. Server starts normally with a clean hardware state

**Verifying stale flows exist** (before flushing):
```bash
# Check if there are orphaned flows
ethtool -S $IFACE | grep quic_active_flows
# If non-zero and no QUIC server is running, these are stale flows
```

### Verifying Server is Running

```bash
# Check process
ps aux | grep http_server

# Check listening port
sudo netstat -ulnp | grep 4433

# Check offload session (only when active TX offload session)
sudo lsof | grep anon_inode | grep http_server
```

### Verifying QUIC Hardware Offload

Use `ethtool -S` to view the driver's QUIC offload statistics. These counters show whether flows are being offloaded to hardware and whether the hardware is encrypting packets.

**View all QUIC stats**:
```bash
ethtool -S $IFACE | grep quic
```

**Example output — TX offload active** (during or after a data transfer):
```
     quic_active_flows: 1
     quic_tx_add: 1
     quic_tx_del: 0
     quic_rx_add: 0
     quic_rx_del: 0
     quic_err_add_flow: 0
     quic_err_del_flow: 0
     quic_err_duplicate_flow: 0
     quic_err_no_mem: 0
     quic_err_key_ctx_alloc: 0
     quic_err_crypto_cmd: 0
     quic_err_filter_alloc: 0
     quic_err_invalid_param: 0
     quic_err_device_busy: 0
     quic_err_flow_not_found: 0
     quic_tx_hw_pkt: 1494752
     quic_tx_lookup_flow_miss: 0
     quic_tx_lookup_key_phase_miss: 0
     quic_rx_hw_pkt: 0
     quic_rx_payload_decrypted: 0
     quic_rx_hdr_decrypted: 0
     quic_rx_key_phase_mismatch: 0
```

**How to tell offload is working**:
- `quic_tx_add` > 0 — at least one TX flow was offloaded to hardware
- `quic_tx_hw_pkt` > 0 and increasing — hardware is actively encrypting packets
- `quic_active_flows` > 0 — flows are currently offloaded
- All `quic_err_*` counters are 0 — no errors during offload setup

**Example output — software-only mode** (no `-O tx` flag, or offload not configured):
```
     quic_active_flows: 0
     quic_tx_add: 0
     quic_tx_del: 0
     quic_rx_add: 0
     quic_rx_del: 0
     quic_err_add_flow: 0
     quic_err_del_flow: 0
     quic_err_duplicate_flow: 0
     quic_err_no_mem: 0
     quic_err_key_ctx_alloc: 0
     quic_err_crypto_cmd: 0
     quic_err_filter_alloc: 0
     quic_err_invalid_param: 0
     quic_err_device_busy: 0
     quic_err_flow_not_found: 0
     quic_tx_hw_pkt: 0
     quic_tx_lookup_flow_miss: 0
     quic_tx_lookup_key_phase_miss: 0
     quic_rx_hw_pkt: 0
     quic_rx_payload_decrypted: 0
     quic_rx_hdr_decrypted: 0
     quic_rx_key_phase_mismatch: 0
```

When running in software-only mode, all QUIC counters remain at 0 because no flows are programmed into the hardware.

**Counter reference**:

| Counter | Description |
|---------|-------------|
| `quic_active_flows` | Number of flows currently offloaded to hardware |
| `quic_tx_add` | Total TX flows successfully added |
| `quic_tx_del` | Total TX flows deleted |
| `quic_rx_add` | Total RX flows added (not used in current integration) |
| `quic_rx_del` | Total RX flows deleted |
| `quic_tx_hw_pkt` | Packets encrypted by hardware (key indicator of offload) |
| `quic_tx_lookup_flow_miss` | TX packets that missed flow lookup (fell back to software) |
| `quic_tx_lookup_key_phase_miss` | TX packets where key phase didn't match installed key |
| `quic_err_add_flow` | Errors adding a flow |
| `quic_err_del_flow` | Errors deleting a flow |
| `quic_err_duplicate_flow` | Attempted to add an already-existing flow |
| `quic_err_no_mem` | Flow add failed due to memory allocation |
| `quic_err_key_ctx_alloc` | Failed to allocate hardware key context |
| `quic_err_crypto_cmd` | Hardware crypto command failed |
| `quic_err_filter_alloc` | Failed to allocate hardware filter |
| `quic_err_invalid_param` | Invalid parameters passed to driver |
| `quic_err_device_busy` | Device was busy, operation rejected |
| `quic_err_flow_not_found` | Tried to delete a flow that doesn't exist |
| `quic_rx_hw_pkt` | Packets decrypted by hardware (RX offload) |
| `quic_rx_payload_decrypted` | RX payloads successfully decrypted |
| `quic_rx_hdr_decrypted` | RX headers successfully decrypted |
| `quic_rx_key_phase_mismatch` | RX packets where key phase didn't match |

---

## Client Machine Setup

If you're running the client on a different machine, you'll need to set up the driver and network interface.

### Step 1: Load BNXT_EN Driver

```bash
cd ~/projects/nxt-linux-drivers/v3

# Load TLS module (required dependency)
sudo modprobe tls

# Unload existing driver (if any)
sudo rmmod bnxt_en 2>/dev/null || true

# Load new driver with offload support
sudo insmod ./bnxt_en.ko

# Verify driver loaded
lsmod | grep bnxt_en
dmesg | tail -20 | grep -i bnxt
```

### Step 2: Configure Client Interface

```bash
# On client machine
export CLIENT_IP="192.168.1.11"  # Different IP from server
export SERVER_IP="192.168.1.10"  # Server's IP address
export IFACE="eno16795np0"         # Client's interface name

# Set IP address
sudo ifconfig $IFACE $CLIENT_IP up

# Verify interface
ip addr show $IFACE
```

### Step 3: Verify Connectivity

```bash
# Ping server from client
ping -c 4 $SERVER_IP

# If ping fails, check:
# - Both machines on same subnet
# - Network cables connected
# - Firewall rules allow ICMP and UDP
```

---

## Running HTTP Client

### Basic Request

```bash
cd ~/projects/lsquic/bin

./http_client \
    -s ${SERVER_IP}:4433 \
    -H localhost \
    -p /2147483648 \
    -t \
    -y 0 \
    -o ql_bits=0 \
    -K
```

**Parameter explanation**:
- `-s <ip:port>`: Server address to connect to
- `-H <host>`: Host header value
- `-p <path>`: Path to request (server will generate data of this size)
- `-y 0`: No timestamp
- `-o ql_bits=0`: Disable greasing
- `-K`: Discard server response
- `-t`: Print stats to stdout

**Expected output**:
```
overall statistics as calculated by ./http_client:
time for connect: n: 1; min: 2.87 ms; max: 2.87 ms; mean: 2.87 ms; sd: 0.00 ms
time for request: n: 1; min: 4033.71 ms; max: 4033.71 ms; mean: 4033.71 ms; sd: 0.00 ms
time to 1st byte: n: 1; min: 0.76 ms; max: 0.76 ms; mean: 0.76 ms; sd: 0.00 ms
downloaded 2147483742 application bytes in 4.038 seconds
0.25 reqs/sec; 531861202 bytes/sec
read handler count 34617
Aggregate connection stats collected by engine:
Connections: 1
Ticks: 34619
In:
    Total bytes: 2200248401
    packets: 1494798
    undecryptable packets: 0
    duplicate packets: 0
    error packets: 0
    STREAM frame count: 1494752
    STREAM payload size: 2147915067
    Header bytes: 0; uncompressed: 0; ratio 0.000
    ACK frames: 2570
    ACK frames processed: 2568
    ACK frames merged: 2
Out:
    Total bytes: 1285972
    packets: 33345
    acked via loss record: 0
    acks: 33341
    retx packets: 0
    STREAM frame count: 4
    STREAM payload size: 50
    Header bytes: 0; uncompressed: 0; ratio 0.000
    ACKs: 33341
```

### Performance Test (Multiple Connections)

For load testing with multiple concurrent connections:

```bash
./http_client \
    -s ${SERVER_IP}:4433 \
    -R 100 \
    -r 100 \
    -w 10 \
    -p /2147483648 \
    -H localhost \
    -t \
    -y 0 \
    -n 1000 \
    -K \
    -o ql_bits=0 \
    -g \
    -S sndbuf=100000000 \
    -S rcvbuf=200000000
```

**Additional parameters for load testing**:
- `-R <num>`: Max concurrent requests
- `-r <num>`: Max requests per connection
- `-w <num>`: Worker threads
- `-n <num>`: Total number of requests

**Key metrics**:
- **Throughput**: Data transfer rate (bytes/sec)
- **Failed requests**: Should be 0 (if not, check logs)
- **Time**: Total test duration

---

## Troubleshooting

### Issue 1: Server Fails to Start

**Symptoms**:
```
Error: Cannot bind to address ${SERVER_IP}:4433
```

**Causes & Solutions**:

1. **Port already in use**:
   ```bash
   sudo netstat -ulnp | grep 4433
   # Kill conflicting process or use different port
   ```

2. **Interface not configured**:
   ```bash
   sudo ifconfig $IFACE $SERVER_IP up
   ip addr show $IFACE  # Verify
   ```

3. **Firewall blocking port**:
   ```bash
   sudo ufw allow 4433/udp
   # Or for firewalld:
   sudo firewall-cmd --add-port=4433/udp --permanent
   sudo firewall-cmd --reload
   ```

### Issue 2: Offload Not Working

**Symptoms**:
```
Failed to open QUIC session on eno16795np0: Operation not supported
```

**Diagnosis**:
```bash
# 1. Check driver is loaded
lsmod | grep bnxt_en

# 2. Check driver version
modinfo bnxt_en | grep version

# 3. Check kernel logs
dmesg | tail -50 | grep -i "quic\|bnxt"
```

**Solutions**:

1. **TLS module not loaded**:
   ```bash
   # Load TLS module first
   sudo modprobe tls
   lsmod | grep tls
   ```

2. **Driver not loaded**:
   ```bash
   cd ~/projects/nxt-linux-drivers/v3
   sudo modprobe tls  # Required dependency
   sudo insmod ./bnxt_en.ko
   ```

3. **Firmware doesn't support offload**:
   ```bash
   ethtool -i $IFACE
   # Check firmware version, may need update
   ```

### Issue 3: Low Performance with Offload

**Symptoms**: TX offload enabled but performance similar to software-only.

**Diagnosis** (see [Verifying QUIC Hardware Offload](#verifying-quic-hardware-offload) for full counter reference):
```bash
# Check all QUIC offload stats
ethtool -S $IFACE | grep quic

# Key indicators:
# - quic_tx_add should be non-zero (flows were offloaded)
# - quic_tx_hw_pkt should be increasing during data transfer
# - quic_err_* counters should all be 0
```

**Common Causes**:

1. **CID length incompatible**:
   - lsquic default CID length is 8 bytes (compatible)
   - Check logs for: "Cannot offload: CID lengths not supported"
   
2. **Socket buffers too small**:
   ```bash
   sudo sysctl -w net.core.rmem_max=2147483647
   sudo sysctl -w net.core.wmem_max=2147483647
   ```

**Advanced Diagnostics**:
```bash
# Enable driver debug logging
echo "module bnxt_en +p" | sudo tee /sys/kernel/debug/dynamic_debug/control

# Check dmesg for offload activity
dmesg -w | grep -i quic

# Look for:
# - "QUIC flow added" (successful offload)
# - "KEY_PHASE_MISMATCH" (dual key issue)
# - "CRYPTO_CMD failed" (hardware issue)
```

**For complete troubleshooting guide**, see:
- [QUIC_OFFLOAD_USER_GUIDE.md - Troubleshooting](../nxt-linux-drivers/v3/quic/docs/QUIC_OFFLOAD_USER_GUIDE.md#troubleshooting)
- [QUIC_API.md - Error Codes](../nxt-linux-drivers/v3/quic/docs/QUIC_API.md#error-codes)

### Issue 4: Client Cannot Connect

**Symptoms**:
```
Connection to ${SERVER_IP}:4433 failed: Connection timeout
```

**Diagnosis**:
```bash
# 1. Check server is running
ssh server "ps aux | grep http_server"

# 2. Check network connectivity
ping $SERVER_IP

# 3. Check UDP connectivity
nc -u -v $SERVER_IP 4433

# 4. Traceroute
traceroute $SERVER_IP
```

**Solutions**:

1. **Server not running**: Start server
2. **Wrong IP/port**: Verify `-s` parameter
3. **Firewall blocking**: Open UDP port 4433
4. **Network issue**: Check cables, switches, routing

### Issue 5: Build Failures

**lsquic build fails**:
```bash
# Missing BoringSSL
export BORINGSSL=$HOME/projects/boringssl
cmake -DLIBSSL_DIR=$BORINGSSL .

# Missing dependencies
sudo apt-get install -y zlib1g-dev libevent-dev
```

**bnxt_en build fails**:
```bash
# Missing kernel headers
sudo apt-get install linux-headers-$(uname -r)

# Wrong kernel version
cd /lib/modules/$(uname -r)/build
ls -la  # Should show kernel build directory
```

### Issue 6: Certificate Errors

**Symptoms**:
```
Error: Cannot load certificate from cert.pem
```

**Solution**:
```bash
cd ~/projects/lsquic/bin

# Generate new certificate
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout key.pem \
    -out cert.pem \
    -days 365 \
    -subj "/CN=localhost"

# Verify format
openssl x509 -in cert.pem -text -noout
openssl rsa -in key.pem -check
```

---

## Known Limitations

The current LSQUIC integration with QUIC hardware offload has the following known limitations:

### 1. GSO (Generic Segmentation Offload) Not Supported

**Description**: The LSQUIC application does not currently support Generic Segmentation Offload (GSO) when using QUIC hardware offload.

**Impact**:
- Packets are sent individually rather than in batches
- May result in slightly higher CPU overhead for packet transmission
- Does not affect encryption offload functionality

**Workaround**: None required. TX offload still provides significant performance benefits even without GSO.

**Status**: GSO support may be added in future releases.

### 2. Dual Key Phase Offload Not Fully Integrated

**Description**: The dual key phase offload feature (for seamless QUIC key updates) is not fully integrated into the LSQUIC application at this time.

**Impact**:
- QUIC key updates may require brief software-mode processing
- Hardware offload temporarily falls back to software during key rotation
- Key updates are infrequent in typical QUIC connections, so impact is minimal

**Background**: The QUIC protocol supports key updates during a connection to enhance security. The hardware supports installing two key phases simultaneously (key_phase 0 and key_phase 1) to enable seamless key updates without packet loss. However, the LSQUIC integration code does not yet fully utilize this dual key capability.

**Workaround**: No action required. Key updates will still function correctly in software mode.

**Status**: Full dual key phase support integration is planned for future releases.

### 3. RX Offload Not Integrated

**Description**: RX (receive/decryption) offload is not integrated into the LSQUIC application at this time.

**Impact**:
- Only TX (transmit/encryption) offload is available
- Incoming packets are decrypted in software
- Asymmetric performance: uploads benefit from offload, downloads do not

**Workaround**: None. Use TX offload mode with `-O tx` flag.

**Status**: RX offload integration is under consideration for future releases.

---

## Appendix

### A. Command Reference

#### Server Commands

```bash
# Software-only
./http_server \
    -s <ip:port> \
    -c localhost,cert.pem,key.pem \
    -y 0 -o ql_bits=0 -g \
    -S sndbuf=2000000000 -S rcvbuf=1000000000 \
    -m 250 -L notice

# TX offload
./http_server \
    -s <ip:port> \
    -c localhost,cert.pem,key.pem \
    -y 0 -o ql_bits=0 -g \
    -S sndbuf=2000000000 -S rcvbuf=1000000000 \
    -m 250 -L notice \
    -O tx

# TX offload with flush on startup (clean up stale flows)
./http_server \
    -s <ip:port> \
    -c localhost,cert.pem,key.pem \
    -y 0 -o ql_bits=0 -g \
    -S sndbuf=2000000000 -S rcvbuf=1000000000 \
    -m 250 -L notice \
    -O tx -F
```

#### Client Commands

```bash
# Basic request
./http_client \
    -s <ip:port> \
    -p /<size> \
    -H localhost \
    -t -y 0 -o ql_bits=0 -K

# Performance test (high concurrency)
./http_client \
    -s <ip:port> \
    -R 100 -r 100 -w 10 \
    -p /<size> \
    -H localhost \
    -t -y 0 -n 1000 -K \
    -o ql_bits=0 -g \
    -S sndbuf=100000000 -S rcvbuf=200000000
```

### B. Directory Layout

```
~/projects/
├── boringssl/
│   ├── build/
│   │   ├── libssl.a
│   │   └── libcrypto.a
│   └── include/
│
├── lsquic/
│   ├── src/
│   │   └── liblsquic/
│   │       └── liblsquic.a
│   └── bin/
│       ├── http_server
│       ├── http_client
│       ├── http_offload.c
│       ├── http_offload.h
│       ├── cert.pem
│       └── key.pem
│
└── nxt-linux-drivers/
    └── v3/
        ├── bnxt_en.ko
        ├── bnxt_quic.c
        ├── usrinclude/
        │   └── linux/
        │       └── bnxt_quic_usr_include.h
        └── quic/                        # QUIC offload documentation
            └── docs/                    # Core documentation
                ├── QUIC_API.md
                ├── QUIC_OFFLOAD_USER_GUIDE.md
                └── ...
```

### C. Environment Variables

**For Build (Both Server and Client)**:
```bash
# Add to ~/.bashrc for persistence
export BORINGSSL=$HOME/projects/boringssl
export LSQUIC=$HOME/projects/lsquic
export BNXT_DRIVER=$HOME/projects/nxt-linux-drivers/v3
```

**For Server Machine**:
```bash
# Server-specific configuration
export SERVER_IP="192.168.1.10"
export IFACE="eno16795np0"
```

**For Client Machine**:
```bash
# Client-specific configuration
export SERVER_IP="192.168.1.10"  # IP of the server machine
export CLIENT_IP="192.168.1.11"  # IP of the client machine
export IFACE="eno16795np0"
```

### D. Troubleshooting Checklist

Before asking for help, verify:

- [ ] BCM576xx (Thor2/P7) hardware - **P5 NOT supported**
- [ ] Using IETF QUIC (v1, v2, or I-D) - **Google QUIC NOT supported**
- [ ] BoringSSL built successfully
- [ ] LSQUIC built
- [ ] BNXT driver loaded (`lsmod | grep bnxt_en`)
- [ ] Interface configured (`ip addr show eno16795np0`)
- [ ] Certificates generated (`ls -l cert.pem key.pem`)
- [ ] Server started without errors
- [ ] Network connectivity (`ping <server_ip>`)
- [ ] UDP port open (`sudo netstat -ulnp | grep 4433`)
- [ ] Offload session created
- [ ] No errors in `dmesg` or `ethtool -S`

### E. Related Documentation

**LSQUIC Official Documentation**:
- **[LSQUIC Documentation](https://lsquic.readthedocs.io/en/latest/)** - Complete LSQUIC library documentation
- **[LSQUIC README.md](README.md)** - GitHub repository information and quick start
- **[LSQUIC GitHub Repository](https://github.com/litespeedtech/lsquic)** - Source code and examples
- **LSQUIC Examples**: See `lsquic/EXAMPLES.txt` for additional usage examples
- **BoringSSL**: https://boringssl.googlesource.com/boringssl/

**Broadcom Driver Documentation** (Primary References):
- **[Broadcom Ethernet Network Adapter User Guide](https://techdocs.broadcom.com/us/en/storage-and-ethernet-connectivity/ethernet-nic-controllers/bcm957xxx/adapters.html)** - **PRIMARY ONLINE REFERENCE**
  - Installing Software for Ethernet Network Adapters (Linux, VMware, Windows)
  - Configuring Ethernet Network Adapters
  - Tuning for improved performance
  - Gathering Statistics (ethtool -S, hwmon)
  - Ethernet Network Adapter Utilities
  - RoCE (RDMA over Converged Ethernet) setup
  - Precision Time Protocol (PTP)
  - OVS and TC offload
  - Frequently Asked Questions and troubleshooting
  
- **[Local Driver README](../nxt-linux-drivers/v3/README.TXT)** - Local bnxt_en driver documentation
  - Driver compilation and installation
  - Operating system compatibility and limitations  
  - Driver settings and configuration (ethtool, devlink, etc.)
  - Hardware capabilities (SR-IOV, DCB, PTP, etc.)
  - Firmware update procedures
  - Statistics and monitoring
  - Complete troubleshooting guide
  
**QUIC Hardware Offload Documentation**:
- **[QUIC_OFFLOAD_USER_GUIDE.md](../nxt-linux-drivers/v3/quic/docs/QUIC_OFFLOAD_USER_GUIDE.md)** - QUIC offload API guide
  - API reference and data structures
  - Dual key support and key updates
  - Complete C code examples
  - Troubleshooting QUIC offload issues
- **[QUIC_API.md](../nxt-linux-drivers/v3/quic/docs/QUIC_API.md)** - Focused API documentation
  - ioctl commands reference
  - Error codes
  - Quick examples

**Note**: Always consult the [official Broadcom TechDocs website](https://techdocs.broadcom.com/us/en/storage-and-ethernet-connectivity/ethernet-nic-controllers/bcm957xxx/adapters.html) for the most up-to-date driver documentation, compatibility information, and best practices.

### F. Revision History

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2026-01-14 | Draft - build and run guide for IETF QUIC TX offload |

---

## Quick Start Summary

### For the Impatient

```bash
# 1. Build everything
cd ~/projects
git clone https://boringssl.googlesource.com/boringssl && cd boringssl && cmake . && make -j$(nproc)
export BORINGSSL=$PWD
cd ~/projects
git clone https://github.com/Broadcom/lsquic.git && cd lsquic
cmake -DBORINGSSL_DIR=$BORINGSSL -DBNXT_EN_USRINCLUDE_DIR=~/projects/nxt-linux-drivers/v3/usrinclude .
make -j$(nproc)
cd bin && openssl req -x509 -newkey rsa:2048 -nodes -keyout key.pem -out cert.pem -days 365 -subj "/CN=localhost"

# 2. Setup server machine
cd ~/projects/nxt-linux-drivers/v3
sudo modprobe tls
sudo insmod ./bnxt_en.ko
export SERVER_IP="192.168.1.10"
export IFACE="eno16795np0"
sudo ifconfig $IFACE $SERVER_IP up

# 3. Run server (TX offload, flush stale flows on startup)
cd ~/projects/lsquic/bin
./http_server -s ${SERVER_IP}:4433 -c localhost,./cert.pem,./key.pem \
    -y 0 -o ql_bits=0 -g \
    -L notice -O tx -F

# 4. Setup client machine (if different from server)
cd ~/projects/nxt-linux-drivers/v3
sudo modprobe tls
sudo insmod ./bnxt_en.ko
export CLIENT_IP="192.168.1.11"
export SERVER_IP="192.168.1.10"
export IFACE="eno16795np0"
sudo ifconfig $IFACE $CLIENT_IP up
ping -c 4 $SERVER_IP  # Verify connectivity

# 5. Run client (from client machine)
cd ~/projects/lsquic/bin
./http_client -s ${SERVER_IP}:4433 -H localhost -p /2147483648 -t -y 0 -o ql_bits=0 -K

# Done! Check results.
```

---

**End of LSQUIC User Guide**

*For questions or issues, see [Troubleshooting](#troubleshooting) section or related documentation.*

