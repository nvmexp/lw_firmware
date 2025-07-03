#!/home/gnu/bin/perl
# Following is the implementation of mergeCoverageFiles class 

package mergeCoverageFiles;
use Cwd;

# This sub routine is used to give an object which is just a hash refrence
sub new
{
    my $class = shift;
    my $flist = {};
    bless ($flist,$class);
    return ($flist); # got a hash refrence blessed 
}

# This function is used to add total coverage of directories : Directory => totalcovered_total pairs
sub addTotalList
{
    my ($self, $key, $val) = @_;
    # if function is present already in the hash return    
    if(exists $self->{$key}) 
    {
        return ;
    }
    $self->{$key} = $val;
    return ;
}
# This function is used to get combined report of coverage files given by covfn in one hash
sub addList
{
    my ($self, $key, $val) = @_;
    # if function is present already in the hash update 
    # with (the value already there || $value) and return
    
    if(exists $self->{$key}) 
    {
        $self->{$key} = ($self->{$key}) || $val;
        return ;
    }
    $self->{$key} = $val;
    return ;
}

# This function is used to accumulate coverage of leaf dirs(dirs with files)
sub addDir
{
    my ($self, $key, $val,$flag)=(@_);
    # a hash of arrays that is one key will map to multiple values
    if (exists $self -> {$key })
    {
        if($flag)
        {
            @{$self -> {$key}}[0] +=1;
        }
    }
    else
    {
        if($flag)
        {
            @{ $self -> {$key} }[0] = 1;
        }
        else
        {
            @{$self -> {$key} }[0]  = 0;
        }
    }
    # we use push so that the value remains at first index
    push(@{ $self -> {$key} },$val);
    return ;
}

# This is used to accumulate :File => (func1,func2,func3) mapping
sub addFile
{
    my ($self, $key, $val)=(@_);
    # a hash of arrays that is one key will map to multiple values
    push(@{ $self -> {$key} },$val);
    return ;
}

# This is used to print the coverage of files 
sub printFile
{
    my $this = shift;
    my $direc = shift;
    my $i = 0;
    my %hash = %$this;
    my ($func,$val,$covered,$file);
    my $perc = 0;
    foreach my $string (keys %hash)
    {
        $i++;
        $file = "$direc/$string" ;
        $file  .= ".csv";
        $file   = colwertToOsPath($file);    
        my $ref = $hash {$string};
        
        my $j = 0;
        $covered = 0;
        open(DAT1,">>$file") || die "Cannot open file : $file\n";
        print DAT1 "$string \n\n";
        print DAT1 "FUNCTION ,COVERAGE\n";
        foreach my $out(@{$ref})
        {
            ($func,$val) =split(/\^/,$out);
            if($val == 1)
            {
                $covered++;
            }
            $j++;
            print DAT1 "$func,$val\n";
        }
        if ($j != 0)
        {
            $perc = ($covered / $j) * 100;
        }
        $perc .= "%";
        print DAT1 "\n";
        print DAT1 "PERCENTAGE,COVERED,TOTAL\n";  
        print DAT1 "$perc,$covered,$j\n";
        close DAT1;
    }
    return;
}


