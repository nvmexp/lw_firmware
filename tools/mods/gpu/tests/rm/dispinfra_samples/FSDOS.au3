; This script will launch a full screen dos box and check if user saw it
; If the user doesn't enter an answer within 60 secs, the script assumes
; that user didn't see the FSDOS box

Global $UserInputFile = "input.txt"
Global $UserInputBatchFile = "input.bat"
Global $CmdPromptTitleAndExeName = ElwGet("COMSPEC")
Global $Failure = "FAILURE"
Global $Success = "SUCCESS"
Global $MaxTryCount = 5
Global $AppRunTime = 2000

Global $X29ExeName = "\\lwsw\dumps\mbindlish\X29.exe"
Global $X29Title = "3Dlabs X29"
Global $BlobsExeName = "\\lwsw\dumps\mbindlish\Blobs\Blobs.exe"
Global $BlobsTitle = "Blobs"

Func IsTestSuccessful()
    Local $file = FileOpen($UserInputFile, 0)

    ; Check if file opened for reading OK
    If $file = -1 Then
        return $Failure & " [Unable to open file: " & $UserInputFile & " . Likely user didn't input anything for " & $MaxTryCount & " secs]"
    EndIf

    Local $char = FileRead($file, 1)
    If @error = -1 Then return $Failure  & " [Unable to read file: " & $UserInputFile & "]"
    If $char <> 'y' Then return $Failure  & " [User couldn't see the FSDOS box]"

    FileClose($file)
    return $Success
EndFunc

Func StartX29()
    Run($X29ExeName)
    WinWaitActive($X29Title)
    Sleep(3000)
EndFunc

Func CloseX29()
    WinClose($X29Title)
EndFunc

Func StartBlobs()
    Local $delay = 500;
    Run($BlobsExeName)
    WinWaitActive($BlobsTitle)

    ; Make Blobs Full screen
    ; Delays between keystrokes are required to make sure they aren't missed
    Send("!{ENTER}")
    Sleep($delay)

    ; Select "Change Device"
    Send("{F2}")
    Sleep($delay)

    ; Select "Present Interval" by hitting Shift+TAB 2 times
    Send("{SHIFTDOWN}")
    Sleep($delay)
    Send("{TAB}")
    Sleep($delay)
    Send("{TAB}")
    Sleep($delay)
    Send("{SHIFTUP}")
    Sleep($delay)

    ; Select D3DPRESENT_INTERVAL_DEFAULT
    Send("{UP}")
    Sleep($delay)
    ;Send("{UP}")
    ;Sleep($delay)
    ;Send("{UP}")
    ;Sleep($delay)
    ;Send("{UP}")
    ;Sleep($delay)
    ;Send("{UP}")
    ;Sleep($delay)
    ;Send("{UP}")
    ;Sleep($delay)

    ; Select "OK" by hitting TAB
    Send("{TAB}")
    Sleep($delay)
    ;MsgBox(0, "Done with TAB", "Done with TAB")

    ; Click "OK"
    Send("{SPACE}")
EndFunc

Func CloseBlobs()
    ; Close Blobs
    WinClose($BlobsTitle)
EndFunc

Func DoFSDOS()
    $PID = Run($CmdPromptTitleAndExeName  & " /c start " & $UserInputBatchFile)
    WinWaitActive($CmdPromptTitleAndExeName & " - " & $UserInputBatchFile)
    Send("!{ENTER}")
    Local $RemainingTries = $MaxTryCount
    Do
        Sleep(1000)
        $RemainingTries -= 1
    Until FileExists ($UserInputFile) OR ($RemainingTries = 0)

    MsgBox(0, "Result: ", "Result: " & IsTestSuccessful())

    WinClose($CmdPromptTitleAndExeName)
    WinWaitClose($CmdPromptTitleAndExeName)
EndFunc

; Start blobs, change mode to immediate and kill it after running for $AppRunTime
StartBlobs()
Sleep($AppRunTime)
CloseBlobs()

; Delay for a second between 2 apps
Sleep(1000)

; Start X29 and let it run for $AppRunTime
StartX29()
Sleep($AppRunTime)

; Start FSDOS
DoFSDOS()

; Close X29
CloseX29()
