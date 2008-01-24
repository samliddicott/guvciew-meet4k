.\"                                      Hey, EMACS: -*- nroff -*-
.\" First parameter, NAME, should be all caps
.\" Second parameter, SECTION, should be 1-8, maybe w/ subsection
.\" other parameters are allowed: see man(7), man(1)
.TH GUVCVIEW SECTION "Janeiro  9, 2008"
.\" Please adjust this date whenever revising the manpage.
.\"
.\" Some roff macros, for reference:
.\" .nh        disable hyphenation
.\" .hy        enable hyphenation
.\" .ad l      left justify
.\" .ad b      justify to both left and right margins
.\" .nf        disable filling
.\" .fi        enable filling
.\" .br        insert line break
.\" .sp <n>    insert n+1 empty lines
.\" for manpage-specific macros, see man(7)
.SH NAME
guvcview \- UVC video (and sound) capture tool
.SH SYNOPSIS
.B guvcview
.RI [ options ]  ...
.br
.B bar
.RI [ options ]  ...
.SH DESCRIPTION
This manual page documents briefly the
.B guvcview
and
.B bar
commands.
.PP
.\" TeX users may be more comfortable with the \fB<whatever>\fP and
.\" \fI<whatever>\fP escape sequences to invode bold face and italics, 
.\" respectively.
\fBguvcview\fP is a program that enables video capture through the UVC driver 
.SH OPTIONS
For a complete description, see the Info files.
.TP
.B \-h, 
usage: guvcview [-h -d -g -f -s -c -C -S] 
	-h	print this message
	-d	/dev/videoX       use videoX device
	-g	use read method for grab instead mmap
	-w	disable SDL hardware accel.
	-f	video format  default jpg  others options are yuv jpg
	-s	widthxheight      use specified input size
	-c	enable raw frame capturing for the first frame
	-C	enable raw frame stream capturing from the start
	-S	enable raw stream capturing from the start
.TP
.B \-v, 
Show version of program.
.SH SEE ALSO
.BR bar (1),
.BR baz (1).
.br
The programs will be documented fully by
.IR the author ,
available via the Info system.
.SH AUTHOR
guvcview was written by <Paulo Assis - pj.assis@gmail.com>.
.PP
This manual page was written by Paulo Assis <pj.assis@gmail.com>,
for the Debian project (but may be used by others).
