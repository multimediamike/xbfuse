# xbfuse - Use FUSE To Mount (Original) Xbox and Xbox 360 DVD Filesystems

xbfuse is a program that allows you to mount the filesystem of a 
Microsoft Xbox or Xbox 360 DVD as a read-only part of the Linux 
filesystem. This allows the user to browse the directory structure and 
read the files within. 

xbfuse is made possible with Filesystem in Userspace (FUSE), available 
at:

  https://github.com/libfuse/libfuse

Note that it is not usually possible to simply read an Xbox/360 DVD in
an ordinary computer's DVD-ROM drive. The optical drives in Microsoft 
consoles have special firmware which allows them to access areas of the 
disc that are effectively invisible to most DVD-ROM drives. In order to 
mount a filesystem, generally, you will have to rip the proper 
sector image from the disc using special hardware and tools, or contact 
another source who has already done so.

Note also that there are likely to be bugs and perhaps even security 
problems. xbfuse is currently meant as primarily an experimental 
research tool for studying Microsoft Xbox/360 discs.


### Requirements:
- Linux 2.4.x or 2.6.x (as of 2.6.14 FUSE is part of the kernel, but you still need user libraries)
- FUSE (https://github.com/libfuse/libfuse) 2.5.x or higher
- FUSE development libraries; 'libfuse-dev' on Ubuntu distros

### Build:
After cloning this git repo, perform the standard development steps for building an autotool'd project:

    ./autogen.sh
    ./configure
    make

### Install:
    make install

### Usage:
The basic usage is to supply an Xbox/360 DVD image and an empty mount point on the filesystem:

    xbfuse <image_file> <mount_point>

To unmount a previously mounted filesystem image, use `fusermount`:

    fusermount -u <mount_point>

To debug, or investigate how xbfuse examines the filesystem:

    xbfuse <image_file> <mount_point> -d

To export the absolute file offsets via a file's stat structure's inode field, specify the "-o use_ino" option. For example:

    xbfuse xbox-game.image-file /path/to/mountpoint -o use_ino
    stat --printf='offset: %i\nsize: %s\n' /path/to/mountpoint/default.xbe

This `stat` command will print the absolute offset and size of the default.xbe
file at the root of the filesystem.

### References:
This program was made possible through the information found in
[this XDVDFS document](https://multimedia.cx/xdvdfs.html).
