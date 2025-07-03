#! /home/gnu/bin/perl -w 
#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2011 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

use strict;
use XML::Parser;
use Getopt::Long;

# commandline vars
my ($oldFile,$newFile,$onlyFunc,$help,$Bullseye_Bin,$htmlOutputFile,$clold,$clnew);
  
# Hashref containing infos on xml files
my ($set,$fncov,$funcdiff,$funcwarn,$delta,@coverageod,@coverageow,$foldername,$src,$fnname,%coveragehash1,%coveragehash2,$index,$message);      

# Parse Cmdline Options
my $result = GetOptions("oldFile=s"        => \$oldFile,
                        "newFile=s"        => \$newFile,
			"onlyFunc=i"       => \$onlyFunc,
			"Bullseye_Bin=s"   => \$Bullseye_Bin,
			"clold=s"         => \$clold,
			"clnew=s"         => \$clnew,  
			"htmlOutputFile=s" => \$htmlOutputFile,
                        "help"             => \$help);

# Help Message
sub print_help_msg()
{
    print ("********************** options available ***************************\n");
    print ("-oldFile <path>    : Specify the absolute path to coverage file   \n");
    print ("-newFile <path>    : Specify the absolute path to coverage file   \n");
    print ("OPTIONAL ARGUMENTS\n");
    print ("-onlyFunc          : 0/1,By Default its 0,for only function cov give 1\n");
    print ("-Bullseye_Bin      : exact location of the Bullseye Bin               \n");
    print ("-htmlOutputFile    : name of the output file                          \n");
    print ("-help              : To get description of various commandline options\n");
    print ("********************************************************************\n");
}

sub main()
{
    my ($oldfilename,$newfilename,$bullseyeCommand);
    if(!$result || defined($help))
    {   
        print_help_msg();
        exit 0;
    }
    # Check for mandatory arguments
    if (!defined($oldFile) || !defined($newFile))
    { 
        die "Mandatory arguments are not specified, try -help for more info\n" ; 
    } 
        
    # Change to os specific paths 
    $oldFile = colwertToOsPath($oldFile);
    $newFile = colwertToOsPath($newFile);  
   
    # Check if the files exist
    if(!(-f $oldFile)) 
    { 
        die "\n$oldFile not a file or does not exist\n"; 
    }
    
    if(!(-f $newFile)) 
    { 
        die "\n $newFile not a file or does not exist\n"; 
    }
 
    if(!defined($onlyFunc))
    {
        $onlyFunc=0;
    } 

    if(!defined($htmlOutputFile))
    {
        my ($sec, $min, $hr, $dayofm, $month, $yr) = localtime();
        $htmlOutputFile = "output_".$sec.$min.$hr.$dayofm.$month.$yr.".html";
    }
    
    if(defined($Bullseye_Bin))
    {
        excluderegion($oldFile);
       	excluderegion($newFile);
	$oldfilename ="oldfile1.xml";
	$newfilename ="newfile1.xml";
	$bullseyeCommand = "$Bullseye_Bin/covxml -f"."$oldFile";
        system("$bullseyeCommand > $oldfilename");
        $oldFile = $oldfilename;
	$bullseyeCommand = "$Bullseye_Bin/covxml -f"."$newFile";
        system("$bullseyeCommand > $newfilename");
        $newFile = $newfilename;
        
    }
      
    # opening xml files
    open my $fh, $oldFile or die $!; 
    open my $fh1, $newFile or die $!;  
 
    my $parser = new XML::Parser ( Handlers => {   # Creates our parser object
    Start   => \&hdl_start,
    End     => \&hdl_end,
    Char    => \&hdl_char,
    Default => \&hdl_def,
    });

    # parsing 1st xml file
    $set=0;
    $index=0;
    $parser->parse($fh);

    # parsing 2nd xml file
    $set=0;
    $index=1;
    $parser->parse($fh1);

    $funcdiff =0;
    $funcwarn =0;
    $delta = 0;
    # getting diff of two xml files
    if ($onlyFunc == 1)
    {
        diffunc();
    }
    else
    {
        diffunc();
        diffcond();
    } 
    filloutput();
}    

