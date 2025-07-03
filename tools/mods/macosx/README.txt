OSX Mods
06/15/2005
--------

To install OSX Mods: 
  Unzip OSX Mods to a directory

To run OSX Mods:
  Click on "Mods"
   OR
  "cd Mods.app/Contents/Resources" then 
  Run "./mods gputest.js" at the terminal window
   OR
  Run "sudo ./mods gputest.js" over ssh

To lwstomize OSX Mods:
  Control-Click on Mods and choose "Show Package Contents". 
  Edit Mods.app/Contents/Resources/mods.arg, it contains the 
  command line arguments for the clickable mods. 
  For example "mods gputest.js -gpu -loops 2"

To run OSX Mods interactively:
  Remove mods.arg file, edit it to contain "mods -s", or
  run "./mods -s" from the terminal window, or 
  run "sudo ./mods -s" over ssh

To run OSX Mods on a specific LWpu device:
  Add "-gpu_num [index]" where index is the index of the board 
  you want to test. Indexes start at 0 and increase, based on 
  the order that the devices initialized at boot time.

To run all tests in OSX Mods:
  "./mods gputest.js" will run the normal test suite
  "./mods gputest.js -mfg" will run the extended test suite

To run or skip a test in OSX Mods:
  "./mods gputest.js -mfg -test 16" will run only test 16
  "./mods gputest.js -mfg -skip 16" will run all default tests except 16





