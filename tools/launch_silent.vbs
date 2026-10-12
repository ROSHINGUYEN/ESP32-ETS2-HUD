Set WshShell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")

' Tim thu muc goc repo dong theo vi tri script
scriptPath = WScript.ScriptFullName
toolsDir = fso.GetParentFolderName(scriptPath)
repoDir = fso.GetParentFolderName(toolsDir)

if not fso.FileExists(repoDir & "\tools\miu_desktop_ets2.py") then
    repoDir = toolsDir
end if

WshShell.CurrentDirectory = repoDir

pythonScript = repoDir & "\tools\miu_desktop_ets2.py"
cmd = "pythonw """ & pythonScript & """"

On Error Resume Next
WshShell.Run cmd, 0, False
if Err.Number <> 0 then
    cmd = "python """ & pythonScript & """"
    WshShell.Run cmd, 0, False
end if
