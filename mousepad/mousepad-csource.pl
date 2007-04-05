#!/usr/bin/env perl -w
#
# $Id$
#
# Copyright (c) 2007 Nick Schermer <nick@xfce.org>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 Temple
# Place, Suite 330, Boston, MA  02111-1307  USA
#

if ($#ARGV < 0)
  {
    print ("No variable name defined. Exiting...");
    exit;
  }

# get the variable name
my $varname = $ARGV[0];
shift;

# init
my $max_column = 70;
my $result;
my $line ;

# walk though the lines, strip when and append the result
while (<>)
  {
    # ignore empty lines
    next if /^\s*$/;

    # skip comments
    if ($_ =~ '<!--')
      {
        $in_comment = 1;
      }

    if ($in_comment)
      {
        if ($_ =~ '-->')
          {
            $in_comment = 0;
          }
        next;
      }

    $line = $_;

    # strip the code before and after the xml block
    $line =~ s/.*</</g;
    $line =~ s/>.*\n/>/g;

    # append the striped line to the result
    $result .= $line;
  }

# start of the file
print  "static const unsigned int ". $varname ."_length = ". length ($result) .";\n\n".
       "#ifdef __SUNPRO_C\n".
       "#pragma align 4 (". $varname .")\n".
       "#endif\n".
       "#ifdef __GNUC__\n".
       "static const char ". $varname ."[] __attribute__ ((__aligned__ (4))) =\n".
       "#else\n".
       "static const char ". $varname ."[] =\n".
       "#endif\n".
       "{\n";

# escape the comments
$result =~ s/\"/\\"/g;

# print the content of the xml file
for ($i = 0; $i < length ($result); $i = $i + $max_column)
  {
    print "  \"" . substr ($result, $i, $max_column) . "\"\n";
  }

# close
print  "};\n\n";
