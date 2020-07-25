# DE10 NANO SHA3 FPGA Miner

![FPGA mining](doc/miner.png)

Want to support this open source project? Please star it. Want to contribute?
Feel free to create pull requests following the normal github process.

## Table of Contents

* [Introduction](#introduction)
* [Implementation](#implementation)
* [Miner Component](#miner_component)
	* [Block diagram](#block_diagram)
	* [API](#api)
* [Building](#building)
   * [Prerequisites](#prerequisites)
   * [Synthesizing](#synthesizing)
* [Sample](#sample)
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

### 

| Offset | Name | Read/Write | Description|
| --- | --- | --- | --- |
| 0 | SOLN_REG | RO | 64-bit Solution |
| 8 | STATUS_REG | RO | Status (see below) |
| 12 | SHA3_REG | RO | Fingerprint "SHA3" |
| 16 | HDR_REG | RW | 256-bit Header |
| 48 | DIFF_REG | RW | 256-bit difficulty |
| 80 | START_REG | RW | 64-bit start nonce |
| 88 | CTL_REG | RW | Control (see below) |

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

###

Download the latest release version from this project. Using dd on Linux, or Win32DiskImager on Windows,
unzip and copy the image file a to an 8GB or greater SD card. Insert the SD card in the DE10-NANO card slot,
and power up. You can log in as user **root** and password **miner**.

The first thing to do is to expand the root partition. Making sure to preserve the partition
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
/dev/mmcblk0p2      227331 5309243 5081913  2.4G 83 Linux
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
First sector (227331-250347519, default 229376): 227331
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

Poll test
Test 0 74ca3ec86fa25661 ................................................
Test 1 6f5642e74a21e28f ................................................
Test 2 6d2659381dee0639 ................................................
Test 3 6fa338755bffa732 ................................................
Test 4 24a1b7906abba54a ................................................
Test 5 6f77ce271fa701d9 ................................................
Test 6 175bc03d3d2b68f2 ................................................
Test 7 33e2d627394721b0 ................................................
Test 8 7665bd8753190b14 ................................................
Test 9 4691edf16c45b1fd ................................................
PASS

Interrupt test
Test 0 236bb7b726be242d ................................................
Test 1 5b633aab725bbb74 ................................................
Test 2 79acec670c1d4cc2 ................................................
Test 3 1521cb656d8d5b5e ................................................
Test 4 5f2d12a64b9fbc27 ................................................
Test 5 74b7a0237e862e5b ................................................
Test 6 1ff74cdb4dbaf8e7 ................................................
Test 7 302f36e64b088204 ................................................
Test 8 0240b6ed6e2f1ed2 ................................................
Test 9 52cf71d57e894aa4 ................................................
PASS

Checking hash rate
Miner clock 75 MHz, expected hash rate 25.00 MH/S
Measured 25.00 MH/S, PASS

Mining test

Search 1
Header:   8150713f419fb66da280291b578c055ee10c655cec84976467fce53e92868b4a
Start:    4ae05d471390d6b5
Solution: 4ae05d472cced817 (423,493,986 hashes)
Hash:     00000004dc38f15b623668635210dfa0442717f26eaa8487d8ef6e694b6d6156
Diff:     00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 2
Header:   ce62eb3e49cf3441702f2032e0c7de0a98eb71680d8a30178500df29f2e0e540
Start:    727a0dc963a405e2
Solution: 727a0dc96619ddfd (41,277,467 hashes)
Hash:     0000000338a2178db07c452ada806e8dfa22fe27028093c8e2376fce627965ea
Diff:     00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 3
Header:   7b4c8d7b3b9ad9055c0b59106229885ac6922b4fa5b37a7a338e7140f7a8f345
Start:    1b8b47cd65c6eefb
Solution: 1b8b47cd7b84951f (364,750,372 hashes)
Hash:     00000005760592ee77e5036326c56be64a47844cd4f8d7432c0d6db4651a8dfc
Diff:     00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 4
Header:   df54805e4e98fc5a3c8e7d5381d5a979a52402391d9be22f6d5a415e0c21e877
Start:    7a6e21af2921b7b5
Solution: 7a6e21af545379e7 (724,681,266 hashes)
Hash:     00000005622849ef443a44cd01ab76012a1d65c96b5be2e2230e6a153021c08d
Diff:     00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 5
Header:   c1f7780b7e845939fe86566a3227993d5e4c38449672c8523fb1c954e44c176e
Start:    13ae53884743bf08
Solution: 13ae5388483e613b (16,425,523 hashes)
Hash:     00000006d0dbac6100ea844585aa3cebfef16ed5083d29502eff7fd59a2f561a
Diff:     00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 6
Header:   c652bb5103a03b0f44591d4d225e146265c9c3690aec481cc7118f5c9957352a
Start:    623c9502781a5995
Solution: 623c9502a832156d (806,861,784 hashes)
Hash:     0000000530f3b76089b766f7a2beb629b80f42825f408ad222d248ba650e584c
Diff:     00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 7
Header:   9446fc0fe1e9bc40e3f11653d1d4796363bf663a8916190cee6f5c13d019a818
Start:    040137950dca919e
Solution: 040137950e6d0570 (10,646,482 hashes)
Hash:     00000004faa81b30370af7b7053f3a8d5aa705cfb555f83ccef88d1be3067349
Diff:     00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 8
Header:   85d1c941572f7a0f1c1624478358202c8956134d7a625c0b19cbe87ec807dd21
Start:    7973af5e12971ea1
Solution: 7973af5e1742cc56 (78,359,989 hashes)
Hash:     00000007c953a0799d488a088b56efe71b47af3a858a2f705244c1d4af657be2
Diff:     00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 9
Header:   d1c6206925022f4ba5bed22115203e364760432d0a88960b1f0c87520f72d209
Start:    35cbdfa334c3a121
Solution: 35cbdfa33a74e4cc (95,503,275 hashes)
Hash:     00000006930ee717abbe556bfc68f1ea9e888e52c17014209b0bf3d0301cd187
Diff:     00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

Search 10
Header:   a4cbec013826c845038b807587bd035509fb4129664ae72f10d41c61f76a9e3c
Start:    488f6436651e0ba6
Solution: 488f64366f9e10f0 (176,162,122 hashes)
Hash:     000000062677f65694962f0816f14f5fa9661698d51ca24684d55a6d5f7e5d30
Diff:     00000007ffffffffffffffffffffffffffffffffffffffffffffffffffffffff
PASS

root@de10nano:~/linux-socfpga-sha3-miner/tools/testing/miner# 
```

You can also rebuild the kernel (it will take a very long time on the NANO)

```
```

## Disclaimer

This software is provided "AS IS" and any expressed or implied warranties, including, but not limited to, the implied warranties of merchantability and fitness for a particular purpose are disclaimed. In no event shall the regents or contributors be liable for any direct, indirect, incidental, special, exemplary, or consequential damages (including, but not limited to, procurement of substitute goods or services; loss of use, data, or profits; or business interruption) however caused and on any theory of liability, whether in contract, strict liability, or tort (including negligence or otherwise) arising in any way out of the use of this software, even if advised of the possibility of such damage.  
