#!/usr/bin/perl -w
use strict;

# Little script to loop over the *.rom files in the ./ROMs subdirectory
# and convert them into a single C header file (using xxd). You should
# have at least a file called "48_pico.rom" in there, which should be
# the Spectrum's 16K ROM image. It needs to be called that name as the
# build expects it. That ROM's the default so the Spectrum boots into
# its normal ROM image (or whatever you've got in that file).

unlink "./roms.h";

my @names = ();

# Open the ROMs directory, loop over the *.rom files and convert them
# into a single roms.h header file.
#
opendir( my $dh, "./ROMs" ) or die("Unable to open ./ROMs");
while( readdir $dh )
{
  next if( $_ eq "." or $_ eq ".." or $_ !~ /\.rom$/ );

  system( "xxd -i ./ROMs/$_ >> roms.h" );

  my $name = "__ROMs_${_}";
  $name =~ s/\./_/g;
  push( @names, $name );
}
closedir( $dh );

# Open the roms.h header file just created and append an array of
# pointers to the created arrays into it
#
open( my $fh, ">>roms.h" ) or die("Unable to open roms.h");

print( $fh "unsigned char *roms[] = {\n" );
foreach my $name (@names)
{
  print( $fh "$name,\n" );
}
print( $fh "};\n" );

close( $fh );

exit 0;
