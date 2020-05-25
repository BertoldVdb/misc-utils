# aw-efuse

This is a simple utility for reading and writing the eFUSE memory in modern Allwinner CPUs (H3, H5, ...).
Be careful, this program can damage your chip and render it unbootable if the wrong flags are adjusted.

## Usage
The program supports two commands:
 - dump: reads and dumps the full contents of the eFUSE array to the terminal.
 - write *offset* *filename*: write the data from *filename* at the address given by *offset*. The length of the file must be a multiple of 4 bytes.

You can specify multiple commands in one run, for example:
> ./efuse write 0x30 serial.bin write 0x38 flags.bin dump

This will write serial.bin to 0x30, flags.bin to 0x38 and show the resulting memory contents.

