; SPDX-License-Identifier: MPL-2.0

#NoTrayIcon
;
; sqz-remote.au3 — send one transport verb to a running KvitSqueeze.
;
;   sqz-remote.au3 next          also: previous, playpause, stop, activate
;   sqz-remote.au3               with no argument, "next"
;
; Written for a keyboard with no media keys (a Microsoft Sculpt): remap a key
; to run this, and the key skips a track. `kvitsqueeze.exe --next` does the same
; thing, but pays a few hundred milliseconds of Qt start-up per press; this
; writes to the app's single-instance named pipe directly and is immediate.
;
; That pipe is the app's only listening endpoint and it is not a socket, which
; is what keeps prd.md N7 intact. It accepts the five verbs above and ignores
; anything else — see prd.md FR-7.10.

Global Const $GENERIC_WRITE  = 0x40000000
Global Const $OPEN_EXISTING  = 3
Global Const $INVALID_HANDLE = Ptr(-1)

; Flip to True while debugging. Off by default on purpose: this runs from a
; keypress, and a dialog in front of whatever you were doing is worse than a
; key that quietly did nothing.
Global Const $SQZ_DEBUG = False

Func SqzSend($cmd)
    ; Matches instanceName() in src/qml/singleinstance.cpp — one pipe per user,
    ; so fast user switching gives each session its own player.
    Local $path = "\\.\pipe\KvitSqueeze-instance-" & StringLower(@UserName)

    Local $h = DllCall("kernel32.dll", "ptr", "CreateFileW", _
        "wstr",  $path, _
        "dword", $GENERIC_WRITE, _
        "dword", 0, _
        "ptr",   0, _
        "dword", $OPEN_EXISTING, _
        "dword", 0, _
        "ptr",   0)

    If @error Then Return SqzFail("CreateFileW: DllCall failed (@error = " & @error & ")")

    If $h[0] = $INVALID_HANDLE Then
        ; Best-effort: AutoIt makes no promise about preserving the thread's
        ; last-error across DllCall, so treat this as a hint, not a verdict.
        Local $err = DllCall("kernel32.dll", "dword", "GetLastError")
        Return SqzFail("Cannot open " & $path & @CRLF & @CRLF & _
            "GetLastError = " & $err[0] & @CRLF & _
            "2 = KvitSqueeze is not running, 5 = access denied, 231 = pipe busy")
    EndIf

    ; +1 so the string and its terminator both fit: AutoIt truncates a string
    ; that does not. Only the verb itself is written — the app tolerates a
    ; trailing NUL, but there is no reason to send one.
    Local $len = StringLen($cmd)
    Local $buf = DllStructCreate("char[" & $len + 1 & "]")
    DllStructSetData($buf, 1, $cmd)

    Local $w = DllCall("kernel32.dll", "bool", "WriteFile", _
        "handle", $h[0], _
        "struct*", $buf, _
        "dword",  $len, _
        "dword*", 0, _
        "ptr",    0)

    ; Read both of these before the next DllCall: @error belongs to whichever
    ; call was made last, and CloseHandle below is about to become that call.
    ; [0] is WriteFile's return value; [4] is its bytes-written out parameter.
    Local $ok    = (Not @error) And IsArray($w) And $w[0]
    Local $wrote = IsArray($w) ? $w[4] : 0

    ; No FlushFileBuffers: on a named pipe it blocks until the server has read
    ; everything, and closing the handle already leaves the data buffered for it.
    DllCall("kernel32.dll", "bool", "CloseHandle", "handle", $h[0])

    If Not $ok Then Return SqzFail("WriteFile failed")
    If $wrote <> $len Then Return SqzFail("Short write: " & $wrote & " of " & $len & " bytes")

    Return True
EndFunc

Func SqzFail($message)
    If $SQZ_DEBUG Then MsgBox(16, "KvitSqueeze", $message)
    Return SetError(1, 0, False)
EndFunc

Local $cmd = "next"
If $CmdLine[0] > 0 Then $cmd = $CmdLine[1]

Exit SqzSend($cmd) ? 0 : 1