# This is used to create the directory structure inside Output directory given a directory string
sub createDirStructure
{
	
    my $cwd = getcwd();
    my ($object,$argument,$dir) = @_ ;
    my (@tokens);
    
    if($^O eq "MSWin32")
    {
		@tokens = split(/\\/,$argument);
    }
	else
	{
		@tokens = split(/\//,$argument);
	}
    
    foreach my $token(@tokens)
    {
        $dir = "$dir/$token";
        $dir = colwertToOsPath($dir);
        mkdir ($dir,0777) unless (-d $dir);
    }
    return;
}

# This function is called to print all the reports of the directories in .csv 's
sub printDirList
{
    my $this = shift;
    my $outputDir = shift;
    my ($totalNoOfFunctions,$totalCoveredFunctions,@paths,@sortedKeys,%intermediateDir);
    my $i = 1;
    my %hash = %$this;
    my (%totHash,$direc);
    
    # Processing of leaf directories also making an entry into tothash
    foreach my $string (keys %hash)
    {
        if ($string ne '.')
        {
            
            my $fileObj = mergeCoverageFiles:: -> new();
            my $ref = $hash {$string};
            ($direc = "$outputDir/$string/") ;
            $direc  = colwertToOsPath($direc);
            $this -> createDirStructure($string,$outputDir);
            my $j = 0;
            my @temp               = @{$ref};
			
            $totalNoOfFunctions    = $#temp;
            $totalCoveredFunctions = shift(@temp);
            my $seperator = '\+';
            
            foreach my $out(@temp)
            {
                my ($key,$value) = split($seperator,$out);
                $fileObj -> addFile($key,$value);
            }
            $fileObj -> printFile($direc);
            my $perc = ( $totalCoveredFunctions / $totalNoOfFunctions) *100;
            $string .="/";
            $string = colwertToOsPath($string);
            $totHash{$string} = "$totalCoveredFunctions"."_"."$totalNoOfFunctions";
        }
    }
    
    # Sorting the leaf directories descending length wise
    @sortedKeys = sort{
        length($b) <=> length($a)
        }keys %totHash;
    
    
    # Merging to get data for intermediate directories
    foreach my $dir (@sortedKeys)
    {
        my ($coveredFunc,$totalFunc,$tempcoveredFunc,$temptotalFunc,$tempCovered , $tempTotalFunc,@tokens); 
        my $dirString = '/';
        $dirString = colwertToOsPath($dirString);
        ($coveredFunc,$totalFunc) = split(_ ,$totHash{$dir});
        if($^O eq "MSWin32")
        {
            @tokens = split(/\\/,$dir);
        }		
        else		
        {
            @tokens = split(/\//,$dir);
        } 
        # removing the base by decreasing last index by 1
        $#tokens -= 1; # Remember to remove the directory from tokens thing
        
        # aclwmulating all the directories and adding them like a stack to keep them from smaller to larger
        # resetting @paths to ''every time
        @paths = '';
        foreach my $token(@tokens)
        {
            if($token ne '')
            {
                $token  =~ s?/??g;
                $dirString .= "$token/";
                $dirString = colwertToOsPath($dirString);
                push (@paths,$dirString);
            }
        }
        # for all paths aclwmulated in array paths
        while(@paths)
        {
            my $path = pop(@paths);
            # if it is also a base then dont go to upper levels
            if( (exists $totHash{$path}) )
            {
                ($tempcoveredFunc,$temptotalFunc) = split(_ ,$totHash{$path});
                $tempcoveredFunc += $coveredFunc;
                $temptotalFunc   += $totalFunc;
                $totHash{$path} = "$tempcoveredFunc"."_"."$temptotalFunc";
                last;
            }
            else
            {
                # update it and continue the loop
                if( (exists $intermediateDir{$path}) )
                {
                    ($tempcoveredFunc,$temptotalFunc) = split(_ ,$intermediateDir{$path});
                    $tempcoveredFunc += $coveredFunc;
                    $temptotalFunc   += $totalFunc;
                    $intermediateDir{$path} = "$tempcoveredFunc"."_"."$temptotalFunc";
                }
                else
                {
                    $intermediateDir{$path} = "$coveredFunc"."_"."$totalFunc";
                }
            }
        }
    }
    # printing base directories
    foreach my $dir (keys %totHash)
    {
       
        if ($dir ne '')
        {
            my $total_file ="$outputDir/$dir/total.csv";
            $total_file = colwertToOsPath($total_file);
            my $perc ;
            my ($tempcov,$tempfunc) = split(_ ,$totHash{$dir});
            
            if($tempfunc !=0)
            {
                $perc = ($tempcov / $tempfunc) * 100;
            }
            else
            {
                $perc = 0;
            }
            open(DAT,">>$total_file") || die "Cannot open file : $total_file\n";
            print DAT "DIRECTORY,%COVERED,COVERED,TOTAL\n";
            print DAT "$dir,$perc,$tempcov,$tempfunc";
            close DAT;
        }
    }
    # printing intermediate directories
    foreach my $dir (keys %intermediateDir)
    {
        if ($dir ne '')
        {
            my $total_file ="$outputDir/$dir/total.csv";
            $total_file = colwertToOsPath($total_file);
            my ($tempcov,$tempfunc) = split(_ ,$intermediateDir{$dir});
            my $perc;
            if($tempfunc !=0)
            {
                $perc = ($tempcov / $tempfunc) * 100;
            }
            else
            {
                $perc = 0;
            }
            open(DAT,">>$total_file") || die "Cannot open file : $total_file\n";
            print DAT "DIRECTORY,%COVERED,COVERED,TOTAL\n";
            print DAT "$dir,$perc,$tempcov,$tempfunc"; 
            close DAT;
        }
     }
    my $swDir = "$outputDir/sw/";
    $swDir    = colwertToOsPath($swDir);
    if(!(-f "$swDir/total.csv"))
    {
        print "Error :  Software error , non existing $swDir/total.csv";
        exit 0;
    }
    
    print ("Find output in $outputDir \n"); 
}

sub colwertToOsPath
{
    my $argument = @_[0];
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
1;
