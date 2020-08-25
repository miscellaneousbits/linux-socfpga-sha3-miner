# DE10 NANO SHA3 FPGA Miner

![FPGA mining](doc/miner.png)

Want to support this open source project? Please star it. Want to contribute?
Feel free to create pull requests following the normal github process.

## Table of Contents

* [Introduction](#introduction)
* [Implementation](#implementation)
* [User API](#user-api)
* [Installing](#installing)
* [Disclaimer](#disclaimer)

## Introduction

In mining a proof-of-work (POW) is used to verify the authenticity of a blockchain entry. What is a POW?
A POW is a mathematical puzzle which is difficul to solve but easy to verify.

For this example an FPGA mining core is defined for a hypothetical blockchain that uses the SHA3-256 hash.

We are given:

- H: 256 bit header of fixed value
- N: 64 bit nonce
- D: 256 bit difficulty

The problem we need to solve is to find any value of N, such that the SHA3-256 hash of the nonce concatenated
to the header, is less than or equal to the difficulty:

D >= SHA3({H, N})

NOTE: This will not mine a real blockchain. It would be well suited to mine the ETC chain as proposed
in [ECIP-1049](https://github.com/ethereumclassic/ECIPs/issues/13)

## Implementation

A bootable SD card image for the
[DE10 NANO](https://software.intel.com/content/www/us/en/develop/topics/iot/hardware/fpga-de10-nano.html),
a low cost FPGA development board equipped with an Intel 5CSEBA6U23I7NDK FPGA. The FPGA includes a dual
core ARM hard processor. Ubuntu 16.04 is booted and supports the pre-installed FPGA image availlable
[here](https://github.com/miscellaneousbits/DE10_NANO_SOC_MINIMAL.git) and kernel driver support.

In order to provide the maximum FPGA routing resources to the mining core, HDMI, keyboard, and mouse
support have been removed. You can log in to the console either via the USB UART or SSH.

## User API

The driver models the 23 32-bit miner core registers as file /dev/miner0. It supports these file operations.

- open
- close
- lseek (must be to a 4 byte boundary)
- read (must be for a length of 4 bytes)
- write (must be for a length of 4 bytes)
- poll/ppoll/select (wait for solution)

Only single register reads and writes are supported.

### File map

| Offset | Name | Read/Write | Description|
| --- | --- | --- | --- |
| 0 | SOLN_REG | RO | 64-bit Solution |
| 8 | STATUS_REG | RO | 32-bit Status (see below) |
| 12 | SHA3_REG | RO | 32-bit Fingerprint "SHA3" |
| 16 | HDR_REG | RW | 256-bit Header |
| 48 | DIFF_REG | RW | 256-bit Difficulty |
| 80 | START_REG | RW | 64-bit Start nonce |
| 88 | CTL_REG | RW | 32-bit Control (see below) |

### Status register

| Bit # | Name | Description |
| --- | --- | --- |
| 0 | FOUND | Solution found. Solution is stored and IRQ is set. IRQ cleared with next ctl. reg. read. |
| 1 | RUNNING | The run ctl bit is set and the solution nonce is auto-incrementing |
| 2 | TESTING | The test ctl bit is set and compare diff equal |
| 15-8 | MINER_FREQ | Miner clock frequency in MHZ |
| 19-16 | MAJ_VER | Miner core major version |
| 23-20 | MIN_VER | Miner core minor version |

### Control register

| Bit # | Name | Description |
| --- | --- | --- |
| 0 | RUN | 0 - clear, 1 - auto increment the solution nonce and check hashes |
| 1 | TEST | 0 - normal mode, 1 - test mode, look for exact match with diff |
| 2 | HALT | 0 - normal mode, 1 - halt mining and raise interrupt |
| 23-16 | PAD_LAST | last pad byte, 0x80 for KECCACK-256 and SHA3-256 |
| 31-24 | PAD_FIRST | first pad byte, 0x01 for KECCACK-256, and 0x06 for SHA3-256 |

## Installing

### Prerequisites

![FPGA mining](doc/nano.png)

- DE10-NANO
- Desk/Laptop (Windows or Linux Ubuntu/Debian) to run terminal.

### Download and install

Download the latest release version from this project. Using dd on Linux, or Win32DiskImager on Windows,
unzip and copy the image file a to an 8GB or greater SD card. Insert the SD card in the DE10-NANO card slot,
and power up. You can log in as user **root** and password **miner**.

The first thing to do is to expand the root partition. Making sure you note the partition
start sector.

```
de10nano login: root
Password: 
Last login: Fri Jul 24 20:51:03 UTC 2020 on ttyS0
root@de10nano:~# fdisk /dev/mmcblk0

Welcome to fdisk (util-linux 2.27.1).
Changes will remain in memory only, until you decide to write them.
Be careful before using the write command.


Command (m for help): p
Disk /dev/mmcblk0: 119.4 GiB, 128177930240 bytes, 250347520 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0xbdf7f996

Device         Boot  Start     End Sectors  Size Id Type
/dev/mmcblk0p1        2048  206848  204801  100M  b W95 FAT32
/dev/mmcblk0p2      227331 5309243 5081913  2.4G 83 Linux <<== Make note of partition two'2 start sector
/dev/mmcblk0p3      206849  227330   20482   10M a2 unknown

Partition table entries are not in disk order.

Command (m for help): d
Partition number (1-3, default 3): 2

Partition 2 has been deleted.

Command (m for help): n
Partition type
   p   primary (2 primary, 0 extended, 2 free)
   e   extended (container for logical partitions)
Select (default p): 
Partition number (2,4, default 2): 
First sector (227331-250347519, default 229376): 227331 <<== Use the same partition start address as noted above
Last sector, +sectors or +size{K,M,G,T,P} (227331-250347519, default 250347519): 

Created a new partition 2 of type 'Linux' and of size 119.3 GiB.

Command (m for help): w
The partition table has been altered.
Calling ioctl() to re-read partition table.
Re-reading the partition table failed.: Device or resource busy

The kernel still uses the old table. The new table will be used at the next reboot or after you run partprobe(8) or kpa.

root@de10nano:~# sync; reboot
```

Now that the partition is expanded, resize the root file system.


```
de10nano login: root
Password: 
Last login: Thu Feb 11 16:28:12 UTC 2016 on ttyS0
root@de10nano:~# lsblk
NAME        MAJ:MIN RM   SIZE RO TYPE MOUNTPOINT
mmcblk0     179:0    0 119.4G  0 disk 
├─mmcblk0p2 179:2    0 119.3G  0 part /
├─mmcblk0p3 179:3    0    10M  0 part 
└─mmcblk0p1 179:1    0   100M  0 part 

root@de10nano:~# resize2fs /dev/mmcblk0p2
resize2fs 1.42.13 (17-May-2015)
Filesystem at /dev/mmcblk0p2 is mounted on /; on-line resizing required
old_desc_blocks = 1, new_desc_blocks = 8
The filesystem on /dev/mmcblk0p2 is now 31265023 (4k) blocks long.

root@de10nano:~# df -h
Filesystem      Size  Used Avail Use% Mounted on
/dev/root       118G  2.4G  111G   3% /
devtmpfs        503M     0  503M   0% /dev
tmpfs           504M     0  504M   0% /dev/shm
tmpfs           504M  6.7M  497M   2% /run
tmpfs           5.0M     0  5.0M   0% /run/lock
tmpfs           504M     0  504M   0% /sys/fs/cgroup
tmpfs           101M     0  101M   0% /run/user/0

root@de10nano:~# sync; reboot
```

You can now build and run the miner core test

```
root@de10nano:~# cd linux-socfpga-sha3-miner/tools/testing/miner/
root@de10nano:~/linux-socfpga-sha3-miner/tools/testing/miner# make
main.c -> main.o
sha3.c -> sha3.o
main.o sha3.o -> test
root@de10nano:~/linux-socfpga-sha3-miner/tools/testing/miner# ./test

Permutation stage test

Test 0 407e0758049d0b43 ................................................
Test 1 582acaed0a5b2461 ................................................
Test 2 783c26f63f572ed8 ................................................
Test 3 6b4d3be558640236 ................................................
Test 4 100e19fc5bb454bc ................................................
Test 5 472bec212ebc9746 ................................................
Test 6 4a4ceb022c632c02 ................................................
Test 7 4769389949d7c4e6 ................................................
Test 8 402ecfda45b559ed ................................................
Test 9 263cc2ad13c68104 ................................................
PASS

Interrupt test

Test 0 1c39c47e0a343594 ................................................
Test 1 5774c4a41d654b74 ................................................
Test 2 38cd2c903d4f61a8 ................................................
Test 3 64ba1fdb75250e7d ................................................
Test 4 230a37d027d8fc18 ................................................
Test 5 7fa75b5279c17342 ................................................
Test 6 44141d410d935089 ................................................
Test 7 7f86870c3fe0fa5d ................................................
Test 8 273b032b331bfb8e ................................................
Test 9 56c541f173231eb6 ................................................
PASS

Hash rate test

Miner clock 93 MHz, pipeline stages 8, expected hash rate 31.00 MH/S
Measured 31.00 MH/S
PASS

Mining test

Search 1
Header:     8c70c521f02d6c0d2e5bf63c580e19596006ee44a2f78b59639f37221457f31f
Start:      59dc07574dc801a2
Solution:   59dc075750f39248 (53,186,726 hashes)
Hash:       0000000189ffbac8ce8d11e655a700ab78892d379227becac54fdbed98f2fa78
Difficulty: 00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 2
Header:     10836d3238fe0265629bdc49c237032a06715a29310aab5d4a3d5821a7990a7c
Start:      5ad3f8a135ac2f9a
Solution:   5ad3f8a1841dbcc8 (1,316,064,558 hashes)
Hash:       000000054f0279237c0cc086e2c95afd231514925567f0d9b1df63092870defb
Difficulty: 00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 3
Header:     d6a1cd543ab6591d6a2d5d465ed979148e7c0874d3b960027da0f201df2aa114
Start:      6cb423f27e56b7ae
Solution:   6cb423f283995212 (88,250,980 hashes)
Hash:       00000003c031045e1117b61c13d9689e5ba4e69d4b70cf33e6b1ced8f1ae585c
Difficulty: 00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 4
Header:     aac179487e94790e9ee5c20bd81c7005d7a29267feebb0507b14fc5e3a42ca09
Start:      70a4431238d81bd2
Solution:   70a44312bf88d8c6 (2,259,729,652 hashes)
Hash:       000000068019e27f3aecb15ab5ba7b8d44c5ba8ab913a21502c647358703fb68
Difficulty: 00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 5
Header:     dc43925723c611230a1adb1d3fdf6e21e5fd144d108b354770e9197f303b6d6e
Start:      434024b759ede211
Solution:   434024b76391f6cc (161,748,155 hashes)
Hash:       00000000ea972c480a4386347bdb9765f847c7d3fe0434d875737b4ce616da1f
Difficulty: 00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 6
Header:     ca6a19248dc60d184b9847773498766aec9f872cd914506b0752d76c69407a2e
Start:      7ff13fb9598b75fa
Solution:   7ff13fb95f36fb91 (95,126,935 hashes)
Hash:       00000002e97e6b6c814c85a4bf38dcac30a07c69893ab7e88cada26fc774f853
Difficulty: 00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 7
Header:     17f8d02c63016b48780a0568b5dd93383c1edb4d4fad974fb3c94409b732d72c
Start:      5961ef8979e90cc6
Solution:   5961ef898edca07b (351,507,381 hashes)
Hash:       00000006acce4539f97d0610957b7e93d3b8aecbe9d4933a034f89e4b8815926
Difficulty: 00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 8
Header:     894eaf656633f430e9d2fa1c93688a03a5126352ced00f6aa4f3bf4a15fc7c51
Start:      587d0bfe0e00185b
Solution:   587d0bfe1c25ecc0 (237,360,229 hashes)
Hash:       000000016283025bc9db671f42549f1ce8d9468a147567446d8634af91a07e73
Difficulty: 00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 9
Header:     26de6a2bc876967ce9de0d267176b222fc0e0d67d57e95524a8b020e0461e453
Start:      010fbf3e0df3cb03
Solution:   010fbf3e28c7f27a (450,111,351 hashes)
Hash:       00000007c8f5966cf709f935618e5ee7f1530d721cb287e5bd2233db03306549
Difficulty: 00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 10
Header:     fed66f2d55b7e02d67cc5e5676e174150a957466a3ea3924c68e0c65bd5eb96f
Start:      51111d5a3e6e7e4f
Solution:   51111d5a7ec004d0 (1,079,084,673 hashes)
Hash:       000000007d6548b93febbe7811c7b454e9db4a8ab995e3b76a20a133f202e502
Difficulty: 00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

root@de10nano:~/linux-socfpga-sha3-miner/tools/testing/miner# 
```

You can also rebuild the kernel (it will take a very long time on the NANO)

```
root@de10nano:~# cd linux-socfpga-sha3-miner/
root@de10nano:~/linux-socfpga-sha3-miner# make zImage modules
  CHK     include/config/kernel.release
  CHK     include/generated/uapi/linux/version.h
  CHK     include/generated/utsrelease.h
  CHK     include/generated/bounds.h
  CHK     include/generated/timeconst.h
  CHK     include/generated/asm-offsets.h
  CALL    scripts/checksyscalls.sh
  CHK     scripts/mod/devicetable-offsets.h
  CHK     include/generated/compile.h
  CHK     kernel/config_data.h

...

  AS      arch/arm/boot/compressed/piggy.o
  LD      arch/arm/boot/compressed/vmlinux
  OBJCOPY arch/arm/boot/zImage
  Kernel: arch/arm/boot/zImage is ready
root@de10nano:~/linux-socfpga-sha3-miner# make modules_install 
  INSTALL crypto/drbg.ko
  INSTALL crypto/echainiv.ko
  INSTALL crypto/hmac.ko
  INSTALL crypto/jitterentropy_rng.ko
  INSTALL crypto/sha256_generic.ko
  INSTALL drivers/char/hw_random/rng-core.ko
  INSTALL drivers/dma/dmatest.ko
  INSTALL drivers/i2c/algos/i2c-algo-bit.ko
  INSTALL drivers/misc/miner/miner.ko
  INSTALL drivers/net/ethernet/altera/altera_tse.ko
  INSTALL drivers/net/ethernet/intel/e1000e/e1000e.ko
  INSTALL drivers/net/ethernet/intel/igb/igb.ko
  INSTALL drivers/net/ethernet/intel/ixgbe/ixgbe.ko
  INSTALL drivers/net/mdio.ko
  DEPMOD  4.14.130-ltsi+
root@de10nano:~/linux-socfpga-sha3-miner# mount /dev/mmcblk0p1 /media
root@de10nano:~/linux-socfpga-sha3-miner# cp arch/arm/boot/zImage /media
root@de10nano:~/linux-socfpga-sha3-miner# umount /media
root@de10nano:~/linux-socfpga-sha3-miner# sync
root@de10nano:~/linux-socfpga-sha3-miner# reboot
```

## Disclaimer

This software is provided "AS IS" and any expressed or implied warranties, including, but not limited to, the implied warranties of merchantability and fitness for a particular purpose are disclaimed. In no event shall the regents or contributors be liable for any direct, indirect, incidental, special, exemplary, or consequential damages (including, but not limited to, procurement of substitute goods or services; loss of use, data, or profits; or business interruption) however caused and on any theory of liability, whether in contract, strict liability, or tort (including negligence or otherwise) arising in any way out of the use of this software, even if advised of the possibility of such damage.  
