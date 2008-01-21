# Example watch control file for uscan
# Rename this file to "watch" and then you can run the "uscan" command
# to check for upstream updates and more.
# See uscan(1) for format

# Compulsory line, this is a version 4 file
version=4

# Uncomment to examine a Webpage 
# <Webpage URL> <string match>
#http://www.example.com/downloads.php guvcview-(.*)\.tar\.gz

# Uncomment to examine a Webserver directory
#http://www.example.com/pub/guvcview-(.*)\.tar\.gz

# Uncommment to examine a FTP server
#ftp://ftp.example.com/pub/guvcview-(.*)\.tar\.gz debian uupdate

# Uncomment to find new files on sourceforge, for debscripts >= 2.9
# http://sf.net/guvcview/guvcview-(.*)\.tar\.gz