# The Handlers
sub hdl_start
{
    my($p,$elt,%atts)= @_;
    return unless (($elt eq 'fn')|| ($elt eq 'src')||($elt eq 'probe') || ($elt eq 'folder'));

    my ($column,$col_seq);
    $atts{'_str'} = '';
    $message = \%atts; 
    if($elt eq 'folder')
    {
        $foldername .= "/".$message->{'name'};
    }
    if($elt eq 'fn')
    {
        $set =1;
    }
    if($elt eq 'src')
    {
        $src = $message->{'name'};
    }
    elsif ($elt eq 'probe')
    {
        if(exists($atts{'column'}))
        {
            $column = $message->{'column'};
        }
        else
        {
            $column = 0;
        }  
        if(exists($atts{'col_seq'}))
        {
            $col_seq = $message->{'col_seq'};
        }
        else
        {
            $col_seq = 0;
        }
        if($index eq 0)
        {
            $foldername =~ s?///?/?g ;	
	    push @{$coveragehash1{$foldername."/".$src.",".$fnname}},{fn_cov => $fncov,line => $message->{'line'},column => $column, col_sq => $col_seq ,kind => $message->{'kind'} , event => $message->{'event'}};
        }
        else
        {
            $foldername =~ s?///?/?g ;
	    push @{$coveragehash2{$foldername."/".$src.",".$fnname}},{fn_cov => $fncov,line => $message->{'line'},column => $column , col_sq => $col_seq ,kind => $message->{'kind'} , event => $message->{'event'}};
 
        }
    }
    elsif ($elt eq 'fn')
    {
	my $tmpfuncname;
	$tmpfuncname = $message->{'name'};
	$tmpfuncname =~ /(\S+)\(/;
        $fnname = $1;
	$fncov = $message->{'fn_cov'};
        if($index eq 0)
        {
            $foldername =~ s?///?/?g ;
            $coveragehash1{$foldername."/".$src.",".$fnname}= [];
        }
        else
        {
            $foldername =~ s?///?/?g ;
	    $coveragehash2{$foldername."/".$src.",".$fnname}= [];
        }  
    }
}
sub hdl_end
{
    my($p,$elt)= @_;
    if($elt eq 'folder')
    {
        $foldername =~ m/(\S*)\/(\S*)$/ ;
	$foldername = $1;
    }
    if($elt eq 'src')
    {
        undef $src;
    }
    undef $message;
    if($elt eq 'fn')
    {
       undef $fnname;
    }
    if($set ne 1) 
    {
        print "No Function tag found, exiting!!\n";
        exit;
    }
}

sub hdl_char
{
}

sub hdl_def
{
}

# function that will give diff between two xml files based on functional coverage
sub diffunc()
{
    my ($fncov2,$count,$fncov1,$filepath,$filename,$funcname);
    $count =0;
    outer:foreach (sort keys %coveragehash2)
          {
              $fncov1 = 0;
              $fncov2 = 0;
              if (exists ($coveragehash1{$_}))
              {
                  $fncov1 = $coveragehash1{$_}[0]{fn_cov};
	      }
              $fncov2 = $coveragehash2{$_}[0]{fn_cov};
              if(($fncov2 eq 0)&& ($fncov1 eq 1))
              {
	          $_ =~ /(\S+?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\,(\S*)/;
		  $filepath = $8;
	          $funcname = $9;
		  push @coverageod, { filepath => $filepath , funcname => $funcname, cond => "N/A" , delta => "-1",comments => "Coverage down" };
		  $funcdiff++;
		  $delta--;
		  next outer;
              }
	      elsif(($fncov2 eq 1)&&($fncov1 eq 0))
	      {
	          $_ =~ /(\S+?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\,(\S*)/;
		  $filepath = $8;
		  $funcname = $9;
		  push @coverageod, { filepath => $filepath , funcname => $funcname, cond => "N/A" , delta => "+1",comments => "Coverage Up" };
		  $funcdiff++;
		  $delta++;
		  next outer;
              }
	      elsif (($fncov2 eq 0) && (!exists($coveragehash1{$_})))
              {
	          $funcwarn++;
	          $_ =~ /(\S+?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\,(\S*)/;
	          $filepath = $8;
	          $funcname = $9;
	          push @coverageow, { filepath => $filepath , funcname => $funcname, cond => "N/A" , delta => "N/A" ,comments => "Warning: Fn not present/renamed in old file" };
              }  
          }
}

# fucntion that will give diff between two xml files based on conditional coverage
sub diffcond()
{
    my (@tmphashnew,$tmp,$filepath,$funcname,@eg,$key,$key1,$recordnew,$recordold,$fncovnew,$fncovold,$pri,$cond,$j,$flag);
    outer: foreach $key(sort keys %coveragehash2)
    {
        $fncovold = 0;
        $fncovnew = 0;
        undef(@tmphashnew);
        file1: foreach my $recordnew(@{$coveragehash2{$key}})
               {
                   $fncovnew = $recordnew->{fn_cov};
                   if($fncovnew eq 0)
	           {
	               next outer;
	           }
                   $tmp = givepri($recordnew->{event});
	           push @tmphashnew, {line=> $recordnew->{line},kind => $recordnew->{kind}, event => $tmp};
               }
        $j =0;
	$flag = 0;
        if (exists ($coveragehash1{$key}))
        {
            file2:  foreach my $recordold(@{$coveragehash1{$key}})
                    {
                        $fncovold = $recordold->{fn_cov};
                        if($fncovold eq 0)
                        {
		            next outer;
		        } 
                        
			#if new file function has less lines than old file
			if($j == ($#tmphashnew+1))
			{
                           $key =~ /(\S+?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\,(\S*)/;
			   $filepath = $8;
			   $funcname = $9;
			   $funcwarn++;
			   push @coverageow, {filepath => $filepath , funcname => $funcname, cond => "N/A", delta => "N/A",comments =>"Warning this
			 function is modified" };
			   next outer;
			}
			
			if($recordold->{kind} eq $tmphashnew[$j]{kind})
	                {
		            $pri = givepri($recordold->{event}); 
			   if($tmphashnew[$j]{event} < $pri)
			    {
			        $key =~ /(\S+?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\,(\S*)/;
			        $filepath = $8;
			        $funcname = $9;
			        $cond = "line=".$tmphashnew[$j]{line}.",kind=".$tmphashnew[$j]{kind};
			        push @coverageod, {filepath => $filepath , funcname => $funcname, cond => $cond, delta => "-1",comments => "Coverage Down" };
			        $funcdiff++;
				$delta--;
	                    } 
		        }
	                else
	                {
		            $key =~ /(\S+?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\,(\S*)/;
			    $filepath = $8;
		            $funcname = $9;
		            $cond = "line=".$tmphashnew[$j]{line}.",kind=".$tmphashnew[$j]{kind};
		            push @coverageow, {filepath => $filepath,funcname => $funcname, cond => $cond, delta => "N/A",comments => "Warning this function is modified" };
		            $funcwarn++;
			    next outer;
                        } 
	                $j++;
		        $flag =1;
		    
		    }   
        }
        
	# if new file has more lines than old file
	if(($j < $#tmphashnew) && ($flag == 1))
        {
	    $key =~ /(\S+?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\/(\S*?)\,(\S*)/;
	    $filepath = $8;
	    $funcname = $9;
	    $cond = "line=".$tmphashnew[$j]{line}.",kind=".$tmphashnew[$j]{kind};
	    push @coverageow, {filepath => $filepath , funcname => $funcname, cond => $cond , delta => "N/A",comments =>"Warning this function is modified" };
            $funcwarn++;
        }
 
    }
}  

# function that gives priority
sub givepri()
{
    my $value = shift;
    my %prihash = ("full",3,"true",2,"false",1,"none",0);
    if(exists($prihash{$value}))
    {
        return $prihash{$value};
    }
    else
    {
        return -1;
    } 
}
# exclude region from the cov files
sub excluderegion()
{
   my ($argument,$excluderegionCommand,$knownregion,$tempCommand,$funcCoverageCommand,@output,$hit,$excluderegiondebug,$excluderegionout,$i,$tmpvar,$hitout);
   $argument = shift;
   $tempCommand = "$Bullseye_Bin/covfn -c";
   $tempCommand = colwertToOsPath($tempCommand);
   $funcCoverageCommand = "$tempCommand -f"."$argument";
   @output = `$funcCoverageCommand`;
   $i=0;
   $knownregion =0;
   $hit =0;
   $excluderegiondebug = "/debug";
   $excluderegionout   = "/_out";
   $hitout =0;
   foreach my $line(@output)
   {
      if ($i == 0)
      {
         $i++;
      }
      else
      {
         chomp($line);
         if(($line =~ m/$excluderegiondebug/) || ($line =~ m/$excluderegionout/))
         {
            if(($line =~ m/$excluderegiondebug/)&& ($hit != 2))
            {
               $line =~ /(\S*),"(\S*)$excluderegiondebug(\S*)/ ;
               $tmpvar = $2.$excluderegiondebug;
               if($knownregion eq 0)
               {
                  $knownregion = $tmpvar;
	          print "$knownregion\n";
		  $hit++;
		  $tempCommand = "$Bullseye_Bin/covselect -f$argument -a -v";
		  $tempCommand = colwertToOsPath($tempCommand);
		  $excluderegionCommand = "$tempCommand !$knownregion/";
		  system($excluderegionCommand);
	       } 
	       if($knownregion ne $tmpvar)
	       {
	          $knownregion = $tmpvar;
		  print "$knownregion\n";
		  $hit++;
		  $knownregion = colwertToOsPath($knownregion);
		  $tempCommand = "$Bullseye_Bin/covselect -f$argument -a -v";
		  $tempCommand = colwertToOsPath($tempCommand);
		  $excluderegionCommand = "$tempCommand !$knownregion/";
		  system($excluderegionCommand);
	       }   
	    }
	    elsif (($line =~ m/$excluderegionout/)&& ($hitout != 1))
	    {
	        my $tempregion;
		$hitout =1;
		$line =~ /(\S*),"(\S*)$excluderegionout(\S*)/ ;
		$tempregion = $2.$excluderegionout;
		$tempregion = colwertToOsPath($tempregion);
		$tempCommand = "$Bullseye_Bin/covselect -f$argument -a -v";
		$tempCommand = colwertToOsPath($tempCommand);
		$excluderegionCommand = "$tempCommand !$tempregion/";
		system($excluderegionCommand);
	    }
	 }
      }
   }
}

# colwerts to Os path
sub colwertToOsPath
{
    my $argument = '';
    $argument = shift;
    # ^O eq MsWin32 for all versions of Windows .
    if ($^O eq "MSWin32") 
    {
        $argument =~ s?/?\\?g;
    }
    else
    {
     $argument =~ s?\\?/?g;
    }
    $argument =~ s?//?/?g ;
    $argument  =~ s{\\\\}{\\}g;
    return $argument;
}

sub filloutput()
{

   open HTML ,">$htmlOutputFile";
   print HTML "<html>";
   print HTML "<head>";
   print HTML "<title>CODE COVERAGE PAGE</title>";
   print HTML "<style type=\"text/css\">";
   print HTML "* {margin:0; padding:0}";
   print HTML "html {height:100%}";
   print HTML "body {
                       min-width:400px;
                       width:100%;
                       height:101%;
                       background:#fff;
                       color:#333;
                       font:76%/1.6 verdana,geneva,lucida,'lucida grande',arial,helvetica,sans-serif;
                       text-align:center
                    }";
   print HTML "#wrapper{
                          margin:0 auto;
                          padding:15px 15%;
                          text-align:left
                       }";
   print HTML "#content {
                           max-width:70em;
                           width:100%;
                           margin:0 auto;
                           padding-bottom:20px;
                           overflow:auto
                        }";
   print HTML ".demo {
                        margin:0;
                        padding:1.5em 1.5em 0.75em;
                        border:1px solid #ccc;
                        position:relative;
                        overflow:hidden
                     }";
   print HTML ".collapse p {padding:0 10px 1em}
               .collapse { overflow:auto }
               .top{font-size:.9em; text-align:right}
                #switch, .switch {margin-bottom:5px; text-align:right}";
   print HTML "h1 {
                     margin-bottom:.75em; 
                     font-family:georgia,'times new roman',times,serif; 
                     font-size:2.5em; 
                     font-weight:normal; 
                     color:#c30
                  }";
   print HTML "h3 {
                     margin-bottom:.75em;
                     font-family : Comic Sans, Comic Sans MS, lwrsive;     
                     font-size:2em;
                     font-weight:normal;
	             color:blue}";
   print HTML "h5 {
                     margin-bottom:.75em;
                     font-family : Comic Sans, Comic Sans MS, lwrsive;
                     font-size:2em;
                     font-weight:normal;
                     color:black
                  }";
   
   print HTML "h2{font-size:1em}";
   print HTML ".expand{padding-bottom:.75em}";
   print HTML " a:link, a:visited {
                                     border:1px dotted #ccc;
                                     border-width:0 0 1px;
                                     text-decoration:none;
                                     color:blue
                                  }";
   print HTML "a:hover, a:active, a:focus {
                                             border-style:solid;
                                             background-color:#f0f0f0;
                                             outline:0 none
                                          }";
   print HTML "a:active, a:focus { color:red; }";
   print HTML ".expand a {
                            display:block;
                            padding:3px 10px
                         }";
   print HTML ".expand a:link, .expand a:visited {
                                                    border-width:1px;
                                                    background-image:url(arrow-down.gif);
                                                    background-repeat:no-repeat;
                                                    background-position:98% 50%;
                                                 }";
   print HTML ".expand a:hover, .expand a:active, .expand a:focus {
                                                                     text-decoration:underline
                                                                  }";
   print HTML ".expand a.open:link, .expand a.open:visited {
                                                              border-style:solid;
                                                              background:#eee url(arrow-up.gif) no-repeat 98% 50%
                                                           }</style>";
   print HTML "<!--[if lte IE 7]>
               <style type=\"text/css\">
               h2 a, .demo {position:relative; height:1%}
               </style>
               <![endif]-->";
   print HTML "<!--[if lte IE 6]>
               <script type=\"text/javascript\">
               try { document.execCommand( \"BackgroundImageCache\", false, true); } catch(e) {};
               </script>
               <![endif]-->";
   print HTML "<!--[if !lt IE 6]><!-->
               <script type=\"text/javascript\" src=\"jquery.min.js\"></script>
               <script type=\"text/javascript\" src=\"expand.js\"></script>";
   print HTML "<script type=\"text/javascript\">
               <!--//--><![CDATA[//><!--
               \$(function() {
               // --- Using the default options:
               \$(\"h2.expand\").toggler();
               \$(\"#content\").expandAll({trigger: \"h2.expand\", ref: \"div.demo\", localLinks: \"p.top a\"});
               });
               //--><!]]>
               </script>
               <!--<![endif]-->
               </head>";
   print HTML "<body>
               <div id=\"wrapper\"> 
               <div id=\"content\">  
	       <h1 align=\"center\" class=\"heading\">CODE COVERAGE DIFF</h1>";
               if((defined ($clold))&& (defined ($clnew)))
               {
                  print HTML "<h5 COLOR =\"black\" align=\"center\">(SW ";
                  print HTML "$clnew - $clold)</h5>";
               }
	       print HTML "<div class=\"demo\">
               <h3 align=\"center\" class=\"open\"><u>SUMMARY</u></h3>
               <div class=\"expand\">";
	       if($delta > 0)
               {
	          print HTML "<h4 align=\"center\">Total Coverage : Up</h4>"; 
               }
	       elsif ($delta < 0)
	       {
	          print HTML "<h4 align=\"center\">Total Coverage : Down</h4>";
	       }
	       else
	       {
	          print HTML "<h4 align=\"center\">Total Coverage : Same</h4>";
               }		
					      
   print HTML "<h4 align=\"center\">Differences : $funcdiff</h4>"; 
   print HTML "<h4 align=\"center\">Warnings  : $funcwarn </h4>";
   print HTML " </div>
                <h2 class=\"expand\">DIFFERENCES</h2>
                <div class=\"collapse\">";
   print HTML "<br><table align = \"center\" height=\"30\" border=\"2\" bordercolor=\"black\">\n";
   print HTML "<tr height=\"30\" BGCOLOR=\"ORANGE\"><th colspan=\"5\"> <h2> No of Differences : $funcdiff</h2></th></tr>";
   if($funcdiff != 0)
   {
       print HTML "<h4> Coverage Up";
       print HTML "<table BORDER=0 WIDTH =\"10%\" CELLPADDING=0 CELLSPACING=0
	   height=15><TR><TD bgcolor=\"GREEN\" align=center>
	   <table BORDER=0 CELLPADDING=0 CELLSPACING=3>
	   <TR><TD></TD></TR></TABLE>
	   </TD></TR></TABLE>";
       print HTML "<h4>Coverage Down";
       print HTML "<table BORDER=0 WIDTH=\"10%\" CELLPADDING=0 CELLSPACING=0
	   height=15><TR><TD bgcolor=\"RED\" align=center> <table BORDER=0
	   CELLPADDING=0 CELLSPACING=3> <TR><TD></TD></TR></TABLE>
	   </TD></TR></TABLE>";
      print HTML "<tr><th>File Path</th><th>Function Name</th><th>Decision/Condition</th></tr>";
      foreach(sort {$$a{filepath} cmp $$b{filepath} } @coverageod)
      {
         if($$_{delta} eq "-1")
         {
            print HTML "<tr><td>$$_{filepath}</td><td BGCOLOR=\"RED\">$$_{funcname}</td><td BGCOLOR=\"RED\">$$_{cond}</td></tr>";
	 } 
	 else
	 {
	    print HTML "<tr><td>$$_{filepath}</td><td BGCOLOR =\"GREEN\">$$_{funcname}</td><td BGCOLOR=\"GREEN\" >$$_{cond}</td></tr>";
	 }
      }
      print HTML "<tr height=\"30\"><th colspan=\"5\"> <h2> Total Coverage Delta : $delta<h2></th></tr>";
   }
   print HTML "</table>";
   print HTML "<p class=\"top\"><a href=\"#content\">Top of page</a></p>
               </div>
               <h2 class=\"expand\">WARNINGS</h2>
               <div class=\"collapse\">";
    
   print HTML "<table align = \"center\" height=\"30\" border=\"2\" bordercolor=\"black\" width=\"100%\">\n";
   print HTML "<tr height=\"30\"><th colspan=\"5\"> <h2> No of Warnings: $funcwarn</h2></th></tr>";
   if($funcwarn != 0)
   {
      print HTML "<tr><th>File Path</th><th>Function Name</th><th>Comments</th></tr>";
      foreach(sort {$$a{filepath} cmp $$b{filepath} } @coverageow)
      {
         print HTML "<tr><td>$$_{filepath}</td><td>$$_{funcname}</td><td>$$_{comments}</td></tr>";
      }
   }
   print HTML "</table>";
   print HTML "<p class=\"top\"><a href=\"#content\">Top of page</a></p>
              </div>
              </div>
              </div>
              </div>
             </body></html>";
   close HTML;
}
main();
